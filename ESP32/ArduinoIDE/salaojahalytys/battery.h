#pragma once
#include <Arduino.h>
#include "settings.h"

// ============================================================
// battery.h – Akun/paristojännitteen mittaus
//
// Kytkentä (settings.h:ssa määritelty):
//   VBATT ── R1(100k) ──┬── BATT_ADC_GPIO
//                        │
//                       R2(100k)
//                        │
//                     BATT_GND_GPIO (OUTPUT LOW = mittaus päällä)
//
// BATT_GND_GPIO INPUT = ei virtaa (deep sleep -tila)
// ============================================================

static void batteryBegin() {
  pinMode(BATT_ADC_GPIO, INPUT);
  pinMode(BATT_GND_GPIO, INPUT);
  // 11dB attenuaatio: ADC-alue 0–3.6V
  // Ilman tätä alue on vain 0–1.1V → 3×AA (max 2.25V jaettuna) saturoi
  analogSetPinAttenuation(BATT_ADC_GPIO, ADC_11db);
}

static float batteryVoltage() {
  pinMode(BATT_GND_GPIO, OUTPUT);
  digitalWrite(BATT_GND_GPIO, LOW);
  delay(BATT_WARMUP_MS);

  uint32_t sum = 0;
  for (int i = 0; i < BATT_SAMPLES; i++) {
    sum += analogRead(BATT_ADC_GPIO);
    delay(1);
  }
  uint32_t raw = sum / BATT_SAMPLES;

  pinMode(BATT_GND_GPIO, INPUT);

  float vadc  = (float)raw / BATT_ADC_MAX * 3.6f;  // 3.6V = 11dB full scale
  float vbatt = vadc * BATT_DIVIDER_RATIO;
  return vbatt;
}

static uint8_t batteryPercent(float vbatt) {
  if (vbatt <= BATT_MIN_V) return 0;
  if (vbatt >= BATT_MAX_V) return 100;
  return (uint8_t)((vbatt - BATT_MIN_V) / (BATT_MAX_V - BATT_MIN_V) * 100.0f);
}

static void batteryEnd() {
  pinMode(BATT_GND_GPIO, INPUT);
}
