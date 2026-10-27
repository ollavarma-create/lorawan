#pragma once
#include <Preferences.h>
#include <Arduino.h>
#include "settings.h"

// ============================================================
// nvs_config.h – NVS-asetukset (ajonaikaiset, pysyvät)
//
// Kaikki avainnimet max 15 merkkiä (NVS-rajoitus).
// Namespace: "lora" (NVS_NS)
//
// Käyttö:
//   nvsCfg.begin();
//   String ssid = nvsCfg.getWifiSSID();
//   nvsCfg.setSleepSec(300);
//   nvsCfg.end();
// ============================================================

class NVSConfig {
public:

  // Avaa NVS-yhteys – kutsu setup():ssa
  void begin() {
    _p.begin(NVS_NS, false);
  }

  // Sulje NVS-yhteys – kutsu ennen deep sleepiä
  void end() {
    _p.end();
  }

  // ---- WiFi -----------------------------------------------

  String getWifiSSID() {
    return _p.getString("wifi_ssid", DEFAULT_WIFI_SSID);
  }
  void setWifiSSID(const String& s) {
    _p.putString("wifi_ssid", s);
  }

  String getWifiPass() {
    return _p.getString("wifi_pass", DEFAULT_WIFI_PASS);
  }
  void setWifiPass(const String& s) {
    _p.putString("wifi_pass", s);
  }

  // ---- OTA-isäntänimi (valinnainen mukautus) ---------------

  // Palauttaa mukautetun nimen tai automaattisen (lorawan-XXXX)
  String getDeviceName() {
    return _p.getString("dev_name", "");
  }
  void setDeviceName(const String& n) {
    _p.putString("dev_name", n);
  }

  // ---- Deep sleep -----------------------------------------

  uint32_t getSleepSec() {
    return _p.getUInt("sleep_sec", DEFAULT_SLEEP_SEC);
  }
  void setSleepSec(uint32_t s) {
    _p.putUInt("sleep_sec", s);
  }

  // ---- LoRaWAN-avaimet ------------------------------------
  // Tallennetaan vain jos halutaan ylikirjoittaa käännösaikaiset oletukset.
  // Palauttaa käännösaikaisen oletuksen jos NVS on tyhjä.

  uint64_t getJoinEUI() {
    return _p.getULong64("join_eui", DEFAULT_JOIN_EUI);
  }
  void setJoinEUI(uint64_t v) {
    _p.putULong64("join_eui", v);
  }

  // AppKey: 16 tavua. Palauttaa true jos NVS:ssä on tallennettu avain.
  bool getAppKey(uint8_t out[16]) {
    size_t len = _p.getBytesLength("app_key");
    if (len != 16) return false;
    _p.getBytes("app_key", out, 16);
    return true;
  }
  void setAppKey(const uint8_t key[16]) {
    _p.putBytes("app_key", key, 16);
  }

  // ---- Pinnit (proobauksen välimuisti) --------------------
  //
  // hasPinConfig() → true jos proobaus on tehty ja tallennettu
  // getPinConfig()  → täyttää cfg:n, palauttaa true jos löytyi
  // setPinConfig()  → tallentaa löydetyt pinnit
  // clearPinConfig() → pakottaa proobauksen uudelleen seuraavalla käynnistyksellä

  bool hasPinConfig() {
    return _p.getBool("pins_ok", false);
  }

  bool getPinConfig(PinConfig& cfg) {
    if (!hasPinConfig()) return false;
    cfg.sda  = (int8_t)_p.getChar("pin_sda",  0);
    cfg.scl  = (int8_t)_p.getChar("pin_scl",  1);
    cfg.sck  = (int8_t)_p.getChar("pin_sck",  6);
    cfg.mosi = (int8_t)_p.getChar("pin_mosi", 3);
    cfg.miso = (int8_t)_p.getChar("pin_miso", 7);
    return true;
  }

  void setPinConfig(const PinConfig& cfg) {
    _p.putChar("pin_sda",  cfg.sda);
    _p.putChar("pin_scl",  cfg.scl);
    _p.putChar("pin_sck",  cfg.sck);
    _p.putChar("pin_mosi", cfg.mosi);
    _p.putChar("pin_miso", cfg.miso);
    _p.putBool("pins_ok",  true);
  }

  void clearPinConfig() {
    _p.remove("pins_ok");
    _p.remove("pin_sda");
    _p.remove("pin_scl");
    _p.remove("pin_sck");
    _p.remove("pin_mosi");
    _p.remove("pin_miso");
  }

  // ---- Diagnostiikka --------------------------------------

  // Tulosta kaikki tallennetut asetukset serialiin (debug-käyttöön)
  void printAll() {
    Serial.println("=== NVS-asetukset ===");
    Serial.printf("  wifi_ssid : %s\n",       getWifiSSID().c_str());
    Serial.printf("  sleep_sec : %u s\n",      getSleepSec());
    Serial.printf("  join_eui  : 0x%016llX\n", getJoinEUI());
    if (hasPinConfig()) {
      PinConfig c; getPinConfig(c);
      Serial.printf("  pins      : SDA=%d SCL=%d SCK=%d MOSI=%d MISO=%d\n",
        c.sda, c.scl, c.sck, c.mosi, c.miso);
    } else {
      Serial.println("  pins      : ei tallennettu, proobataan");
    }
    String dn = getDeviceName();
    if (dn.length() > 0) Serial.printf("  dev_name  : %s\n", dn.c_str());
    Serial.println("=====================");
  }

private:
  Preferences _p;
};

// Globaali instanssi – käytä kaikkialla NVSin sijaan
extern NVSConfig nvsCfg;
