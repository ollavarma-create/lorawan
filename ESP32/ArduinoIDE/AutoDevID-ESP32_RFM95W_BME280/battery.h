#pragma once
#include <Arduino.h>
#include "settings.h"

// ============================================================
// battery.h – Akun jännitteen mittaus
//
// Kytkentä (settings.h:ssa määritelty):
//   VBATT ─── R_top(100k) ─┬─ BATT_ADC_GPIO
//                           └─ R_bot(100k) ─ BATT_GND_GPIO
//
// BATT_GND_GPIO LOW  = divisori aktiivinen, ADC lukee Vbatt/2
// BATT_GND_GPIO INPUT = ei kulutusta (deep sleep -tila)
//
// Payload-tavu:
//   batteryPercent() palauttaa 0–100, tallennetaan payloadin tavuun 3.
//   Decoder laskee takaisin jännitteen: V = (pct/100)*(4.2-3.0)+3.0
// ============================================================

// Alusta akkumittaus – aseta GPIO-tilat
// Kutsu kerran setup():ssa ENNEN mittausta
static void batteryBegin() {
  pinMode(BATT_ADC_GPIO, INPUT);
  pinMode(BATT_GND_GPIO, INPUT);
  analogSetPinAttenuation(BATT_ADC_GPIO, ADC_11db);
}

// Mittaa akun jännite volttia
// Kytkee GPIO LOWiksi, odottaa latausta, mittaa, palauttaa INPUT-tilaan
static float batteryVoltage() {
  // Kytke divisori maahan
  pinMode(BATT_GND_GPIO, OUTPUT);
  digitalWrite(BATT_GND_GPIO, LOW);
  delay(BATT_WARMUP_MS);  // Anna kondensaattorin latautua

  // Ota useita lukuja ja keskiarvista kohinan poistamiseksi
  uint32_t sum = 0;
  for (int i = 0; i < BATT_SAMPLES; i++) {
    sum += analogRead(BATT_ADC_GPIO);
    delay(1);
  }
  uint32_t raw = sum / BATT_SAMPLES;

  // Sammuta divisori heti mittauksen jälkeen
  pinMode(BATT_GND_GPIO, INPUT);

  // ESP32 ADC: 12-bit, 0–4095 vastaa 0–3.3V (Vref)
  // Jännite divisorin jälkeen: Vadc = raw / 4095 * 3.3
  // Todellinen akkujännite: Vbatt = Vadc * BATT_DIVIDER_RATIO
  float vadc = (float)raw / BATT_ADC_MAX * 3.3f;
  float vbatt = vadc * BATT_DIVIDER_RATIO;

  return vbatt;
}

// Muunna jännite prosenteiksi (0–100)
// LiPo: 3.0V = 0%, 4.2V = 100%
static uint8_t batteryPercent(float vbatt) {
  if (vbatt <= BATT_MIN_V) return 0;
  if (vbatt >= BATT_MAX_V) return 100;
  float pct = (vbatt - BATT_MIN_V) / (BATT_MAX_V - BATT_MIN_V) * 100.0f;
  return (uint8_t)pct;
}

// Palauta kaikki GPIO:t turvalliseen tilaan ennen deep sleepiä
// INPUT = ei virtaa R_bot:n läpi, ei akun tyhjennystä
static void batteryEnd() {
  pinMode(BATT_GND_GPIO, INPUT);
}
