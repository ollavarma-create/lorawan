#include <RadioLib.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <esp_sleep.h>
#include <esp_mac.h>
#include <EEPROM.h>

// -----------------------------------------------
// LAITEKOHTAINEN ASETUS - muuta tama ennen flashausta
// Kommentoi toinen rivi
#define BOARD_SUPERMINI
// #define BOARD_DEVBOARD

// -----------------------------------------------
// Logitus ON/OFF - muuta falseksi tuotannossa
#define DEBUG true

// -----------------------------------------------
// Pinnit automaattisesti valinnan mukaan
#ifdef BOARD_SUPERMINI
  #define SCK_PIN   6
  #define MOSI_PIN  3
  #define MISO_PIN  7
  #define BME_SDA   0
  #define BME_SCL   1
#else  // BOARD_DEVBOARD
  #define SCK_PIN   6
  #define MOSI_PIN  2
  #define MISO_PIN  9
  #define BME_SDA   0
  #define BME_SCL   1
#endif

// RFM95W pinnit (samat molemmissa)
#define CS_PIN    5
#define DIO0_PIN  4
#define RST_PIN   -1
#define DIO1_PIN  -1

// -----------------------------------------------
// Deep sleep
RTC_DATA_ATTR uint32_t sleepTime = 600 * 1000000ULL;  // Default 10 min (us)
// RTC_DATA_ATTR uint32_t sleepTime = 60 * 1000000ULL; // Testi 1 min (us)
RTC_DATA_ATTR bool isJoined = false;
RTC_DATA_ATTR uint8_t sessionBuf[RADIOLIB_LORAWAN_SESSION_BUF_SIZE] = {0};

// -----------------------------------------------
// OTAA-avaimet - joinEUI ja appKey sama kaikille laitteille
// devEUI generoidaan automaattisesti MAC-osoitteesta
uint64_t joinEUI = 0xae6ce52af812689b7ULL;
uint64_t devEUI = 0;  // Taytetaan buildDevEUI():ssa

uint8_t appKey[16] = {
  0x6a, 0xae, 0x06, 0x42, 0x13, 0x92, 0x64, 0x56,
  0x1e, 0xf0, 0x8f, 0xe3, 0x73, 0xd4, 0x92, 0xf9
};

uint8_t nwkKey[16];

// -----------------------------------------------
// Radio ja LoRaWAN
SX1276 radio = new Module(CS_PIN, DIO0_PIN, RST_PIN, DIO1_PIN);
LoRaWANNode node(&radio, &EU868);

// BME280
Adafruit_BME280 bme;

// EEPROM
#define NONCES_ADDR 0
#define EEPROM_SIZE 64
uint8_t noncesBuf[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];

// -----------------------------------------------
void debugPrint(const char* msg) {
  if (DEBUG) Serial.println(msg);
}

// Rakenna devEUI automaattisesti ESP32:n uniikista MAC-osoitteesta (IEEE EUI-64)
// Tallennetaan suoraan tavutaulukkoon RadioLibin odottamassa muodossa
void buildDevEUI() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  // IEEE EUI-64: mac[0..2] + FF:FE + mac[3..5]
  // Rakennetaan uint64_t tavujärjestyksessa mac[0]..mac[2] + FF:FE + mac[3]..mac[5]
  devEUI = ((uint64_t)mac[0] << 56) | ((uint64_t)mac[1] << 48) |
           ((uint64_t)mac[2] << 40) | ((uint64_t)0xFF   << 32) |
           ((uint64_t)0xFE   << 24) | ((uint64_t)mac[3] << 16) |
           ((uint64_t)mac[4] <<  8) |  (uint64_t)mac[5];

  if (DEBUG) {
    // Tulostetaan pienilla kirjaimilla suoraan Chirpstackiin copy-pastea varten
    Serial.print("devEUI Chirpstackiin >>> ");
    for (int i = 7; i >= 0; i--) {
      char buf[3];
      sprintf(buf, "%02x", (uint8_t)(devEUI >> (i * 8)));
      Serial.print(buf);
    }
    Serial.println(" <<<");

    Serial.print("Board: ");
    #ifdef BOARD_SUPERMINI
      Serial.println("SuperMini");
    #else
      Serial.println("DevBoard");
    #endif
  }
}

void goToSleep(uint64_t duration_us) {
  radio.sleep();
  Wire.end();
  SPI.end();
  gpio_deep_sleep_hold_dis();
  debugPrint("Siirrytaan deep sleepiin...");
  if (DEBUG) Serial.flush();
  esp_sleep_enable_timer_wakeup(duration_us);
  esp_deep_sleep_start();
}

void setupRadio() {
  debugPrint("Alustetaan RFM95...");
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
  SPI.setFrequency(1000000);
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);

  int16_t state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    debugPrint("Radio alustus epaonnistui!");
    goToSleep(300 * 1000000ULL);
  }
  debugPrint("Radio OK");

  radio.setFrequency(868.3);
  node.setADR(false);           // ADR pois - pakotetaan DR5 (SF7)
  node.setDatarate(5);          // DR5 = SF7, nopein ja vahan virtaa
  memcpy(nwkKey, appKey, 16);
  state = node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  if (state != RADIOLIB_ERR_NONE) {
    debugPrint("beginOTAA epaonnistui!");
    goToSleep(300 * 1000000ULL);
  }
}

void setupBME() {
  debugPrint("Alustetaan BME280...");
  Wire.begin(BME_SDA, BME_SCL);
  if (!bme.begin(0x76) && !bme.begin(0x77)) {
    debugPrint("BME280 EI LOYDY!");
  }
  // Forced mode: yksi mittaus -> automaattisesti sleep-tilaan
  bme.setSampling(
    Adafruit_BME280::MODE_FORCED,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::FILTER_OFF
  );
}

bool performActivation() {
  debugPrint("Aloitetaan OTAA activation...");
  int attempts = 0;
  const int MAX_ATTEMPTS = 5;
  int16_t activationState = RADIOLIB_ERR_NETWORK_NOT_JOINED;

  while (attempts < MAX_ATTEMPTS) {
    attempts++;
    if (DEBUG) Serial.printf("Yritys %d / %d\n", attempts, MAX_ATTEMPTS);

    radio.setFrequency(868.3);
    activationState = node.activateOTAA();

    if (activationState == RADIOLIB_LORAWAN_SESSION_RESTORED ||
        activationState == RADIOLIB_LORAWAN_NEW_SESSION) {
      debugPrint("Activation ONNISTUI!");
      if (activationState == RADIOLIB_LORAWAN_NEW_SESSION) {
        memcpy(noncesBuf, node.getBufferNonces(), sizeof(noncesBuf));
        EEPROM.put(NONCES_ADDR, noncesBuf);
        EEPROM.commit();
        debugPrint("Uudet nonces tallennettu EEPROM:iin");
      }
      // Aseta DR uudelleen joinin jalkeen (varmistaa SF7)
      node.setADR(false);
      node.setDatarate(5);
      return true;
    }
    if (DEBUG) {
      Serial.print("Activation epaonnistui, koodi: ");
      Serial.println(activationState);
    }
    delay(15000 + attempts * 5000);
  }
  return false;
}

void handleDownlink(uint8_t* downlink, size_t len) {
  if (len >= 3 && downlink[0] == 0x01) {
    uint32_t newTime = (downlink[1] << 8) | downlink[2];
    newTime *= 1000000ULL;
    if (newTime > 60000000ULL && newTime < 86400000000ULL) {
      sleepTime = newTime;
      debugPrint("Uusi sleep-aika asetettu downlinkista!");
    }
  }
}

void setup() {
  if (DEBUG) {
    Serial.begin(115200);
    #ifdef BOARD_SUPERMINI
      // ESP32-C3 SuperMini kayttaa USB-CDC:ta - odota yhteytta pitempaan
      delay(3000);
    #else
      while (!Serial) delay(10);
      delay(2000);
    #endif
    debugPrint("\n=== ESP32 + RFM95W + BME280 + LoRaWAN OTAA (ULP) ===\n");
  }

  buildDevEUI();  // Generoi devEUI MAC:sta
  setupRadio();
  setupBME();
  EEPROM.begin(EEPROM_SIZE);

  // Palauta nonces ja sessio jos isJoined
  if (isJoined) {
    debugPrint("Palautetaan nonces ja sessio...");
    EEPROM.get(NONCES_ADDR, noncesBuf);
    int16_t state = node.setBufferNonces(noncesBuf);
    if (state != RADIOLIB_ERR_NONE) {
      debugPrint("setBufferNonces epaonnistui -> reset");
      isJoined = false;
    } else {
      state = node.setBufferSession(sessionBuf);
      if (state != RADIOLIB_ERR_NONE) {
        debugPrint("setBufferSession epaonnistui -> reset");
        isJoined = false;
      } else {
        debugPrint("Nonces ja sessio palautettu");
        if (DEBUG) Serial.printf("Aktiivinen: %s, FCntUp: %lu\n",
          node.isActivated() ? "kylla" : "ei", node.getFCntUp());
      }
    }
  }

  if (!performActivation()) {
    debugPrint("Activation epaonnistui -> retry");
    goToSleep(300 * 1000000ULL);
  } else {
    isJoined = true;
  }

  // Forced mode: kaynnista mittaus ja odota (~10 ms)
  bme.takeForcedMeasurement();

  float temp = bme.readTemperature();
  float hum  = bme.readHumidity();
  float pres = bme.readPressure() / 100.0F;

  if (isnan(temp) || isnan(hum) || isnan(pres)) {
    debugPrint("BME280 lukuarvo virheellinen!");
  } else {
    // 3-tavu binaaripayload:
    //   Tavu 0: lampotila + 40  (0-125 -> -40C...+85C)
    //   Tavu 1: kosteus          (0-100 %)
    //   Tavu 2: paine - 900      (0-160 -> 900-1060 hPa)
    uint8_t payload[3];
    payload[0] = (uint8_t)constrain((int)temp + 40, 0, 125);
    payload[1] = (uint8_t)constrain((int)hum, 0, 100);
    payload[2] = (uint8_t)constrain((int)pres - 900, 0, 160);

    if (DEBUG) {
      Serial.printf("Payload (3 tavua): temp=%d hum=%d pres=%d\n",
        payload[0] - 40, payload[1], payload[2] + 900);
    }

    uint8_t downlink[64];
    size_t downlinkLen = 0;
    int16_t state;
    int retryAttempts = 0;
    const int MAX_UPLINK_RETRIES = 3;

    do {
      retryAttempts++;
      state = node.sendReceive(
        payload, sizeof(payload), 1,
        downlink, &downlinkLen, true
      );

      if (state >= 0) {
        debugPrint("Uplink onnistui");
        if (DEBUG) {
          Serial.printf("RSSI=%d dBm, SNR=%.1f dB\n",
            radio.getRSSI(), radio.getSNR());
        }
        if (downlinkLen > 0) {
          handleDownlink(downlink, downlinkLen);
          if (DEBUG) {
            debugPrint("Downlink:");
            for (size_t i = 0; i < downlinkLen; i++)
              Serial.printf("%02X ", downlink[i]);
            Serial.println();
          }
        }
        memcpy(sessionBuf, node.getBufferSession(), sizeof(sessionBuf));
        debugPrint("Sessio tallennettu RTC:iin");
        break;
      } else {
        if (DEBUG) Serial.printf("Uplink epaonnistui (yritys %d), koodi: %d\n",
          retryAttempts, state);
        delay(10000 + retryAttempts * 5000);
      }
    } while (retryAttempts < MAX_UPLINK_RETRIES && state < 0);

    if (state < 0) debugPrint("Kaikki uplink-yritykset epaonnistuivat -> sleep");
  }

  goToSleep(sleepTime);
}

void loop() {
  // Ei kayteta - kaikki logiikka setupissa deep sleepin takia
}
