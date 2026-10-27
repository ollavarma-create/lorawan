#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "settings.h"

class NVSConfig {
  Preferences _prefs;

public:
  void begin() {
    _prefs.begin(NVS_NS, false);
  }

  void end() {
    _prefs.end();
  }

  uint32_t getSleepSec() {
    return _prefs.getUInt("sleep_sec", DEFAULT_SLEEP_SEC);
  }

  void setSleepSec(uint32_t sec) {
    _prefs.putUInt("sleep_sec", sec);
  }

  void getAppKey(uint8_t* key) {
    uint8_t def[16] = DEFAULT_APP_KEY;
    if (_prefs.getBytes("app_key", key, 16) != 16) {
      memcpy(key, def, 16);
    }
  }

  void setAppKey(const uint8_t* key) {
    _prefs.putBytes("app_key", key, 16);
  }

  uint64_t getJoinEUI() {
    return _prefs.getULong64("join_eui", DEFAULT_JOIN_EUI);
  }

  void setJoinEUI(uint64_t eui) {
    _prefs.putULong64("join_eui", eui);
  }
};
