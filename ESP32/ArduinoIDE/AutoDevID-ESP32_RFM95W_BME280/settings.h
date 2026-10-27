#pragma once

// ============================================================
// settings.h – Käännösaikaiset vakiot ja oletusarvot
// Muuta TÄNNE omat WiFi-tunnukset, avaimet ja pinnit.
// Kaikki voidaan ylikirjoittaa NVS:llä ajon aikana.
// ============================================================

// Firmware-versio (näkyy OTA-isäntänimessä ja serialissa)
#define FW_VERSION "1.1.0"

// -----------------------------------------------
// WiFi – tarvitaan VAIN OTA-päivityksiä varten
// Laitteen on normaalisti deep sleepissä eikä tarvitse WiFiä.
// Kirjoita oikeat arvot tai aseta NVS:ään: nvsCfg.setWifiSSID / setWifiPass
#define DEFAULT_WIFI_SSID   "Wifi Access Pointtis"
#define DEFAULT_WIFI_PASS   "salarisi"

// OTA – ArduinoOTA-asetukset
// Isäntänimi muodostuu automaattisesti: "lorawan-" + 4 viimeistä MAC-tavua (hex)
// Esim. "lorawan-ce0c98"  → näkyy Arduino IDE:n Portit-valikossa
#define OTA_HOSTNAME_PREFIX "lorawan-"
#define OTA_PASSWORD        ""          // Tyhjä = ei salasanaa OTA-yhteydelle
#define OTA_TIMEOUT_SEC     300         // Kuinka kauan odotetaan OTA-pushia (sekuntia)

// -----------------------------------------------
// LoRaWAN avaimet – VAIHDA OMAT ARVOSI
// Sama kaikille laitteille. devEUI generoidaan automaattisesti MAC:sta.
// Huom: joinEUI on 8 tavua = max 16 hex-merkkiä (uint64_t)
#define DEFAULT_JOIN_EUI    0x4dd50223df97c4bbULL
#define DEFAULT_APP_KEY { 0x82, 0x0c, 0x0f, 0xd6, 0x73, 0x27, 0x52, 0x8c, \
                          0x19, 0x3e, 0x05, 0xa9, 0x31, 0xc0, 0x88, 0xab }

// -----------------------------------------------
// Akun mittaus – voltage divider, GPIO-ohjattu GND
//
// Kytkentä:
//   VBATT ──── R_top (100k) ──┬──── BATT_ADC_GPIO  (ESP32 ADC)
//                              │
//                             R_bot (100k)
//                              │
//                           BATT_GND_GPIO  (OUTPUT LOW = mittaus päällä)
//
// BATT_GND_GPIO = OUTPUT LOW  → mittaus aktiivinen, R_top/R_bot jakaa jännitteen
// BATT_GND_GPIO = INPUT       → ei virtaa, ei kulutusta nukkuessa
//
// LiPo-akku: 4.2V täynnä, 3.0V tyhjä
// Divi-kerroin 2.0 (R_top = R_bot): max ADC = 4.2/2 = 2.1V → alle 3.3V, OK
#define BATT_GND_GPIO       32          // GPIO joka kytkee alimman vastuksen maahan
#define BATT_ADC_GPIO       34           // ADC-luku (0–3.3V)
#define BATT_DIVIDER_RATIO  2.0f        // Vbatt = Vadc * BATT_DIVIDER_RATIO
#define BATT_SAMPLES        8           // Montako ADC-lukua keskiarvoistetaan
#define BATT_WARMUP_MS      5           // Odotus ennen mittausta (kondensaattori latautuu)
#define BATT_MIN_V          3.0f        // Tyhjä-raja (volttia)
#define BATT_MAX_V          4.2f        // Täynnä-raja (volttia)
#define BATT_ADC_MAX    4095

// -----------------------------------------------
// Deep sleep – oletusaika
#define DEFAULT_SLEEP_SEC   600         // 10 minuuttia

// -----------------------------------------------
// RFM95W (SX1276) – kiinteät ohjaus-pinnit, sama molemmissa boardeissa.
// SPI-databussipinnit (SCK/MOSI/MISO) vaihtelevat boardin mukaan
// → ne probootaan tai luetaan NVS:stä.
#define RFM_CS_PIN          5
#define RFM_DIO0_PIN        4
#define RFM_RST_PIN         RADIOLIB_NC   // Ei reset-pinniä käytössä
#define RFM_DIO1_PIN        RADIOLIB_NC

// -----------------------------------------------
// NVS-nimiavaruus (max 15 merkkiä)
#define NVS_NS              "lora"

// -----------------------------------------------
// PinConfig – proobattujen tai NVS:stä luettujen pinnien rakenne
// Jaetaan settings.h:ssa, jotta pin_probe.h ja nvs_config.h voivat käyttää sitä.
struct PinConfig {
  int8_t sda  = 0;    // BME280 I2C data
  int8_t scl  = 1;    // BME280 I2C kello
  int8_t sck  = 6;    // RFM95W SPI kello
  int8_t mosi = 3;    // RFM95W SPI MOSI (SuperMini oletus)
  int8_t miso = 7;    // RFM95W SPI MISO (SuperMini oletus)
};

// -----------------------------------------------
// Pin-probing kandidaatit – etsitään tässä järjestyksessä
//
// BME280 I2C: {SDA, SCL} -parit
// Yleisimmät ESP32-C3 ja WROOM-32 yhdistelmät
static const int8_t BME_I2C_CANDIDATES[][2] = {
  { 0,  1},   // SuperMini & DevBoard (molemmat)
  { 8,  9},   // Vaihtoehtoinen C3
  {20, 21},   // C3 WROOM vaihtoehto
  {21, 22},   // WROOM-32 klassinen
  { 4,  5},   // Yleinen vaihtoehto
  {-1, -1}    // Sentinel – lopeta haku
};

// RFM95W SPI: {SCK, MOSI, MISO} kolmikot
// CS = RFM_CS_PIN (5), ei vaihdu
static const int8_t SPI_CANDIDATES[][3] = {
  { 6,  3,  7},   // ESP32-C3 SuperMini
  { 6,  2,  9},   // ESP32-C3 DevBoard
  {18, 23, 19},   // WROOM-32 VSPI klassinen
  {14, 13, 12},   // VSPI vaihtoehto
  { 5, 27, 19},   // HSPI
  {-1, -1, -1}    // Sentinel
};

// Kovakoodattu pinniasetus – ohittaa proobauksen
// Poista kommentti kun boardin pinnit tiedetään
#define BOARD_PINS_KNOWN

#ifdef BOARD_PINS_KNOWN
static inline PinConfig getDefaultPinConfig() {
  PinConfig c;
  c.sda  = 21;
  c.scl  = 22;
  c.sck  = 18;
  c.mosi = 23;
  c.miso = 19;
  return c;
}
#endif
