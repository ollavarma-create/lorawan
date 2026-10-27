#pragma once

// ============================================================
// settings.h – Salaojasensori
// ============================================================

#define FW_VERSION "1.0.0"

// -----------------------------------------------
// WiFi – tarvitaan vain OTA-päivityksiä varten
#define DEFAULT_WIFI_SSID   "WiFi-verkko"
#define DEFAULT_WIFI_PASS   "salasana"

// OTA
#define OTA_HOSTNAME_PREFIX "drain-"
#define OTA_PASSWORD        ""
#define OTA_TIMEOUT_SEC     300

// -----------------------------------------------
// LoRaWAN avaimet – vaihda Chirpstackin arvot
// devEUI generoidaan automaattisesti MAC-osoitteesta
#define DEFAULT_JOIN_EUI    0x0000000000000000ULL
#define DEFAULT_APP_KEY     { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

// -----------------------------------------------
// Akkumittaus – 3×AA, voltage divider 100k+100k
//
// Kytkentä:
//   VBATT ── R1(100k) ──┬── BATT_ADC_GPIO
//                        │
//                       R2(100k)
//                        │
//                     BATT_GND_GPIO (OUTPUT LOW = mittaus päällä)
//
// 3×AA: 4.5V täynnä → 2.25V ADC (ok, alle 3.3V)
//       3.6V alaraja → 1.8V ADC (HT7833 dropout-raja)
#define BATT_GND_GPIO       32
#define BATT_ADC_GPIO       34
#define BATT_DIVIDER_RATIO  2.0f
#define BATT_SAMPLES        8
#define BATT_WARMUP_MS      5
#define BATT_MIN_V          3.6f    // HT7833 tarvitsee min ~3.6V sisään
#define BATT_MAX_V          4.5f    // 3×AA täynnä
#define BATT_ADC_MAX        4095

// -----------------------------------------------
// Deep sleep – oletusaika 1 tunti
#define DEFAULT_SLEEP_SEC   3600

// -----------------------------------------------
// Vesisensori – resistiivinen elektrodipari
//
// Kytkentä:
//   WATER_EXCITE_GPIO ──── Elektrodi 1
//                                   ~~~ (vesi johtaa) ~~~
//   WATER_SENSE_GPIO  ──── Elektrodi 2
//   (ESP32:n sisäinen pull-up ~45kΩ → 3.3V)
//
// Mittaus vain herätyksen aikana (~10 ms) → ei elektrolyysiä
#define WATER_EXCITE_GPIO   26
#define WATER_SENSE_GPIO    27

// -----------------------------------------------
// RFM95W (SX1276)
#define RFM_CS_PIN          5
#define RFM_DIO0_PIN        4
#define RFM_RST_PIN         RADIOLIB_NC
#define RFM_DIO1_PIN        RADIOLIB_NC

// -----------------------------------------------
// NVS-nimiavaruus
#define NVS_NS              "drain"

// -----------------------------------------------
// Kiinteä pinniasetus – ESP32-WROOM-32D
struct PinConfig {
  int8_t sda  = 21;   // ei käytössä (ei I2C-sensoria)
  int8_t scl  = 22;   // ei käytössä
  int8_t sck  = 18;   // RFM95W SPI
  int8_t mosi = 23;
  int8_t miso = 19;
};

#define BOARD_PINS_KNOWN
static inline PinConfig getDefaultPinConfig() {
  return PinConfig{};
}
