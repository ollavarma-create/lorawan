// ============================================================
// DrainSensor.ino
// LoRaWAN OTAA + salaojasensori + Ultra Low Power
//
// Payload (2 tavua):
//   Tavu 0: 0x00 = kuiva, 0x01 = vettä havaittu
//   Tavu 1: akku % (0–100)
//
// Downlink-komennot (fPort=1):
//   [0x01, high, low]  → muuta uni-aika sekunteina (16-bit big-endian)
//   [0x02]             → OTA-tila 5 min
//   [0x02, min]        → OTA-tila N minuuttia
// ============================================================

#include <RadioLib.h>
#include <esp_sleep.h>
#include <esp_mac.h>
#include <EEPROM.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "settings.h"
#include "nvs_config.h"
#include "ota_update.h"
#include "battery.h"
#include "water_sensor.h"

// -----------------------------------------------
// Logitus (false = tuotanto, true = debug)
#define DEBUG false

// -----------------------------------------------
// RTC-muisti (säilyy deep sleepin yli)
RTC_DATA_ATTR bool     isJoined     = false;
RTC_DATA_ATTR uint32_t sleepTimeSec = DEFAULT_SLEEP_SEC;
RTC_DATA_ATTR uint8_t  sessionBuf[RADIOLIB_LORAWAN_SESSION_BUF_SIZE] = {0};
RTC_DATA_ATTR bool     rtcOtaMode       = false;
RTC_DATA_ATTR uint8_t  rtcOtaTimeoutMin = 5;

// -----------------------------------------------
// Globaalit oliot
NVSConfig    nvsCfg;
SX1276*      radio = nullptr;
LoRaWANNode* node  = nullptr;

// EEPROM nonces
#define EEPROM_SIZE       (RADIOLIB_LORAWAN_NONCES_BUF_SIZE + 4)
#define NONCES_ADDR        0
#define NONCES_MAGIC_ADDR  RADIOLIB_LORAWAN_NONCES_BUF_SIZE
#define NONCES_MAGIC       0xA5

uint8_t  noncesBuf[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
uint64_t joinEUI = DEFAULT_JOIN_EUI;
uint8_t  appKey[16] = DEFAULT_APP_KEY;
uint8_t  nwkKey[16];
uint64_t devEUI = 0;

// -----------------------------------------------
static void dbg(const char* msg) {
  if (DEBUG) Serial.println(msg);
}

static void buildDevEUI() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  devEUI = ((uint64_t)mac[0] << 56) | ((uint64_t)mac[1] << 48) |
           ((uint64_t)mac[2] << 40) | (0xFFULL << 32)           |
           (0xFEULL << 24)           | ((uint64_t)mac[3] << 16)  |
           ((uint64_t)mac[4] << 8)   |  (uint64_t)mac[5];

  if (DEBUG) {
    Serial.printf("\n=== DrainSensor | FW %s ===\n", FW_VERSION);
    Serial.print("devEUI (rekisteroi Chirpstackiin): ");
    for (int i = 7; i >= 0; i--) {
      uint8_t b = (devEUI >> (i * 8)) & 0xFF;
      if (b < 0x10) Serial.print("0");
      Serial.print(b, HEX);
      if (i > 0) Serial.print(":");
    }
    Serial.println();
  }
}

static void goToSleep(uint32_t sec) {
  if (radio) radio->sleep();
  SPI.end();
  waterSensorEnd();
  batteryEnd();
  nvsCfg.end();
  gpio_deep_sleep_hold_dis();
  if (DEBUG) { Serial.printf("[sleep] %u s\n", sec); Serial.flush(); }
  esp_sleep_enable_timer_wakeup((uint64_t)sec * 1000000ULL);
  esp_deep_sleep_start();
}

static bool setupRadio(const PinConfig& pins) {
  SPI.begin(pins.sck, pins.miso, pins.mosi, RFM_CS_PIN);

  radio = new SX1276(new Module(RFM_CS_PIN, RFM_DIO0_PIN, RFM_RST_PIN, RFM_DIO1_PIN));
  node  = new LoRaWANNode(radio, &EU868);

  int16_t state = radio->begin();
  if (state != RADIOLIB_ERR_NONE) {
    if (DEBUG) Serial.printf("[radio] Virhe: %d\n", state);
    return false;
  }

  nvsCfg.getAppKey(appKey);
  joinEUI = nvsCfg.getJoinEUI();
  memcpy(nwkKey, appKey, 16);

  node->setADR(false);
  node->setDatarate(5);  // DR5 = SF7

  state = node->beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  if (state != RADIOLIB_ERR_NONE) {
    if (DEBUG) Serial.printf("[radio] beginOTAA virhe: %d\n", state);
    return false;
  }
  return true;
}

static bool performActivation() {
  const int MAX_ATTEMPTS = 5;
  int16_t state;

  for (int i = 1; i <= MAX_ATTEMPTS; i++) {
    if (DEBUG) Serial.printf("[join] Yritys %d/%d\n", i, MAX_ATTEMPTS);
    state = node->activateOTAA();

    if (state == RADIOLIB_LORAWAN_SESSION_RESTORED ||
        state == RADIOLIB_LORAWAN_NEW_SESSION) {
      dbg("[join] OK");
      if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
        memcpy(noncesBuf, node->getBufferNonces(), sizeof(noncesBuf));
        EEPROM.put(NONCES_ADDR, noncesBuf);
        EEPROM.write(NONCES_MAGIC_ADDR, NONCES_MAGIC);
        EEPROM.commit();
      }
      return true;
    }
    if (DEBUG) Serial.printf("[join] Epäonnistui: %d\n", state);
    delay(15000 + i * 5000);
  }
  return false;
}

static void handleDownlink(uint8_t* data, size_t len) {
  if (len < 1) return;
  switch (data[0]) {
    case 0x01:
      if (len >= 3) {
        uint32_t s = ((uint32_t)data[1] << 8) | data[2];
        if (s >= 60 && s <= 86400) {
          sleepTimeSec = s;
          nvsCfg.setSleepSec(s);
          if (DEBUG) Serial.printf("[dl] Uni-aika: %u s\n", s);
        }
      }
      break;
    case 0x02: {
      uint8_t m = (len >= 2 && data[1] > 0) ? data[1] : 5;
      rtcOtaMode = true;
      rtcOtaTimeoutMin = m;
      if (DEBUG) Serial.printf("[dl] OTA: %u min\n", m);
      break;
    }
    default:
      if (DEBUG) Serial.printf("[dl] Tuntematon: 0x%02X\n", data[0]);
  }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  if (DEBUG) {
    Serial.begin(115200);
    while (!Serial) delay(10);
    delay(500);
  }

  buildDevEUI();

  // ── 1. NVS ──────────────────────────────────────────────
  nvsCfg.begin();
  sleepTimeSec = nvsCfg.getSleepSec();

  // ── 2. OTA-tilan tarkistus ──────────────────────────────
  if (rtcOtaMode) {
    rtcOtaMode = false;
    enterOtaMode((uint16_t)rtcOtaTimeoutMin * 60);
  }

  // ── 3. Kiinteät pinnit (WROOM-32D) ──────────────────────
  PinConfig pins = getDefaultPinConfig();

  // ── 4. Akkumittaus ──────────────────────────────────────
  batteryBegin();
  float   vbatt = batteryVoltage();
  uint8_t batt  = batteryPercent(vbatt);
  if (DEBUG) Serial.printf("[batt] %.2fV = %u%%\n", vbatt, batt);

  // ── 5. Vesimittaus ──────────────────────────────────────
  waterSensorBegin();
  bool wet = waterDetected();
  if (DEBUG) Serial.printf("[water] %s\n", wet ? "VETTÄ HAVAITTU" : "Kuiva");

  // ── 6. Radio + LoRaWAN ───────────────────────────────────
  if (!setupRadio(pins)) {
    dbg("[radio] Virhe → nukutaan 5 min");
    goToSleep(300);
  }

  // ── 7. EEPROM nonces ────────────────────────────────────
  EEPROM.begin(EEPROM_SIZE);
  bool eepromValid = (EEPROM.read(NONCES_MAGIC_ADDR) == NONCES_MAGIC);
  if (eepromValid) {
    EEPROM.get(NONCES_ADDR, noncesBuf);
    if (node->setBufferNonces(noncesBuf) != RADIOLIB_ERR_NONE) {
      if (DEBUG) Serial.println("[join] EEPROM nonces korruptoitunut");
      EEPROM.write(NONCES_MAGIC_ADDR, 0x00);
      EEPROM.commit();
    }
  }

  // ── 8. Sessio RTC:stä tai uusi join ─────────────────────
  if (isJoined) {
    if (node->setBufferSession(sessionBuf) != RADIOLIB_ERR_NONE) {
      dbg("[join] Sessio kelpaamaton → uusi join");
      isJoined = false;
    }
  }

  if (!isJoined) {
    if (!performActivation()) {
      dbg("[join] Aktivointi epäonnistui → nukutaan 5 min");
      goToSleep(300);
    }
  }

  // ── 9. Lähetä payload [water, batt] ─────────────────────
  uint8_t payload[2] = { wet ? 0x01u : 0x00u, batt };
  if (DEBUG) Serial.printf("[payload] %02X %02X\n", payload[0], payload[1]);

  uint8_t downlink[64];
  size_t  downlinkLen = 0;
  int16_t state;
  int     retries = 0;

  do {
    retries++;
    downlinkLen = 0;
    state = node->sendReceive(payload, sizeof(payload), 1, downlink, &downlinkLen, true);

    if (downlinkLen > 0) {
      if (DEBUG) {
        Serial.printf("[dl] %u B: ", downlinkLen);
        for (size_t i = 0; i < downlinkLen; i++) Serial.printf("%02X ", downlink[i]);
        Serial.println();
      }
      handleDownlink(downlink, downlinkLen);
    }

    if (state >= 0) {
      if (DEBUG) Serial.printf("[uplink] OK | RSSI=%d dBm\n", radio->getRSSI());
      memcpy(sessionBuf, node->getBufferSession(), sizeof(sessionBuf));
      isJoined = true;
      break;
    } else {
      if (DEBUG) Serial.printf("[uplink] Virhe (yritys %d): %d\n", retries, state);
      delay(10000 + retries * 5000);
    }
  } while (retries < 3 && state < 0);

  // ── 10. Deep sleep ───────────────────────────────────────
  goToSleep(sleepTimeSec);
}

void loop() {}
