#pragma once
#include <Arduino.h>
#include "settings.h"

// ============================================================
// battery.h – Battery voltage measurement (ESP32-C3 compatible)
// Switched voltage divider: GND via GPIO to eliminate quiescent drain
// ============================================================

#define DEBUG false

// Initialise ADC pin and switch GPIO
static void batteryBegin() {
  pinMode(BATT_ADC_GPIO, INPUT);
  analogSetPinAttenuation(BATT_ADC_GPIO, ADC_ATTENDB_MAX);  // 0-3.3V range
  
  // Init switch GPIO: high-impedance by default (divider OFF)
  pinMode(BATT_GND_SWITCH_GPIO, INPUT);
}

// Enable/disable the voltage divider via GPIO switch
static inline void _battSwitchOn() {
  pinMode(BATT_GND_SWITCH_GPIO, OUTPUT);
  digitalWrite(BATT_GND_SWITCH_GPIO, LOW);  // Connect to GND
}

static inline void _battSwitchOff() {
  // Return to high-impedance to eliminate divider current
  pinMode(BATT_GND_SWITCH_GPIO, INPUT);
}

static float batteryVoltage() {
  // Turn ON divider
  _battSwitchOn();
  delay(BATT_WARMUP_MS);  // Let voltage settle
  
  uint32_t sumMv = 0;
  for (int i = 0; i < BATT_SAMPLES; i++) {
    int raw = analogReadMilliVolts(BATT_ADC_GPIO);
    sumMv += raw;
    delay(5);
  }
  
  // Turn OFF divider IMMEDIATELY after measurements
  _battSwitchOff();
  
  float vadc  = (float)(sumMv / BATT_SAMPLES) / 1000.0f;
  float vbatt = vadc * BATT_DIVIDER_RATIO;

  if (DEBUG) Serial.printf("[batt raw] vadc=%.3fV → vbatt=%.3fV\n", vadc, vbatt);
  return vbatt;
}

// Convert voltage to percentage (0–100)
static uint8_t batteryPercent(float vbatt) {
  if (vbatt <= BATT_MIN_V) return 0;
  if (vbatt >= BATT_MAX_V) return 100;
  float pct = (vbatt - BATT_MIN_V) / (BATT_MAX_V - BATT_MIN_V) * 100.0f;
  return (uint8_t)pct;
}

// Optional: ensure switch is off (call before deep sleep as extra safety)
static void batteryEnd() {
  _battSwitchOff();
}