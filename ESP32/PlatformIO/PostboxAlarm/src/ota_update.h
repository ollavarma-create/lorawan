#pragma once
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "settings.h"
#include "nvs_config.h"

// ============================================================
// ota_update.h – ArduinoOTA over WiFi
//
// Usage:
//  1. Device receives downlink [0x02, timeout_min]
//  2. RTC flag set: rtcOtaMode = true
//  3. On next wake: enterOtaMode() is called
//  4. Device connects to WiFi, starts ArduinoOTA server
//  5. User opens Arduino IDE → Ports → "postihaly-XXXX (ESP32)"
//  6. Uploads new firmware normally
//  7. ESP32 reboots automatically after successful OTA
//
// If timeout expires before push:
//  → rtcOtaMode is cleared, device resumes normal operation
// ============================================================

// Build OTA hostname from MAC address
// Returns e.g. "postihaly-ce0c98"
static String buildOtaHostname() {
  String custom = nvsCfg.getDeviceName();
  if (custom.length() > 0) return custom;

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char buf[24];
  snprintf(buf, sizeof(buf), "%s%02x%02x%02x",
    OTA_HOSTNAME_PREFIX, mac[3], mac[4], mac[5]);
  return String(buf);
}

// Main function: connect to WiFi and wait for OTA update
// Returns true if OTA was performed (device reboots automatically)
// Returns false if timeout or WiFi failure
static bool enterOtaMode(uint16_t timeoutSec = OTA_TIMEOUT_SEC) {
  String ssid = nvsCfg.getWifiSSID();
  String pass = nvsCfg.getWifiPass();
  String hostname = buildOtaHostname();

  Serial.printf("\n[OTA] Active – waiting %u s\n", timeoutSec);
  Serial.printf("[OTA] Hostname: %s (FW: %s)\n", hostname.c_str(), FW_VERSION);
  Serial.printf("[OTA] WiFi: %s\n", ssid.c_str());

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());

  Serial.print("[OTA] Connecting");
  uint32_t wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifiStart > 20000) {
      Serial.println("\n[OTA] WiFi connection failed, exiting OTA mode");
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      return false;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[OTA] Connected: %s\n", WiFi.localIP().toString().c_str());

  // Configure ArduinoOTA
  ArduinoOTA.setHostname(hostname.c_str());

  String pwd = String(OTA_PASSWORD);
  if (pwd.length() > 0) {
    ArduinoOTA.setPassword(pwd.c_str());
  }

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
    Serial.printf("[OTA] Starting update: %s\n", type.c_str());
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update complete – rebooting");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static uint8_t lastPct = 255;
    uint8_t pct = (progress * 100) / total;
    if (pct != lastPct && pct % 10 == 0) {
      Serial.printf("[OTA] %u%%\n", pct);
      lastPct = pct;
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    const char* msg;
    switch (error) {
      case OTA_AUTH_ERROR:    msg = "Auth error";      break;
      case OTA_BEGIN_ERROR:   msg = "Begin error";     break;
      case OTA_CONNECT_ERROR: msg = "Connect error";   break;
      case OTA_RECEIVE_ERROR: msg = "Receive error";   break;
      case OTA_END_ERROR:     msg = "End error";       break;
      default:                msg = "Unknown error";   break;
    }
    Serial.printf("[OTA] Error [%u]: %s\n", error, msg);
  });

  ArduinoOTA.begin();

  Serial.println("[OTA] Server running – open Arduino IDE → Ports");
  Serial.printf("[OTA] Waiting up to %u seconds...\n", timeoutSec);

  uint32_t otaStart = millis();
  uint32_t lastPrint = 0;
  while (millis() - otaStart < (uint32_t)timeoutSec * 1000) {
    ArduinoOTA.handle();

    uint32_t elapsed   = (millis() - otaStart) / 1000;
    uint32_t remaining = timeoutSec - elapsed;

    if (elapsed - lastPrint >= 30) {
      lastPrint = elapsed;
      if (remaining > 0) {
        Serial.printf("[OTA] %u s remaining...\n", remaining);
      }
    }
    delay(10);
  }

  Serial.println("[OTA] Timeout – resuming normal operation");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return false;
}
