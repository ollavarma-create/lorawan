#pragma once
#include <Arduino.h>
#include "settings.h"

// ============================================================
// water_sensor.h – Resistiivinen vesisensoori
//
// Kytkentä:
//   WATER_EXCITE_GPIO ──── Elektrodi 1
//                                   ~~~ (vesi johtaa) ~~~
//   WATER_SENSE_GPIO  ──── Elektrodi 2
//
//   Sisäinen pull-up (~45kΩ) pitää SENSE-linjan ylhäällä
//   kun elektrodien välissä ei ole vettä.
//
// Toimintaperiaate:
//   1. EXCITE: OUTPUT HIGH (~10 ms)
//   2. SENSE luetaan:
//      LOW  = vettä (virta kulkee elektrodien kautta)
//      HIGH = kuiva (pull-up voittaa)
//   3. EXCITE: INPUT (ei virtaa lepotilassa)
//
// Elektrolyysi:
//   Herätyksiä 24 krt/vrk × 10 ms = 240 ms/vrk virtaa.
//   Ruostumaton teräs tai tinattomaton PCB-kupari kestää
//   vuosia tällä rasituksella.
// ============================================================

static void waterSensorBegin() {
  pinMode(WATER_EXCITE_GPIO, INPUT);        // Oletustila: ei virtaa
  pinMode(WATER_SENSE_GPIO,  INPUT_PULLUP); // Sisäinen pull-up päälle
}

static bool waterDetected() {
  // Aktivoi herätys
  pinMode(WATER_EXCITE_GPIO, OUTPUT);
  digitalWrite(WATER_EXCITE_GPIO, HIGH);
  delay(10);  // Anna jännitteen vakiintua

  bool wet = (digitalRead(WATER_SENSE_GPIO) == LOW);

  // Sammuta välittömästi – minimoi elektrolyysi
  pinMode(WATER_EXCITE_GPIO, INPUT);

  return wet;
}

static void waterSensorEnd() {
  pinMode(WATER_EXCITE_GPIO, INPUT);
  pinMode(WATER_SENSE_GPIO,  INPUT);  // Poista pull-up ennen deep sleepiä
}
