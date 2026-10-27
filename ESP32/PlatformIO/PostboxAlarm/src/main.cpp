// ============================================================
// Postihaly – LoRaWAN Mailbox Alert
// ESP32-C3 SuperMini + RFM95W-868S2 + Microswitch
//
// *** REQUIRES: RadioLib >= 7.3.0 ***
//
// Wake sources:
//   GPIO: GPIO2 HIGH → mailbox lid opened (microswitch released)
//   Timer: periodic heartbeat for battery status reporting
//
// Payload (2 bytes, fPort=1):
//   Byte 0: event type (0x01 = mail, 0x02 = heartbeat)
//   Byte 1: battery %  (0–100)
//
// Downlink commands (fPort=1):
//   [0x01, high, low]  → change heartbeat interval (seconds, big-endian)
//   [0x02]             → enter OTA mode (5 min timeout)
//   [0x02, min]        → enter OTA mode (N minutes)
//
// ChirpStack decoder: see chirpstack_decoder.js
// ============================================================

#include <RadioLib.h>

// ── Version check ─────────────────────────────────────────
#if !defined(RADIOLIB_VERSION)
  #warning "RadioLib version could not be verified. Ensure >= 7.3.0"
#elif (RADIOLIB_VERSION < 0x07030000)
  #error "RadioLib >= 7.3.0 required! Update via Arduino IDE Library Manager."
#endif

#include <SPI.h>
#include <esp_sleep.h>
#include <esp_mac.h>
#include <Preferences.h>

// ESP32-C3 specific: brownout register doesn't exist
#ifndef CONFIG_IDF_TARGET_ESP32C3
  #include "soc/soc.h"
  #include "soc/rtc_cntl_reg.h"
#endif

#include "settings.h"
#include "nvs_config.h"
#include "ota_update.h"
#include "battery.h"

// -----------------------------------------------
// Logging (set false for production)
#define DEBUG false

// -----------------------------------------------
// RTC-retained memory (survives deep sleep)
RTC_DATA_ATTR bool     isJoined        = false;
RTC_DATA_ATTR uint32_t heartbeatSec    = DEFAULT_HEARTBEAT_SEC;
RTC_DATA_ATTR uint8_t  sessionBuf[RADIOLIB_LORAWAN_SESSION_BUF_SIZE] = {0};
RTC_DATA_ATTR uint16_t rtcSessionBufSize = 0;

// OTA mode
RTC_DATA_ATTR bool    rtcOtaMode       = false;
RTC_DATA_ATTR uint8_t rtcOtaTimeoutMin = 5;

// Cooldown flag: if true, we just woke from a mail event cooldown
RTC_DATA_ATTR bool    inCooldown       = false;

// -----------------------------------------------
// Global objects
NVSConfig    nvsCfg;
Preferences  prefs;
SX1276*      radio = nullptr;
LoRaWANNode* node  = nullptr;

// Nonces buffer
uint8_t noncesBuf[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];

// LoRaWAN keys
uint64_t joinEUI = DEFAULT_JOIN_EUI;
uint8_t  appKey[16] = DEFAULT_APP_KEY;
uint8_t  nwkKey[16];
uint64_t devEUI = 0;

// -----------------------------------------------
// Debug output
static void dbg(const char* msg) {
  if (DEBUG) Serial.println(msg);
}

// -----------------------------------------------
// Print version and buffer info at boot
static void printVersionInfo() {
  if (!DEBUG) return;

  Serial.println(F("\n╔══════════════════════════════════════════╗"));
  Serial.println(F("║   Postihaly – LoRaWAN Mailbox Alert      ║"));
  Serial.println(F("╚══════════════════════════════════════════╝"));
  Serial.printf("  Target             : ESP32-C3 SuperMini\n");
  Serial.printf("  FW version         : %s\n", FW_VERSION);
#if defined(RADIOLIB_VERSION)
  Serial.printf("  RadioLib version   : 0x%08X\n", RADIOLIB_VERSION);
#endif
  Serial.printf("  NONCES_BUF_SIZE    : %u bytes\n", RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  Serial.printf("  SESSION_BUF_SIZE   : %u bytes\n", RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
  Serial.printf("  isJoined (RTC)     : %s\n", isJoined ? "true" : "false");
  Serial.printf("  heartbeatSec       : %u\n", heartbeatSec);
  Serial.println();
}

// -----------------------------------------------
// Build DevEUI from MAC address (IEEE EUI-64)
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
// Forward declarations
static void handleDownlink(uint8_t* data, size_t len);
static void clearNonces();

static bool saveNonces() {
  prefs.begin("lorawan", false);
  bool ok = true;
  ok &= (prefs.putUShort("nonces_size", RADIOLIB_LORAWAN_NONCES_BUF_SIZE) > 0);
  ok &= (prefs.putBytes("nonces", noncesBuf, RADIOLIB_LORAWAN_NONCES_BUF_SIZE)
          == RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  prefs.end();

  if (ok) {
    dbg("[nonces] Saved to NVS OK");
  } else {
    dbg("[nonces] WARNING: NVS save failed!");
  }
  return ok;
}

static bool loadNonces() {
  prefs.begin("lorawan", true);

  if (!prefs.isKey("nonces") || !prefs.isKey("nonces_size")) {
    prefs.end();
    dbg("[nonces] NVS empty – first boot");
    return false;
  }

  uint16_t storedSize = prefs.getUShort("nonces_size", 0);
  if (storedSize != RADIOLIB_LORAWAN_NONCES_BUF_SIZE) {
    prefs.end();
    if (DEBUG) {
      Serial.printf("[nonces] SIZE CHANGED: NVS=%u, RadioLib=%u → discarding old\n",
        storedSize, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
    }
    clearNonces();
    return false;
  }

  size_t readLen = prefs.getBytes("nonces", noncesBuf, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  prefs.end();

  if (readLen != RADIOLIB_LORAWAN_NONCES_BUF_SIZE) {
    if (DEBUG) Serial.printf("[nonces] Read failed: got %u / %u bytes\n",
      readLen, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
    clearNonces();
    return false;
  }

  int16_t state = node->setBufferNonces(noncesBuf);
  if (state != RADIOLIB_ERR_NONE) {
    if (DEBUG) Serial.printf("[nonces] setBufferNonces failed: %d → clean join\n", state);
    clearNonces();
    return false;
  }

  dbg("[nonces] Loaded from NVS and set OK");
  return true;
}

static void clearNonces() {
  prefs.begin("lorawan", false);
  prefs.remove("nonces");
  prefs.remove("nonces_size");
  prefs.end();
  dbg("[nonces] NVS data cleared");
}

// -----------------------------------------------
// ESP32-C3 Deep Sleep GPIO Wake — toimiva versio
static void configureGpioWake(gpio_num_t pin, bool wakeOnHigh) {
  // Aseta pinni oikein joka kerta
  pinMode((uint8_t)pin, INPUT_PULLUP);   // Tärkeä! Microswitch + sisäinen pull-up

  // C3:lle tarvitaan sekä gpio_wakeup_enable että esp_deep_sleep_enable_gpio_wakeup
  gpio_wakeup_enable(pin, wakeOnHigh ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
  
  // TÄRKEIN: Käytä deep sleep -versiota C3:lla
  esp_deep_sleep_enable_gpio_wakeup(1ULL << pin, 
    wakeOnHigh ? ESP_GPIO_WAKEUP_GPIO_HIGH : ESP_GPIO_WAKEUP_GPIO_LOW);

  esp_sleep_enable_gpio_wakeup();   // Vanha kutsu tarvitaan edelleen joissain core-versioissa

  if (DEBUG) {
    Serial.printf("[wake] GPIO%d wake on %s configured (deep sleep enabled)\n", 
                  (int)pin, wakeOnHigh ? "HIGH" : "LOW");
  }
}

// -----------------------------------------------
static void goToSleep(uint32_t timerSec, bool enableGpioWake = true) {
  if (radio) { radio->sleep(); }
  SPI.end();
  nvsCfg.end();

  // Sammuta LED ennen sleepiä (voit kokeilla pinnejä 9 ja 10 myös)
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);
  gpio_hold_en(GPIO_NUM_8);
  gpio_deep_sleep_hold_en();

  batteryEnd();

  if (enableGpioWake) {
    configureGpioWake((gpio_num_t)SWITCH_GPIO, true);
  }

  esp_sleep_enable_timer_wakeup((uint64_t)timerSec * 1000000ULL);

  if (DEBUG) {
    Serial.printf("[sleep] Sleeping %u s | GPIO wake: %s\n",
      timerSec, enableGpioWake ? "ENABLED" : "DISABLED");
    Serial.flush();
    delay(10);
  }

  esp_deep_sleep_start();
}

// -----------------------------------------------
// Radio initialisation
static bool setupRadio() {
  dbg("[radio] Initialising SPI + RFM95W...");
  
  // ESP32-C3 SuperMini: fixed SPI pins from settings.h
  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, RFM_CS_PIN);
  SPI.setFrequency(1000000);
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);

  radio = new SX1276(new Module(RFM_CS_PIN, RFM_DIO0_PIN, RFM_RST_PIN, RFM_DIO1_PIN));
  node  = new LoRaWANNode(radio, &EU868);

  int16_t state = radio->begin();
  if (state != RADIOLIB_ERR_NONE) {
    if (DEBUG) Serial.printf("[radio] Init failed: %d\n", state);
    return false;
  }
  dbg("[radio] OK");

  nvsCfg.getAppKey(appKey);
  joinEUI = nvsCfg.getJoinEUI();
  memcpy(nwkKey, appKey, 16);

  // ADR and datarate BEFORE beginOTAA() → join message at SF7
  node->setADR(false);
  node->setDatarate(5);  // DR5 = SF7BW125

  state = node->beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  if (state != RADIOLIB_ERR_NONE) {
    if (DEBUG) Serial.printf("[radio] beginOTAA failed: %d\n", state);
    return false;
  }
  dbg("[radio] beginOTAA OK");
  return true;
}

// -----------------------------------------------
// OTAA activation
static bool performActivation() {
  dbg("[join] Starting OTAA...");
  const int MAX_ATTEMPTS = 5;
  int16_t state = RADIOLIB_ERR_NETWORK_NOT_JOINED;

  for (int i = 1; i <= MAX_ATTEMPTS; i++) {
    if (DEBUG) Serial.printf("[join] Attempt %d / %d\n", i, MAX_ATTEMPTS);

    state = node->activateOTAA();

    if (state == RADIOLIB_LORAWAN_SESSION_RESTORED ||
        state == RADIOLIB_LORAWAN_NEW_SESSION) {
      dbg("[join] SUCCESS");

      if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
        memcpy(noncesBuf, node->getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
        if (!saveNonces()) {
          dbg("[join] WARNING: Nonces save failed");
        }
        if (DEBUG) Serial.printf("[join] New session – DevNonce saved\n");
      } else {
        dbg("[join] Session restored from nonces data");
      }
      return true;
    }

    if (DEBUG) Serial.printf("[join] Failed, code: %d\n", state);
    delay(15000 + i * 5000);
  }
  return false;
}

// -----------------------------------------------
/// -----------------------------------------------
// Send payload and handle downlink
static bool sendPayload(uint8_t eventType, uint8_t battPct) {
  uint8_t payload[2];
  payload[0] = eventType;
  payload[1] = battPct;

  if (DEBUG) Serial.printf("[payload] %02X %02X (type=%s, batt=%u%%)\n",
    payload[0], payload[1],
    eventType == EVT_MAIL ? "MAIL" : "HEARTBEAT", battPct);

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

    // Process downlink
    if (downlinkLen > 0) {
      if (DEBUG) {
        Serial.printf("[dl] %u bytes: ", downlinkLen);
        for (size_t i = 0; i < downlinkLen; i++) Serial.printf("%02X ", downlink[i]);
        Serial.println();
      }
      handleDownlink(downlink, downlinkLen);
    }

    if (state >= 0) {
      float rssi = radio->getRSSI();
      float snr  = radio->getSNR();
      if (DEBUG) Serial.printf("[uplink] OK | RSSI=%.1f dBm SNR=%.1f dB\n", rssi, snr);

      // Save session & nonces
      memcpy(sessionBuf, node->getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
      rtcSessionBufSize = RADIOLIB_LORAWAN_SESSION_BUF_SIZE;
      isJoined = true;

      memcpy(noncesBuf, node->getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
      saveNonces();

      return true;
    }

    if (DEBUG) Serial.printf("[uplink] Failed (attempt %d), code: %d\n", retries, state);
    delay(10000 + retries * 5000);

  } while (retries < 3 && state < 0);

  dbg("[uplink] All attempts failed");
  return false;
}

// -----------------------------------------------
// Downlink command handler
static void handleDownlink(uint8_t* data, size_t len) {
  if (len < 1) return;

  switch (data[0]) {

    case 0x01: {
      // Change heartbeat interval: [0x01, high, low] seconds
      if (len >= 3) {
        uint32_t newSec = ((uint32_t)data[1] << 8) | data[2];
        if (newSec >= 60 && newSec <= 86400) {
          heartbeatSec = newSec;
          nvsCfg.setHeartbeatSec(newSec);
          if (DEBUG) Serial.printf("[dl] New heartbeat interval: %u s\n", newSec);
        }
      }
      break;
    }

    case 0x02: {
      // Enter OTA mode: [0x02] or [0x02, timeout_min]
      uint8_t timeoutMin = (len >= 2 && data[1] > 0) ? data[1] : 5;
      rtcOtaMode = true;
      rtcOtaTimeoutMin = timeoutMin;
      if (DEBUG) Serial.printf("[dl] OTA mode set: %u min\n", timeoutMin);
      break;
    }

    default:
      if (DEBUG) Serial.printf("[dl] Unknown command: 0x%02X\n", data[0]);
      break;
  }
}

// ============================================================
// SETUP – all logic here (loop is unused)
// ============================================================
void setup() {
  // ── 0. Disable brownout reset (ESP32 only, skip on C3) ───
  #ifndef CONFIG_IDF_TARGET_ESP32C3
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  #endif

  if (DEBUG) {
    Serial.begin(115200);
    while (!Serial) delay(10);
    delay(500);
  }

  // ── LED sammutus (ESP32-C3 SuperMini) ─────────────────────
  pinMode(8, OUTPUT);           // Sininen status-LED useimmissa SuperMinissä (pin 8,9 tai 10)
  digitalWrite(8, HIGH);        // HIGH = LED pois (active-low)

  // Varmistetaan myös deep sleep -tilassa
  gpio_hold_en(GPIO_NUM_8);
  gpio_deep_sleep_hold_en();

  buildDevEUI();
  printVersionInfo();

  // ── 1. Determine wake cause ───────────────────────────────
  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  uint8_t eventType = EVT_HEARTBEAT;

  if (cause == ESP_SLEEP_WAKEUP_GPIO) {
    if (DEBUG) Serial.println("[wake] GPIO – mailbox lid opened!");
    eventType = EVT_MAIL;
  } else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    if (inCooldown) {
      // Woke from cooldown after a mail event
      if (DEBUG) Serial.println("[wake] Cooldown expired – resuming normal operation");
      inCooldown = false;
      // Go straight back to sleep with GPIO + heartbeat timer
      // No need to send anything
      goToSleep(heartbeatSec, true);
      return;  // never reached
    }
    if (DEBUG) Serial.printf("[wake] Timer – heartbeat (%u s interval)\n", heartbeatSec);
    eventType = EVT_HEARTBEAT;
  } else {
    if (DEBUG) Serial.println("[wake] First boot / power-on reset");
    eventType = EVT_HEARTBEAT;
  }

  // ── 2. Open NVS settings ───────────────────────────────────
  nvsCfg.begin();
  heartbeatSec = nvsCfg.getHeartbeatSec();
  if (DEBUG) nvsCfg.printAll();

  // ── 3. OTA mode check (from previous downlink) ────────────
  if (rtcOtaMode) {
    rtcOtaMode = false;
    dbg("[ota] OTA mode active – checking for update");
    uint16_t timeoutSec = (uint16_t)rtcOtaTimeoutMin * 60;
    enterOtaMode(timeoutSec);
    dbg("[ota] Resuming normal operation");
  }

  // ── 4. Battery measurement ─────────────────────────────────
  batteryBegin();  // Updated in battery.h for ESP32-C3 ADC
  float vbatt  = batteryVoltage();
  uint8_t batt = batteryPercent(vbatt);
  if (DEBUG) Serial.printf("[batt] %.2f V = %u %%\n", vbatt, batt);

  // ── 5. Radio + LoRaWAN ─────────────────────────────────────
  if (!setupRadio()) {
    dbg("[radio] Init failed → sleeping 5 min");
    goToSleep(300, true);
  }

  // ── 6. Load nonces from NVS ────────────────────────────────
  bool noncesLoaded = loadNonces();

  // ── 7. Restore session from RTC ────────────────────────────
  if (isJoined && rtcSessionBufSize != RADIOLIB_LORAWAN_SESSION_BUF_SIZE) {
    if (DEBUG) {
      Serial.printf("[session] SESSION_BUF_SIZE changed: RTC=%u, RadioLib=%u → discarding\n",
        rtcSessionBufSize, RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
    }
    isJoined = false;
  }

  if (isJoined) {
    dbg("[session] Restoring session from RTC...");
    int16_t s2 = node->setBufferSession(sessionBuf);
    if (s2 != RADIOLIB_ERR_NONE) {
      if (DEBUG) Serial.printf("[session] Invalid (%d) → rejoining\n", s2);
      isJoined = false;
    } else {
      dbg("[session] Restored OK – skipping OTAA");
    }
  }

  // ── 8. Activate OTAA ───────────────────────────────────────
  if (!noncesLoaded && !isJoined) {
    dbg("[join] No nonces data → clean join (ensure ChirpStack flush)");
  }

  if (!performActivation()) {
    dbg("[join] Activation failed → sleeping 5 min");
    goToSleep(300, true);
  }

  // ── 9. Send payload ────────────────────────────────────────
  sendPayload(eventType, batt);

  // ── 10. Deep sleep ─────────────────────────────────────────
  if (eventType == EVT_MAIL) {
    // After mail alert: short cooldown to prevent re-trigger from lid bounce
    // During cooldown, only timer wake (no GPIO wake)
    inCooldown = true;
    if (DEBUG) Serial.printf("[sleep] Mail cooldown: %u s (GPIO wake disabled)\n", MAIL_COOLDOWN_SEC);
    goToSleep(MAIL_COOLDOWN_SEC, false);
  } else {
    // Normal heartbeat: sleep with both GPIO and timer wake
    goToSleep(heartbeatSec, true);
  }
}

// Loop is unused – all logic in setup() due to deep sleep architecture
void loop() {}