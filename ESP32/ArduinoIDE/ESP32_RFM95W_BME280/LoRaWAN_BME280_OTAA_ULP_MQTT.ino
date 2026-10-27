#include <RadioLib.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>  // Lisätty JSON-payloadia varten
#include <esp_sleep.h>    // Lisätty deep sleepiä varten
#include <EEPROM.h>       // Lisätty EEPROM sessio-tallennukseen

// ───────────────────────────────────────────────
// Logitus ON/OFF – Muuta falseksi tuotannossa virran säästämiseksi
#define DEBUG false

// Deep sleep asetukset
RTC_DATA_ATTR uint32_t sleepTime = 600 * 1000000ULL;  // Default 10 min (µs), tallennetaan RTC-muistiin
// Kommentoi alla oleva rivi ja poista kommentit ylläolevasta jos tuotantokäytössä!
// RTC_DATA_ATTR uint32_t sleepTime = 60 * 1000000ULL;  // Testi 1 min (µs), tallennetaan RTC-muistiin
RTC_DATA_ATTR bool isJoined = false;  // Säilytä join-tila sleepin yli
RTC_DATA_ATTR uint8_t sessionBuf[RADIOLIB_LORAWAN_SESSION_BUF_SIZE] = {0};  // Sessio RTC-muistissa

// OTAA-avaimet – ChirpStack-arvoilla (sama kuin ennen)
uint64_t joinEUI = 0x;  // Chrirpstack Device EUI64
uint64_t devEUI  = 0x;  // Chirpstack Join EUI64

// Chirpstack Application key

uint8_t appKey[16] = {
  0x00, 0x00, 0x06, 0x42, 0x13, 0x92, 0x64, 0x56,
  0x1e, 0x00, 0x8f, 0xe3, 0x73, 0xd4, 0x92, 0xf9
};

uint8_t nwkKey[16];

// ───────────────────────────────────────────────
// RFM95W pinnit 
#define CS_PIN    5
#define DIO0_PIN  4
#define RST_PIN   -1
#define DIO1_PIN  -1

// Huomaa alla olevat pinnierot ESP32DevBoardM ja ESP32 SuperMini välillä!

// SPI-pinnit RFM95W + ESP32DevBoard
#define SCK_PIN   6
//#define MOSI_PIN  2
//#define MISO_PIN  9

// SPI-pinnit RFM95W + ESP32SuperMini
#define MOSI_PIN  3  // Muutettu 2:sta 3:een (vältetään GPIO2 strapping)
#define MISO_PIN  7  // Muutettu 9:stä 7:ään (vältetään GPIO9 BOOT/strapping)

// ───────────────────────────────────────────────
// Radio ja LoRaWAN-node
SX1276 radio = new Module(CS_PIN, DIO0_PIN, RST_PIN, DIO1_PIN);
LoRaWANNode node(&radio, &EU868);

// BME280
Adafruit_BME280 bme;

// EEPROM-koko nonces-tallennukseen (vain nonces, ~14 tavua)
#define NONCES_ADDR 0
#define EEPROM_SIZE 64  

// Nonces-buffer
uint8_t noncesBuf[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];

// Funktiot modulaarisuudelle
void debugPrint(const char* msg) {
  if (DEBUG) Serial.println(msg);
}

void setupRadio() {
  debugPrint("Alustetaan RFM95...");
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
  SPI.setFrequency(1000000);
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);

  int16_t state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    debugPrint("Radio alustus epäonnistui!");
    while (true) delay(1000);
  }
  debugPrint("Radio OK");

  radio.setFrequency(868.3);  // Pakotettu taajuus
  node.setDatarate(5);        // DR5 (SF7)
  memcpy(nwkKey, appKey, 16);
  state = node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  if (state != RADIOLIB_ERR_NONE) {
    debugPrint("beginOTAA epäonnistui!");
    while (true) delay(1000);
  }
  node.setADR(true);
}

void setupBME() {
  debugPrint("Alustetaan BME280...");
  Wire.begin(0, 1);  // SDA=0, SCL=1
  if (!bme.begin(0x76) && !bme.begin(0x77)) {
    debugPrint("BME280 EI LÖYTYNYT!");
  }
}

bool performActivation() {
  debugPrint("Aloitetaan OTAA activation (restore or join)...");
  int attempts = 0;
  const int MAX_ATTEMPTS = 5;
  int16_t activationState = RADIOLIB_ERR_NETWORK_NOT_JOINED;

  while (attempts < MAX_ATTEMPTS) {
    attempts++;
    if (DEBUG) Serial.printf("Yritys %d / %d\n", attempts, MAX_ATTEMPTS);

    radio.setFrequency(868.3);
    activationState = node.activateOTAA();
    if (activationState == RADIOLIB_LORAWAN_SESSION_RESTORED || activationState == RADIOLIB_LORAWAN_NEW_SESSION) {
      debugPrint("Activation ONNISTUI!");
      if (activationState == RADIOLIB_LORAWAN_NEW_SESSION) {
        memcpy(noncesBuf, node.getBufferNonces(), sizeof(noncesBuf));
        EEPROM.put(NONCES_ADDR, noncesBuf);
        EEPROM.commit();
        debugPrint("Uudet nonces tallennettu EEPROM:iin");
      }
      return true;
    }
    if (DEBUG) {
      Serial.print("Activation epäonnistui, koodi: ");
      Serial.println(activationState);
    }
    delay(15000 + attempts * 5000);
  }
  return false;
}

void handleDownlink(uint8_t* downlink, size_t len) {
  if (len >= 3 && downlink[0] == 0x01) {  // Komento: 0x01 + 2 tavua (sekunnit)
    uint32_t newTime = (downlink[1] << 8) | downlink[2];
    newTime *= 1000000ULL;  // Muuta sekunneiksi µs:ksi
    if (newTime > 60000000ULL && newTime < 86400000000ULL) {  // 1min - 24h rajoitus
      sleepTime = newTime;
      debugPrint("Uusi sleep-aika asetettu downlinkista!");
    }
  }
  // Lisää muita komentoja tarvittaessa (esim. 0x02 reset)
}

void setup() {
  if (DEBUG) {
    Serial.begin(115200);
    while (!Serial) delay(10);
    delay(2000);
    debugPrint("\n=== ESP32-C3 + RFM95W + BME280 + LoRaWAN OTAA (Päivitetty) ===\n");
  }

  setupRadio();
  setupBME();

  EEPROM.begin(EEPROM_SIZE);  // Alusta EEPROM

  // Palauta nonces ja sessio jos isJoined
  if (isJoined) {
    debugPrint("Palautetaan nonces ja sessio...");
    // Lue nonces EEPROM:sta
    EEPROM.get(NONCES_ADDR, noncesBuf);
    int16_t state = node.setBufferNonces(noncesBuf);
    if (state != RADIOLIB_ERR_NONE) {
      debugPrint("setBufferNonces epäonnistui → reset");
      isJoined = false;
    } else {
      // Palauta sessio RTC:sta
      state = node.setBufferSession(sessionBuf);
      if (state != RADIOLIB_ERR_NONE) {
        debugPrint("setBufferSession epäonnistui → reset");
        isJoined = false;
      } else {
        debugPrint("Nonces ja sessio palautettu");
        if (DEBUG) Serial.printf("Aktiivinen: %s, FCntUp: %lu\n", node.isActivated() ? "kyllä" : "ei", node.getFCntUp());
      }
    }
  }

  // Suorita activation (restore tai join)
  if (!performActivation()) {
    debugPrint("Activation epäonnistui → deep sleep retryyn");
    esp_sleep_enable_timer_wakeup(300 * 1000000ULL);  // Retry 5 min päästä
    esp_deep_sleep_start();
  } else {
    isJoined = true;
  }

  // Lue sensorit ja lähetä (vain kerran per herääminen)
  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  float pres = bme.readPressure() / 100.0F;

  if (isnan(temp) || isnan(hum) || isnan(pres)) {
    debugPrint("BME280 lukuarvo virheellinen!");
  } else {
    // JSON-payload (MQTT-yhteensopiva)
    StaticJsonDocument<128> doc;
    doc["temp"] = temp;
    doc["hum"] = hum;
    doc["pres"] = pres;

    char payload[128];
    serializeJson(doc, payload);

    debugPrint("Lähetetään JSON: ");
    if (DEBUG) Serial.println(payload);

    uint8_t downlink[64];
    size_t downlinkLen = 0;

    // Confirmed uplink retry-mekanismilla
    int16_t state;
    int retryAttempts = 0;
    const int MAX_UPLINK_RETRIES = 3;

    do {
      retryAttempts++;
      state = node.sendReceive(
        (uint8_t*)payload,
        strlen(payload),
        1,
        downlink,
        &downlinkLen,
        true
      );

      if (state >= 0) {
        debugPrint("Uplink onnistui");
        if (DEBUG) {
          Serial.print("Signaali: RSSI=");
          Serial.print(radio.getRSSI());
          Serial.print(" dBm, SNR=");
          Serial.print(radio.getSNR());
          Serial.println(" dB");
        }
        if (downlinkLen > 0) {
          debugPrint("Downlink vastaanotettu (ikkuna: ");
          if (DEBUG) Serial.print(state);
          debugPrint("): ");
          handleDownlink(downlink, downlinkLen);
          if (DEBUG) {
            for (size_t i = 0; i < downlinkLen; i++) {
              Serial.printf("%02X ", downlink[i]);
            }
            Serial.println();
          }
        } else {
          debugPrint("Ei downlinkia");
        }

        // Tallenna päivitetty sessio RTC:iin uplinkin jälkeen (päivitetyt counterit)
        memcpy(sessionBuf, node.getBufferSession(), sizeof(sessionBuf));
        debugPrint("Sessio tallennettu RTC:iin");

        break;
      } else {
        if (DEBUG) {
          Serial.print("Uplink epäonnistui (yritys ");
          Serial.print(retryAttempts);
          Serial.print("), koodi: ");
          Serial.println(state);
        }
        delay(10000 + retryAttempts * 5000);
      }
    } while (retryAttempts < MAX_UPLINK_RETRIES && state < 0);

    if (state < 0) {
      debugPrint("Kaikki uplink-yritykset epäonnistuivat → sleep");
    }
  }

  // Sleep-koodi
  radio.standby();  // Aseta radio standby virran säästämiseksi
  debugPrint("Siirrytään deep sleepiin...");
  esp_sleep_enable_timer_wakeup(sleepTime);
  esp_deep_sleep_start();
}

void loop() {
  // Ei käytetä – kaikki logiikka setupissa deep sleepin takia
}
