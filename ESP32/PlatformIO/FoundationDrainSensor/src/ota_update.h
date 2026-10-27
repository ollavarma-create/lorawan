#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "settings.h"

static void enterOtaMode(uint16_t timeoutSec) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 30000) {
    delay(500);
  }
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char hostname[32];
  snprintf(hostname, sizeof(hostname), "%s%02x%02x%02x",
           OTA_HOSTNAME_PREFIX, mac[3], mac[4], mac[5]);

  ArduinoOTA.setHostname(hostname);
  if (strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }
  ArduinoOTA.begin();

  t0 = millis();
  while (millis() - t0 < (uint32_t)timeoutSec * 1000UL) {
    ArduinoOTA.handle();
    delay(10);
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
