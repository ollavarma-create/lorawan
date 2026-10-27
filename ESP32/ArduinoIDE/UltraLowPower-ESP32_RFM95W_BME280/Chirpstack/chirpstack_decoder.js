/**
 * BME280 binääridekooderi – 3-tavu formaatti
 *   Tavu 0: lämpötila + 40  (0–125 → -40°C...+85°C)
 *   Tavu 1: kosteus          (0–100 %)
 *   Tavu 2: paine - 900      (0–160 → 900–1060 hPa)
 */
function decodeUplink(input) {
  if (input.bytes.length < 3) {
    return { data: {}, errors: ['Payload liian lyhyt, odotetaan 3 tavua, saatiin ' + input.bytes.length] };
  }
  return {
    data: {
      temp: input.bytes[0] - 40,
      hum:  input.bytes[1],
      pres: input.bytes[2] + 900
    }
  };
}

/**
 * Downlink – sleep-ajan muutos (ei pakollinen)
 * Lähetä: [0x01, sekunnit_high, sekunnit_low]
 * Esim. 600s (10 min): [0x01, 0x02, 0x58]
 */
function encodeDownlink(input) {
  return {
    bytes: [],
    fPort: 1
  };
}
