#pragma once
#include <Preferences.h>
#include <Arduino.h>
#include "settings.h"

// ============================================================
// nvs_config.h – NVS persistent settings
//
// All key names max 15 characters (NVS limitation).
// Namespace: "postihal" (NVS_NS)
//
// Usage:
//   nvsCfg.begin();
//   uint32_t hb = nvsCfg.getHeartbeatSec();
//   nvsCfg.end();
// ============================================================

class NVSConfig {
public:

  void begin() {
    _p.begin(NVS_NS, false);
  }

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

  // ---- OTA hostname (optional custom name) ----------------

  String getDeviceName() {
    return _p.getString("dev_name", "");
  }
  void setDeviceName(const String& n) {
    _p.putString("dev_name", n);
  }

  // ---- Heartbeat interval ---------------------------------

  uint32_t getHeartbeatSec() {
    return _p.getUInt("hb_sec", DEFAULT_HEARTBEAT_SEC);
  }
  void setHeartbeatSec(uint32_t s) {
    _p.putUInt("hb_sec", s);
  }

  // ---- LoRaWAN keys ---------------------------------------

  uint64_t getJoinEUI() {
    return _p.getULong64("join_eui", DEFAULT_JOIN_EUI);
  }
  void setJoinEUI(uint64_t v) {
    _p.putULong64("join_eui", v);
  }

  bool getAppKey(uint8_t out[16]) {
    size_t len = _p.getBytesLength("app_key");
    if (len != 16) return false;
    _p.getBytes("app_key", out, 16);
    return true;
  }
  void setAppKey(const uint8_t key[16]) {
    _p.putBytes("app_key", key, 16);
  }

  // ---- Diagnostics ----------------------------------------

  void printAll() {
    Serial.println("=== NVS settings ===");
    Serial.printf("  wifi_ssid  : %s\n",        getWifiSSID().c_str());
    Serial.printf("  hb_sec     : %u s\n",       getHeartbeatSec());
    Serial.printf("  join_eui   : 0x%016llX\n",  getJoinEUI());
    String dn = getDeviceName();
    if (dn.length() > 0) Serial.printf("  dev_name   : %s\n", dn.c_str());
    Serial.println("====================");
  }

private:
  Preferences _p;
};

extern NVSConfig nvsCfg;
