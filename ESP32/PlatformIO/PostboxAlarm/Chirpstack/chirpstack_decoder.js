function decodeUplink(input) {
  var bytes = input.bytes;
  
  if (bytes.length < 2) {
    return { data: { error: "payload too short" } };
  }

  // Parse battery percentage (sent directly from ESP32)
  var battery_pct = bytes[1];
  
  // ✅ KORJAUS: Käytä samaa 1.2V aluetta kuin ESP32:ssä (3.0V–4.2V)
  var pct = battery_pct / 100.0;
  var battery_v = parseFloat((3.0 + pct * 1.2).toFixed(2));  // ← 1.35 → 1.2

  // Event parsing
  var event, mail_present;
  switch (bytes[0]) {
    case 0x01:
      event = "mail";
      mail_present = true;
      break;
    case 0x02:
      event = "heartbeat";
      mail_present = false;
      break;
    default:
      event = "unknown";
      mail_present = null;
  }

  return {
    data: {
      // Telegraf/Grafana-yhteensopivat kentät
      batt_v: battery_v,
      batt_pct: battery_pct,
      event: event,
      mail_present: mail_present,
      
      // Aliakset dashboard-yhteensopivuuteen
      battery: battery_v,
      batteryLevel: battery_pct,
      battery_voltage: battery_v
    }
  };
}
