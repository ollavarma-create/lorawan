#pragma once
#include <Wire.h>
#include <SPI.h>
#include <RadioLib.h>
#include "settings.h"

// ============================================================
// pin_probe.h – Automaattinen pin-tunnistus
//
// Strategia:
//  1. Kokeillaan BME280 I2C-pinnit: Wire.begin() + osoiteskannaus
//  2. Kokeillaan RFM95W SPI-pinnit: RadioLib SX1276.begin() vastaa?
//  3. Tallennetaan löytynyt yhdistelmä NVS:ään (välimuisti)
//
// Proobaus tehdään vain kerran (ensimmäinen käynnistys tai NVS tyhjennetty).
// Seuraavilla käynnistyksillä käytetään välimuistia → nopea käynnistys.
// ============================================================

// -----------------------------------------------
// I2C-skannaus: löytyykö BME280 (0x76 tai 0x77) pinniparilla {sda, scl}?
// Palauttaa löytyneen osoitteen tai 0 jos ei löydy.
static uint8_t probeI2C(int8_t sda, int8_t scl) {
  Wire.begin(sda, scl);
  delay(2);  // Annetaan väylän vakiintua

  uint8_t foundAddr = 0;
  for (uint8_t addr : {0x76, 0x77}) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      foundAddr = addr;
      break;
    }
  }

  Wire.end();
  return foundAddr;
}

// -----------------------------------------------
// SPI-proobaus: vastaako SX1276 pinneillä {sck, mosi, miso}?
// RadioLib lukee version-rekisterin (0x42 = SX1276).
// Palauttaa true jos radio tunnistettu.
static bool probeSPI(int8_t sck, int8_t mosi, int8_t miso) {
  SPI.begin(sck, miso, mosi, RFM_CS_PIN);
  delay(5);

  // Lyhytaikainen moduuli testausta varten
  Module mod(RFM_CS_PIN, RFM_DIO0_PIN, RFM_RST_PIN, RFM_DIO1_PIN);
  SX1276 testRadio(&mod);

  int16_t state = testRadio.begin();
  testRadio.sleep();  // Sammuta heti löytymisen jälkeen
  SPI.end();

  return (state == RADIOLIB_ERR_NONE);
}

// -----------------------------------------------
// autoDetectPins – etsii toimivat I2C ja SPI pinnit
//
// Käy läpi kandidaattilistoja asetustiedostosta.
// Täyttää cfg:n löytyneillä pinneillä.
// Palauttaa true jos MOLEMMAT (I2C ja SPI) löytyivät.
//
// Jos vain toinen löytyy → täytetään se, toinen jää oletukseen.
// Jos kumpikaan ei löydy → cfg saa vakiot settings.h:sta, palautetaan false.
static bool autoDetectPins(PinConfig& cfg, bool debug = false) {
  bool i2cFound = false;
  bool spiFound = false;

  // --- I2C-skannaus ---
  if (debug) Serial.println("[probe] Etsitään BME280 I2C...");

  for (int i = 0; BME_I2C_CANDIDATES[i][0] != -1; i++) {
    int8_t sda = BME_I2C_CANDIDATES[i][0];
    int8_t scl = BME_I2C_CANDIDATES[i][1];

    if (debug) Serial.printf("[probe]  Kokeillaan SDA=%d SCL=%d... ", sda, scl);

    uint8_t addr = probeI2C(sda, scl);
    if (addr != 0) {
      cfg.sda = sda;
      cfg.scl = scl;
      i2cFound = true;
      if (debug) Serial.printf("LÖYTYI @ 0x%02X\n", addr);
      break;
    }
    if (debug) Serial.println("ei vastausta");
    yield();
  }

  if (!i2cFound && debug) Serial.println("[probe] BME280 ei löydy – käytetään oletuspinnit");

  // --- SPI-skannaus ---
  if (debug) Serial.println("[probe] Etsitään RFM95W SPI...");

  for (int i = 0; SPI_CANDIDATES[i][0] != -1; i++) {
    int8_t sck  = SPI_CANDIDATES[i][0];
    int8_t mosi = SPI_CANDIDATES[i][1];
    int8_t miso = SPI_CANDIDATES[i][2];

    if (debug) Serial.printf("[probe]  Kokeillaan SCK=%d MOSI=%d MISO=%d... ", sck, mosi, miso);

    if (probeSPI(sck, mosi, miso)) {
      cfg.sck  = sck;
      cfg.mosi = mosi;
      cfg.miso = miso;
      spiFound = true;
      if (debug) Serial.println("LÖYTYI");
      yield();
      break;
    }
    if (debug) Serial.println("ei vastausta");
  }

  if (!spiFound && debug) Serial.println("[probe] RFM95W ei löydy – käytetään oletuspinnit");

  return i2cFound && spiFound;
}
