#pragma once

// ============================================================
// settings.h – Compile-time constants and defaults
//
// LoRaWAN Mailbox Alert – ESP32-C3 SuperMini + RFM95W
//
// Hardware:
//   - NCR18650 Li-Ion 3.7V → HT7833 LDO → 3.3V
//   - RFM95W-868S2 LoRa module (SPI)
//   - Microswitch on mailbox lid (EXT1 wake)
//   - Voltage divider for battery monitoring (always-on)
//   - Timer: periodic heartbeat (battery status)
// ============================================================

// Firmware version (shown in OTA hostname and serial output)
#define FW_VERSION "1.0.1"

// -----------------------------------------------
// WiFi – only needed for OTA updates
// Overridable via NVS: nvsCfg.setWifiSSID / setWifiPass
#define DEFAULT_WIFI_SSID   "yourAP"
#define DEFAULT_WIFI_PASS   "yourPASS"

// OTA – ArduinoOTA settings
// Hostname is auto-generated: "postihaly-" + last 3 MAC bytes (hex)
#define OTA_HOSTNAME_PREFIX "postihaly-"
#define OTA_PASSWORD        ""
#define OTA_TIMEOUT_SEC     300

// -----------------------------------------------
// LoRaWAN keys – REPLACE WITH YOUR OWN!!
// devEUI is auto-generated from MAC address.
#define DEFAULT_JOIN_EUI    0x118074e00397eb7ULL
#define DEFAULT_APP_KEY { 0x11, 0xa1, 0x5d, 0x68, 0xbf, 0x9f, 0xdf, 0xc2, 0x12, 0x61, 0xa9, 0x88, 0xa8, 0x49, 0x26, 0xb0 }

// -----------------------------------------------
// Microswitch – mailbox lid sensor
//
// Wiring:
//   GPIO2 ── 10k pull-up to 3.3V
//   GPIO2 ── microswitch ── GND
//
// Lid closed: switch pressed → GPIO2 = LOW  (held by switch)
// Lid opens:  switch released → GPIO2 = HIGH (pulled up)
//
// Wake trigger: EXT2 on HIGH level (lid opened)
//
// Re-trigger prevention: MAIL_COOLDOWN_SEC sleep with EXT0 disabled.
#define SWITCH_GPIO         2

// -----------------------------------------------
// Battery measurement – switched voltage divider
//
// Wiring:
//   VBATT ── R5 (100k) ──┬── GPIO1 (ADC)
//                         └── R6 (100k) ── GPIO10 (switchable GND)
//
// When measuring: set GPIO10 LOW → divider active
// When sleeping:  set GPIO10 HIGH/INPUT → divider disconnected (~0 µA)
//
// LiPo: 4.2V full → ADC sees 2.1V, 3.0V empty → ADC sees 1.5V
#define BATT_ADC_GPIO       1
#define BATT_GND_SWITCH_GPIO 10 
#define BATT_DIVIDER_RATIO  2.0f
#define BATT_SAMPLES        8
#define BATT_WARMUP_MS      2           // Short settle time (no switching)
#define BATT_MIN_V          3.0f
#define BATT_MAX_V          4.2f

// -----------------------------------------------
// Heartbeat – periodic wake for battery reporting
// Default: 12 hours. Overridable via NVS or downlink.
#define DEFAULT_HEARTBEAT_SEC  43200    // 12 h

// Cooldown after mail alert – prevents re-trigger if lid bounces
#define MAIL_COOLDOWN_SEC      60

// -----------------------------------------------
// Event types (payload byte 0)
#define EVT_MAIL        0x01
#define EVT_HEARTBEAT   0x02

// -----------------------------------------------
// RFM95W (SX1276) – control pins
// SPI data pins: GPIO18(SCK), GPIO19(MISO), GPIO23(MOSI)
#define RFM_CS_PIN          5
#define RFM_DIO0_PIN        4
#define RFM_RST_PIN         RADIOLIB_NC
#define RFM_DIO1_PIN        RADIOLIB_NC

// Fixed SPI pins for ESP32-C3-SuperMini
#define SPI_SCK_PIN         6
#define SPI_MISO_PIN        7
#define SPI_MOSI_PIN        3

// -----------------------------------------------
// NVS namespace (max 15 characters)
#define NVS_NS              "postihal"
