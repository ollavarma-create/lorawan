#pragma once
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "settings.h"
#include "nvs_config.h"

// ============================================================
// ota_update.h – ArduinoOTA WiFin kautta
//
// Käyttötapa:
//  1. Laite vastaanottaa downlinkin [0x02, timeout_min]
//  2. Asetetaan RTC-lippu: rtcOtaMode = true
//  3. Seuraavalla heräämisellä: enterOtaMode() kutsutaan
//  4. Laite yhdistää WiFiin, käynnistää ArduinoOTA-palvelimen
//  5. Käyttäjä avaa Arduino IDE → Portit → "lorawan-XXXX (ESP32)"
//  6. Lataa uusi firmware normaalisti
//  7. ESP32 käynnistyy uudelleen automaattisesti OTA:n jälkeen
//
// Jos timeout menee umpeen ennen pushia:
//  → rtcOtaMode tyhjennetään, laite jatkaa normaalia toimintaa
// ============================================================

// Muodosta OTA-isäntänimi MAC-osoitteesta
// Palauttaa esim. "lorawan-ce0c98"
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

// Pääfunktio: yhdistä WiFiin ja odota OTA-päivitystä
// timeoutSec: montako sekuntia odotetaan ennen luovuttamista
// Palauttaa true jos OTA suoritettiin (laite käynnistyy uudelleen automaattisesti)
// Palauttaa false jos timeout tai WiFi ei toimi
static bool enterOtaMode(uint16_t timeoutSec = OTA_TIMEOUT_SEC) {
  String ssid = nvsCfg.getWifiSSID();
  String pass = nvsCfg.getWifiPass();
  String hostname = buildOtaHostname();

  Serial.printf("\n[OTA] Tila aktiivinen – odotetaan %u s\n", timeoutSec);
  Serial.printf("[OTA] Isäntänimi: %s (FW: %s)\n", hostname.c_str(), FW_VERSION);
  Serial.printf("[OTA] WiFi: %s\n", ssid.c_str());

  // Yhdistä WiFiin
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());

  Serial.print("[OTA] Yhdistetään");
  uint32_t wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifiStart > 20000) {
      Serial.println("\n[OTA] WiFi-yhteys epäonnistui, lopetetaan OTA-tila");
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      return false;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[OTA] Yhdistetty: %s\n", WiFi.localIP().toString().c_str());

  // Konfiguroi ArduinoOTA
  ArduinoOTA.setHostname(hostname.c_str());

  String pwd = String(OTA_PASSWORD);
  if (pwd.length() > 0) {
    ArduinoOTA.setPassword(pwd.c_str());
  }

  // Takaisinkutsut – edistymisseuranta ja virheenkäsittely
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "tiedostojärjestelmä";
    Serial.printf("[OTA] Aloitetaan päivitys: %s\n", type.c_str());
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Päivitys valmis – käynnistetään uudelleen");
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
      case OTA_AUTH_ERROR:    msg = "Autentikointivirhe";   break;
      case OTA_BEGIN_ERROR:   msg = "Aloitusvirhe";         break;
      case OTA_CONNECT_ERROR: msg = "Yhteysvirhe";          break;
      case OTA_RECEIVE_ERROR: msg = "Vastaanotto-virhe";    break;
      case OTA_END_ERROR:     msg = "Lopetusvirhe";         break;
      default:                msg = "Tuntematon virhe";     break;
    }
    Serial.printf("[OTA] Virhe [%u]: %s\n", error, msg);
  });

  ArduinoOTA.begin();

  Serial.println("[OTA] Palvelin käynnissä – avaa Arduino IDE → Portit");
  Serial.printf("[OTA] Odotetaan enintään %u sekuntia...\n", timeoutSec);

  uint32_t otaStart = millis();
  uint32_t lastPrint = 0;
  while (millis() - otaStart < (uint32_t)timeoutSec * 1000) {
    ArduinoOTA.handle();
    
    uint32_t elapsed   = (millis() - otaStart) / 1000;
    uint32_t remaining = timeoutSec - elapsed;
    
    if (elapsed - lastPrint >= 30) {
      lastPrint = elapsed;
      if (remaining > 0) {
        Serial.printf("[OTA] Jäljellä %u s...\n", remaining);
      }
    }
    delay(10);
  }

  // Timeout – palaa normaaliin toimintaan
  Serial.println("[OTA] Timeout – jatketaan normaalia toimintaa");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return false;  // OTA ei suoritettu (muutoin laite on jo käynnistynyt uudelleen)
}
