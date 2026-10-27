/**
 * BME280 + akku – decoder v2 (4-tavu payload)
 *
 * Tavu 0: lämpötila + 40   (0–125  → -40...+85 °C)
 * Tavu 1: kosteus           (0–100 %)
 * Tavu 2: paine – 900       (0–160 → 900–1060 hPa)
 * Tavu 3: akku %            (0–100 %) [UUSI]
 *         → jännite: V = (pct/100) * (4.2 - 3.0) + 3.0
 *
 * Taaksepäin yhteensopiva: 3-tavu payload toimii ilman akku-kenttää.
 */
function decodeUplink(input) {
  if (input.bytes.length < 3) {
    return {
      data: {},
      errors: ['Payload liian lyhyt: odotetaan ≥3 tavua, saatiin ' + input.bytes.length]
    };
  }

  const data = {
    temp: input.bytes[0] - 40,
    hum:  input.bytes[1],
    pres: input.bytes[2] + 900
  };

  // Tavu 3 (akku) – valinnainen, lisätty v1.1.0:ssa
  if (input.bytes.length >= 4) {
    const pct = input.bytes[3];
    data.batt_pct = pct;
    // Lasketaan myös arvioitu jännite referenssiksi (LiPo: 3.0–4.2V)
    data.batt_v = Math.round((pct / 100 * (4.2 - 3.0) + 3.0) * 100) / 100;
  }

  return { data };
}

/**
 * Downlink – komennot laitteelle
 *
 * Komento 0x01: muuta uni-aika
 *   Lähetä: [0x01, sekunnit_high, sekunnit_low]
 *   Esim. 300s (5 min):  [0x01, 0x01, 0x2C]
 *   Esim. 600s (10 min): [0x01, 0x02, 0x58]
 *
 * Komento 0x02: käynnistä OTA-tila (ArduinoOTA WiFin kautta)
 *   [0x02]       → 5 min timeout (oletus)
 *   [0x02, 0x0A] → 10 min timeout
 *   Laite yhdistää WiFiin ja odottaa Arduino IDE -päivitystä.
 *
 * Komento 0x03: tyhjennä pin-välimuisti (pakottaa uudelleentunnistuksen)
 *   [0x03]
 */
function encodeDownlink(input) {
  // Tänne tulee Chirpstackin "Enqueue downlink" -lomakkeelta
  // Palautetaan tyhjä bytes – käyttäjä täyttää itse haluamansa komennon
  return {
    bytes: [],
    fPort: 1
  };
}
