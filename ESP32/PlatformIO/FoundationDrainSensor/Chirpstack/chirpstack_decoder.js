// Chirpstack payload decoder – DrainSensor
// Payload: 2 tavua [water, batt_pct]

function decodeUplink(input) {
  if (input.bytes.length < 2) {
    return { data: {}, errors: ['Payload liian lyhyt, odotetaan 2 tavua'] };
  }

  var batt_pct = input.bytes[1];
  // LiPo/AA: muunnetaan prosentti jännitteeksi
  // BATT_MIN_V=3.6, BATT_MAX_V=4.5 (3×AA)
  var batt_v = parseFloat((batt_pct / 100.0 * (4.5 - 3.6) + 3.6).toFixed(2));

  return {
    data: {
      water:    input.bytes[0] === 0x01,
      batt_pct: batt_pct,
      batt_v:   batt_v
    }
  };
}

function encodeDownlink(input) {
  return { bytes: [], fPort: 1 };
}
