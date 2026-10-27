// ============================================================
// AutoDevID-ESP32_RFM95W_BME280.ino
// LoRaWAN OTAA + BME280 + Ultra Low Power + OTA-päivitys
//
// *** VAATIMUS: RadioLib >= 7.3.0 ***
// Tämä koodi on kirjoitettu RadioLib 7.3+ API:lle.
// Vanhemmat versiot (< 7.2) eivät ole yhteensopivia, koska
// nonces- ja session-puskurien rakenne on muuttunut.
// Tarkista versio: Arduino IDE → Library Manager → RadioLib
//
// v1.3.0 muutokset:
//  - KORJAUS: EEPROM korvattu ESP32 Preferences -kirjastolla (NVS).
//    Preferences tallentaa nonces avain-arvo -parilla, jolloin
//    puskurikoon muutos RadioLib-päivityksissä ei aiheuta
//    offset-virheitä eikä magic-tavun menetystä.
//  - KORJAUS: Puskurikoko tallennetaan nonces-datan rinnalle.
//    Jos RADIOLIB_LORAWAN_NONCES_BUF_SIZE muuttuu päivityksessä,
//    vanha data hylätään automaattisesti ja tehdään puhdas join.
//  - KORJAUS: SESSION_BUF_SIZE tallennetaan RTC-muistiin
//    samalla periaatteella – koon muutos havaitaan.
//  - LISÄYS: Käynnistyksen debug-tulosteet näyttävät RadioLib-
//    version, puskurikoot ja nonces-tilan selkeästi.
//  - LISÄYS: Nonces tallennetaan MYÖS onnistuneen uplinkin jälkeen
//    (frame counter kasvaa), ei pelkästään joinin yhteydessä.
//
// v1.2.4 muutokset:
//  - KORJAUS: Downlinkin käsittely irrotettu uplinkin ACK-statuksesta.
//
// v1.2.3 muutokset:
//  - KORJAUS: setADR(false) ja setDatarate(5) ennen beginOTAA()
//
// Downlink-komennot (fPort=1):
//  [0x01, high, low]        → muuta uni-aika (sekuntia, big-endian)
//  [0x02]                   → OTA-tila 5 min (ArduinoOTA)
//  [0x02, min]              → OTA-tila N minuuttia
//  [0x03]                   → Tyhjennä NVS-pin-välimuisti
//
// Payload (4 tavua):
//  Tavu 0: lämpötila + 40  (0–125  → -40...+85 °C)
//  Tavu 1: kosteus          (0–100 %)
//  Tavu 2: paine – 900      (0–160 → 900...1060 hPa)
//  Tavu 3: akku %           (0–100 %)
// ============================================================

#include <RadioLib.h>

// ── Versiotarkistus ────────────────────────────────────────
// RadioLib 7.3.0 muutti nonces-puskurin rakenteen lopulliseen
// muotoonsa. Vanhemmat versiot eivät ole yhteensopivia.
#if !defined(RADIOLIB_VERSION)
  #warning "RadioLib-versiota ei voitu tarkistaa. Varmista >= 7.3.0"
#elif (RADIOLIB_VERSION < 0x07030000)
  #error "RadioLib >= 7.3.0 vaaditaan! Päivitä Arduino IDE:n Library Managerista."
#endif

#include <Wire.h>
#include <Adafruit_BME280.h>
#include <esp_sleep.h>
#include <esp_mac.h>
#include <Preferences.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "settings.h"
#include "nvs_config.h"
#include "pin_probe.h"
#include "ota_update.h"
#include "battery.h"

// -----------------------------------------------
// Logitus (false = tuotanto, true = debug)
#define DEBUG true  // ← Aseta false tuotannossa

// -----------------------------------------------
// RTC-muisti (säilyy deep sleepin yli, nollautuu virranpoistossa)
RTC_DATA_ATTR bool     isJoined     = false;
RTC_DATA_ATTR uint32_t sleepTimeSec = DEFAULT_SLEEP_SEC;
RTC_DATA_ATTR uint8_t  sessionBuf[RADIOLIB_LORAWAN_SESSION_BUF_SIZE] = {0};
RTC_DATA_ATTR uint16_t rtcSessionBufSize = 0;  // Tallenna koko version muutoksen havaitsemiseksi

// OTA-tila
RTC_DATA_ATTR bool    rtcOtaMode       = false;
RTC_DATA_ATTR uint8_t rtcOtaTimeoutMin = 5;

// -----------------------------------------------
// Globaalit oliot
NVSConfig    nvsCfg;
Preferences  prefs;      // Korvaa EEPROM:n → ei kiinteitä osoitteita
SX1276*      radio = nullptr;
LoRaWANNode* node  = nullptr;
Adafruit_BME280 bme;

// Nonces-puskuri (koko määräytyy RadioLib-versiosta)
uint8_t noncesBuf[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];

// LoRaWAN avaimet
uint64_t joinEUI = DEFAULT_JOIN_EUI;
uint8_t  appKey[16] = DEFAULT_APP_KEY;
uint8_t  nwkKey[16];
uint64_t devEUI = 0;

// -----------------------------------------------
// Debug-tulostus
static void dbg(const char* msg) {
  if (DEBUG) Serial.println(msg);
}

// -----------------------------------------------
// Tulosta RadioLib-versio ja puskurikoot käynnistyksen yhteydessä
static void printVersionInfo() {
  if (!DEBUG) return;

  Serial.println(F("\n╔══════════════════════════════════════════╗"));
  Serial.println(F("║     LoRaWAN OTAA + OTA – Debug Info      ║"));
  Serial.println(F("╚══════════════════════════════════════════╝"));
  Serial.printf("  FW versio          : %s\n", FW_VERSION);
#if defined(RADIOLIB_VERSION)
  Serial.printf("  RadioLib versio    : 0x%08X\n", RADIOLIB_VERSION);
#else
  Serial.println("  RadioLib versio    : TUNTEMATON");
#endif
  Serial.printf("  NONCES_BUF_SIZE    : %u tavua\n", RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  Serial.printf("  SESSION_BUF_SIZE   : %u tavua\n", RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
  Serial.printf("  isJoined (RTC)     : %s\n", isJoined ? "true" : "false");
  Serial.printf("  rtcSessionBufSize  : %u\n", rtcSessionBufSize);
  Serial.printf("  sleepTimeSec       : %u\n", sleepTimeSec);
  Serial.println();
}

// -----------------------------------------------
// DevEUI MAC-osoitteesta (IEEE EUI-64)
static void buildDevEUI() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  devEUI = ((uint64_t)mac[0] << 56) | ((uint64_t)mac[1] << 48) |
           ((uint64_t)mac[2] << 40) | (0xFFULL << 32)           |
           (0xFEULL << 24)           | ((uint64_t)mac[3] << 16)  |
           ((uint64_t)mac[4] << 8)   |  (uint64_t)mac[5];

  if (DEBUG) {
    Serial.print("  devEUI             : ");
    for (int i = 7; i >= 0; i--) {
      uint8_t b = (devEUI >> (i * 8)) & 0xFF;
      if (b < 0x10) Serial.print("0");
      Serial.print(b, HEX);
      if (i > 0) Serial.print(":");
    }
    Serial.println();
  }
}

// -----------------------------------------------
// Nonces-tallennus NVS:ään (Preferences)
// Tallentaa nonces-datan JA puskurikoon, jotta versiomuutos havaitaan.
static bool saveNonces() {
  prefs.begin("lorawan", false);  // read-write

  bool ok = true;
  ok &= (prefs.putUShort("nonces_size", RADIOLIB_LORAWAN_NONCES_BUF_SIZE) > 0);
  ok &= (prefs.putBytes("nonces", noncesBuf, RADIOLIB_LORAWAN_NONCES_BUF_SIZE) == RADIOLIB_LORAWAN_NONCES_BUF_SIZE);

  prefs.end();

  if (ok) {
    dbg("[nonces] Tallennettu NVS:ään OK");
  } else {
    dbg("[nonces] VAROITUS: NVS-tallennus epäonnistui!");
  }
  return ok;
}

// -----------------------------------------------
// Nonces-lataus NVS:stä
// Palauttaa true jos nonces ladattiin ja asetettiin onnistuneesti.
// Palauttaa false jos NVS on tyhjä, koko ei täsmää tai data on korruptoitunut.
static bool loadNonces() {
  prefs.begin("lorawan", true);  // read-only

  // 1) Tarkista onko dataa tallennettu
  if (!prefs.isKey("nonces") || !prefs.isKey("nonces_size")) {
    prefs.end();
    dbg("[nonces] NVS tyhjä – ensimmäinen käynnistys");
    return false;
  }

  // 2) Tarkista puskurikoko – versio muuttunut?
  uint16_t storedSize = prefs.getUShort("nonces_size", 0);
  if (storedSize != RADIOLIB_LORAWAN_NONCES_BUF_SIZE) {
    prefs.end();
    if (DEBUG) {
      Serial.printf("[nonces] KOKO MUUTTUI: NVS=%u, RadioLib=%u → hylätään vanhat\n",
        storedSize, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
    }
    // Tyhjennä vanhat nonces ettei niitä vahingossa lueta
    clearNonces();
    return false;
  }

  // 3) Lue data
  size_t readLen = prefs.getBytes("nonces", noncesBuf, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  prefs.end();

  if (readLen != RADIOLIB_LORAWAN_NONCES_BUF_SIZE) {
    if (DEBUG) Serial.printf("[nonces] Luku epäonnistui: saatiin %u / %u tavua\n",
      readLen, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
    clearNonces();
    return false;
  }

  // 4) Syötä RadioLibille
  int16_t state = node->setBufferNonces(noncesBuf);
  if (state != RADIOLIB_ERR_NONE) {
    if (DEBUG) Serial.printf("[nonces] setBufferNonces epäonnistui: %d → puhdas join\n", state);
    clearNonces();
    return false;
  }

  dbg("[nonces] Ladattu NVS:stä ja asetettu OK");

  if (DEBUG) {
    Serial.print("[nonces] Data: ");
    for (uint16_t i = 0; i < RADIOLIB_LORAWAN_NONCES_BUF_SIZE; i++) {
      if (noncesBuf[i] < 0x10) Serial.print("0");
      Serial.print(noncesBuf[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }

  return true;
}

// -----------------------------------------------
// Nonces-tyhjennys NVS:stä
static void clearNonces() {
  prefs.begin("lorawan", false);
  prefs.remove("nonces");
  prefs.remove("nonces_size");
  prefs.end();
  dbg("[nonces] NVS-data tyhjennetty");
}

// -----------------------------------------------
// Deep sleep – sammuta kaikki ennen nukkumista
static void goToSleep(uint32_t sec) {
  if (radio) { radio->sleep(); }
  Wire.end();
  SPI.end();
  batteryEnd();
  nvsCfg.end();
  gpio_deep_sleep_hold_dis();
  if (DEBUG) {
    Serial.printf("[sleep] Nukutaan %u sekuntia\n", sec);
    Serial.flush();
  }
  esp_sleep_enable_timer_wakeup((uint64_t)sec * 1000000ULL);
  esp_deep_sleep_start();
}

// -----------------------------------------------
// Radio-alustus
static bool setupRadio(const PinConfig& pins) {
  dbg("[radio] Alustetaan SPI + RFM95W...");
  SPI.begin(pins.sck, pins.miso, pins.mosi, RFM_CS_PIN);
  SPI.setFrequency(1000000);
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);

  radio = new SX1276(new Module(RFM_CS_PIN, RFM_DIO0_PIN, RFM_RST_PIN, RFM_DIO1_PIN));
  node  = new LoRaWANNode(radio, &EU868);

  int16_t state = radio->begin();
  if (state != RADIOLIB_ERR_NONE) {
    if (DEBUG) Serial.printf("[radio] Alustus epäonnistui: %d\n", state);
    return false;
  }
  dbg("[radio] OK");

  nvsCfg.getAppKey(appKey);
  joinEUI = nvsCfg.getJoinEUI();
  memcpy(nwkKey, appKey, 16);

  // ADR ja datarate ENNEN beginOTAA():ta → join-viesti SF7:llä
  node->setADR(false);
  node->setDatarate(5);  // DR5 = SF7BW125

  state = node->beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  if (state != RADIOLIB_ERR_NONE) {
    if (DEBUG) Serial.printf("[radio] beginOTAA epäonnistui: %d\n", state);
    return false;
  }
  dbg("[radio] beginOTAA OK");
  return true;
}

// -----------------------------------------------
// BME280-alustus
static bool setupBME(const PinConfig& pins) {
  dbg("[bme] Alustetaan I2C + BME280...");
  Wire.begin(pins.sda, pins.scl);

  if (!bme.begin(0x76) && !bme.begin(0x77)) {
    dbg("[bme] BME280 EI LÖYDY!");
    return false;
  }

  bme.setSampling(
    Adafruit_BME280::MODE_FORCED,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::FILTER_OFF
  );
  dbg("[bme] OK");
  return true;
}

// -----------------------------------------------
// OTAA aktivointi
static bool performActivation() {
  dbg("[join] Aloitetaan OTAA...");
  const int MAX_ATTEMPTS = 5;
  int16_t state = RADIOLIB_ERR_NETWORK_NOT_JOINED;

  for (int i = 1; i <= MAX_ATTEMPTS; i++) {
    if (DEBUG) Serial.printf("[join] Yritys %d / %d\n", i, MAX_ATTEMPTS);

    state = node->activateOTAA();

    if (state == RADIOLIB_LORAWAN_SESSION_RESTORED ||
        state == RADIOLIB_LORAWAN_NEW_SESSION) {
      dbg("[join] ONNISTUI");

      if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
        // Tallenna uudet nonces NVS:ään
        memcpy(noncesBuf, node->getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
        if (!saveNonces()) {
          dbg("[join] VAROITUS: Nonces-tallennus epäonnistui – seuraava join voi vaatia flushin");
        }

        if (DEBUG) {
          Serial.printf("[join] Uusi sessio – DevNonce tallennettu\n");
        }
      } else {
        dbg("[join] Sessio palautettu nonces-tiedoista");
      }
      return true;
    }

    if (DEBUG) Serial.printf("[join] Epäonnistui, koodi: %d\n", state);
    delay(15000 + i * 5000);
  }
  return false;
}

// -----------------------------------------------
// Downlink-käsittely
static void handleDownlink(uint8_t* data, size_t len) {
  if (len < 1) return;

  switch (data[0]) {

    case 0x01: {
      if (len >= 3) {
        uint32_t newSec = ((uint32_t)data[1] << 8) | data[2];
        if (newSec >= 60 && newSec <= 86400) {
          sleepTimeSec = newSec;
          nvsCfg.setSleepSec(newSec);
          if (DEBUG) Serial.printf("[dl] Uusi uni-aika: %u s\n", newSec);
        }
      }
      break;
    }

    case 0x02: {
      uint8_t timeoutMin = (len >= 2 && data[1] > 0) ? data[1] : 5;
      rtcOtaMode = true;
      rtcOtaTimeoutMin = timeoutMin;
      if (DEBUG) Serial.printf("[dl] OTA-tila asetettu: %u min\n", timeoutMin);
      break;
    }

    case 0x03: {
      nvsCfg.clearPinConfig();
      dbg("[dl] Pin-välimuisti tyhjennetty");
      break;
    }

    default:
      if (DEBUG) Serial.printf("[dl] Tuntematon komento: 0x%02X\n", data[0]);
      break;
  }
}

// ============================================================
// SETUP – kaikki logiikka tässä (loop ei käytössä)
// ============================================================
void setup() {
  // ── 0. Poista brownout-reset ─────────────────────────────
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  if (DEBUG) {
    Serial.begin(115200);
    while (!Serial) delay(10);
    delay(500);
  }

  buildDevEUI();
  printVersionInfo();

  // ── 1. NVS-asetukset auki ───────────────────────────────
  nvsCfg.begin();
  sleepTimeSec = nvsCfg.getSleepSec();

  if (DEBUG) nvsCfg.printAll();

  // ── 2. OTA-tilan tarkistus (edelliseltä heräämiseltä) ───
  if (rtcOtaMode) {
    rtcOtaMode = false;
    dbg("[ota] OTA-tila aktiivinen – haetaan päivitystä");

    uint16_t timeoutSec = (uint16_t)rtcOtaTimeoutMin * 60;
    enterOtaMode(timeoutSec);

    dbg("[ota] Jatketaan normaalia toimintaa");
  }

  // ── 3. Pin-tunnistus ────────────────────────────────────
  PinConfig pins;
#ifdef BOARD_PINS_KNOWN
  pins = getDefaultPinConfig();
  if (!nvsCfg.hasPinConfig()) {
    nvsCfg.setPinConfig(pins);
    dbg("[pins] Kovakoodatut pinnit tallennettu NVS:ään");
  }
  dbg("[pins] Käytetään kovakoodattuja pinnit");
#else
  if (!nvsCfg.getPinConfig(pins)) {
    dbg("[probe] Etsitään pinnit...");
    autoDetectPins(pins, DEBUG);
    nvsCfg.setPinConfig(pins);
  }
#endif
  if (DEBUG) Serial.printf("[pins] SDA=%d SCL=%d SCK=%d MOSI=%d MISO=%d\n",
    pins.sda, pins.scl, pins.sck, pins.mosi, pins.miso);

  // ── 4. Akun mittaus (ennen radiota) ─────────────────────
  batteryBegin();
  float vbatt  = batteryVoltage();
  uint8_t batt = batteryPercent(vbatt);
  if (DEBUG) Serial.printf("[batt] %.2f V = %u %%\n", vbatt, batt);

  // ── 5. Radio + LoRaWAN ───────────────────────────────────
  if (!setupRadio(pins)) {
    dbg("[radio] Alustus epäonnistui → nukutaan 5 min");
    goToSleep(300);
  }

  // ── 6. Nonces-lataus NVS:stä ────────────────────────────
  // Preferences-kirjasto tallentaa nonces avain-arvo -parilla,
  // joten RADIOLIB_LORAWAN_NONCES_BUF_SIZE:n muutos ei aiheuta
  // offset-virheitä. Puskurikoko tarkistetaan erikseen.
  bool noncesLoaded = loadNonces();

  // ── 7. Session-palautus RTC-muistista ───────────────────
  // Tarkista ensin ettei SESSION_BUF_SIZE ole muuttunut
  if (isJoined && rtcSessionBufSize != RADIOLIB_LORAWAN_SESSION_BUF_SIZE) {
    if (DEBUG) {
      Serial.printf("[session] SESSION_BUF_SIZE muuttui: RTC=%u, RadioLib=%u → hylätään\n",
        rtcSessionBufSize, RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
    }
    isJoined = false;
  }

  if (isJoined) {
    dbg("[session] Palautetaan sessio RTC-muistista...");
    int16_t s2 = node->setBufferSession(sessionBuf);
    if (s2 != RADIOLIB_ERR_NONE) {
      if (DEBUG) Serial.printf("[session] Kelpaamaton (%d) → joinataan uudelleen\n", s2);
      isJoined = false;
    } else {
      dbg("[session] Palautettu OK – ohitetaan OTAA");
    }
  }

  // ── 8. Aktivoi OTAA vain jos tarpeen ────────────────────
  if (!isJoined) {
    if (!noncesLoaded) {
      dbg("[join] Ei nonces-dataa → puhdas join (varmista Chirpstack flush)");
    }

    if (!performActivation()) {
      dbg("[join] Aktivointi epäonnistui → nukutaan 5 min");
      goToSleep(300);
    }
  }

  // ── 9. BME280 ────────────────────────────────────────────
  if (!setupBME(pins)) {
    dbg("[bme] Virhe – lähetetään payload ilman sensoriarvoja");
  }

  bme.takeForcedMeasurement();
  float temp = bme.readTemperature();
  float hum  = bme.readHumidity();
  float pres = bme.readPressure() / 100.0F;

  if (DEBUG) Serial.printf("[bme] %.1f°C  %.0f%%  %.0f hPa\n", temp, hum, pres);

  // ── 10. Lähetä payload (4 tavua) ─────────────────────────
  uint8_t payload[4];
  payload[0] = (uint8_t)constrain((int)temp + 40,  0, 125);
  payload[1] = (uint8_t)constrain((int)hum,         0, 100);
  payload[2] = (uint8_t)constrain((int)pres - 900,  0, 160);
  payload[3] = batt;

  if (DEBUG) Serial.printf("[payload] %02X %02X %02X %02X\n",
    payload[0], payload[1], payload[2], payload[3]);

  uint8_t downlink[64];
  size_t downlinkLen = 0;
  int16_t state;
  int retries = 0;

  do {
    retries++;
    downlinkLen = 0;
    state = node->sendReceive(
      payload, sizeof(payload), 1,
      downlink, &downlinkLen, true
    );

    // Käsittele downlink AINA jos dataa saatiin (v1.2.4 korjaus)
    if (downlinkLen > 0) {
      if (DEBUG) {
        Serial.printf("[dl] %u tavua: ", downlinkLen);
        for (size_t i = 0; i < downlinkLen; i++) Serial.printf("%02X ", downlink[i]);
        Serial.println();
      }
      handleDownlink(downlink, downlinkLen);
    }

    if (state >= 0) {
      if (DEBUG) Serial.printf("[uplink] OK | RSSI=%d dBm SNR=%.1f dB\n",
        radio->getRSSI(), radio->getSNR());

      // Tallenna sessio RTC-muistiin + merkitse koko
      memcpy(sessionBuf, node->getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
      rtcSessionBufSize = RADIOLIB_LORAWAN_SESSION_BUF_SIZE;
      isJoined = true;

      // Tallenna nonces myös uplinkin jälkeen – frame counter on kasvanut
      memcpy(noncesBuf, node->getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
      saveNonces();

      break;

    } else {
      if (DEBUG) Serial.printf("[uplink] Epäonnistui (yritys %d), koodi: %d\n", retries, state);
      delay(10000 + retries * 5000);
    }
  } while (retries < 3 && state < 0);

  if (state < 0) dbg("[uplink] Kaikki yritykset epäonnistuivat");

  // ── 11. Deep sleep ───────────────────────────────────────
  goToSleep(sleepTimeSec);
}

// Loop ei käytössä – kaikki logiikka setup():ssa deep sleep -arkkitehtuurin takia
void loop() {}
