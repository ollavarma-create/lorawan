# MQTT Chirpstack Integration — LoRaWAN Data to InfluxDB

This guide covers the Chirpstack v4 MQTT integration with Telegraf into InfluxDB v2.
Data flow: ESP32 node → LoRa → Gateway → Chirpstack → MQTT → Telegraf → InfluxDB

---

## Architecture

```
ESP32 + BME280  (3-byte LoRaWAN payload)
      |
      | LoRa RF (EU868, SF7, DR5)
      v
RPi5 + RAK WM1302 gateway
      |
      | SPI
      v
Chirpstack v4 (Docker, localhost:8080)
      |
      | MQTT publish
      v
Mosquitto MQTT broker (localhost:1893, user: chirpstack_gw)
      |
      | mqtt_consumer plugin
      v
Telegraf 1.37+
      |
      | influxdb_v2 output
      v
InfluxDB v2 (localhost:8086, org: Home, bucket: kotidata)
```

---

## Prerequisites

### Mosquitto — Chirpstack User

Add a dedicated user for Chirpstack in Mosquitto:

```bash
sudo mosquitto_passwd /etc/mosquitto/passwd chirpstack_gw
# Enter the password twice at the prompt
sudo systemctl restart mosquitto
```

Verify that Mosquitto is listening on the correct port.
Check `/etc/mosquitto/mosquitto.conf` or `/etc/mosquitto/conf.d/`:

```
listener 1893
password_file /etc/mosquitto/passwd
```

### Chirpstack — MQTT Integration

Chirpstack publishes uplinks to MQTT automatically. Verify that the integration
is enabled: **Chirpstack UI → Network → Gateway → MQTT**.

Topic structure (Chirpstack v4):
```
application/{applicationID}/device/{devEUI}/event/up
```

Test that messages are arriving:
```bash
mosquitto_sub -h localhost -p 1893 -u chirpstack_gw -P your_password \
  -t "application/#" -v
```

When the device transmits, a JSON message appears in the terminal.

## Device Naming & Grafana Filtering Best Practices
### Why naming matters
Chirpstack's `deviceInfo.deviceName` is automatically mapped to the `device_name` tag in InfluxDB. Using clear, consistent device names instead of raw `dev_eui` or `applicationID` values brings major benefits:
- **Readable queries**: `r["device_name"] == "Ulkosensori"` is easier to maintain than long hex IDs.
- **Reusable dashboards**: Build one panel template and switch devices via Grafana variables.
- **Stable alerting**: Alert rules break less often when names stay consistent across firmware updates or hardware swaps.
- **Troubleshooting**: Logs, tickets, and documentation align with the UI.

### Recommended naming convention
`Location_Function_SensorType`  
Examples: `Ulkosensori`, `Kellari_Kosteus_BME280`, `Huone1_Lampo_DS18B20`

### How to set it in Chirpstack
1. Open Chirpstack UI → Devices → Select device
2. Edit → Device name → Enter your chosen name
3. Save → Next uplink will carry the new name in `deviceInfo.deviceName`

Telegraf already extracts this field:
```toml
[[inputs.mqtt_consumer.json_v2.tag]]
  path = "deviceInfo.deviceName"
  rename = "device_name"
```

### InfluxDB v2 — API Token

1. Open the InfluxDB UI: http://localhost:8086
2. Go to **Data → API Tokens → Generate API Token**
3. Grant the token write access to your bucket
4. Copy the token — it is shown only once

Verify the exact organisation name (case-sensitive!):
```bash
influx org list
```

---

## Telegraf Configuration

Save the file to `/etc/telegraf/telegraf.d/chirpstack.conf`.
Replace `YOUR_PASSWORD` and `YOUR_TOKEN` with the actual values.

```toml
[[inputs.mqtt_consumer]]
  servers = ["tcp://localhost:1893"]
  username = "chirpstack_gw"
  password = "YOUR_PASSWORD"

  # Chirpstack v4 topic structure
  topics = ["application/+/device/+/event/up"]

  data_format = "json_v2"
  name_override = "lorawan"

  [[inputs.mqtt_consumer.json_v2]]

    # Timestamp from Chirpstack's own field
    timestamp_path = "time"
    timestamp_format = "2006-01-02T15:04:05.999999999Z07:00"

    # Tags — indexed, efficient filtering in Grafana
    [[inputs.mqtt_consumer.json_v2.tag]]
      path = "deviceInfo.devEui"
      rename = "dev_eui"

    [[inputs.mqtt_consumer.json_v2.tag]]
      path = "deviceInfo.deviceName"
      rename = "device_name"

    [[inputs.mqtt_consumer.json_v2.tag]]
      path = "deviceInfo.applicationName"
      rename = "application"

    # Optional: requires "location" key set in ChirpStack device Tags
    [[inputs.mqtt_consumer.json_v2.tag]]
      path = "deviceInfo.tags.location"
      rename = "location"
      optional = true

    # Sensor value: temperature
    [[inputs.mqtt_consumer.json_v2.field]]
      path = "object.temp"
      rename = "temperature"
      type = "float"
      optional = true

    # Sensor value: humidity
    [[inputs.mqtt_consumer.json_v2.field]]
      path = "object.hum"
      rename = "humidity"
      type = "float"
      optional = true

    # Sensor value: air pressure
    [[inputs.mqtt_consumer.json_v2.field]]
      path = "object.pres"
      rename = "pressure"
      type = "float"
      optional = true

    # Battery voltage
    [[inputs.mqtt_consumer.json_v2.field]]
      path = "object.batt_v"
      rename = "battery_voltage"
      type = "float"
      optional = true

    # Battery percentage
    [[inputs.mqtt_consumer.json_v2.field]]
      path = "object.batt_pct"
      rename = "battery_pct"
      type = "float"
      optional = true

    # Radio link quality
    [[inputs.mqtt_consumer.json_v2.field]]
      path = "rxInfo.0.rssi"
      rename = "rssi"
      type = "int"
      optional = true

    [[inputs.mqtt_consumer.json_v2.field]]
      path = "rxInfo.0.snr"
      rename = "snr"
      type = "float"
      optional = true

    # Data rate (SF7=5, SF9=3, etc.)
    [[inputs.mqtt_consumer.json_v2.field]]
      path = "dr"
      rename = "data_rate"
      type = "int"
      optional = true

[[outputs.influxdb_v2]]
  urls = ["http://localhost:8086"]
  token = "YOUR_TOKEN"
  organization = "Home"
  bucket = "kotidata"
```

---

## Starting Up

### Test Before Starting

```bash
telegraf --config /etc/telegraf/telegraf.d/chirpstack.conf --test
```

Expected output:
```
I! [inputs.mqtt_consumer] Connected [tcp://localhost:1893]
```

No errors = configuration is valid.

### Start Telegraf

```bash
sudo systemctl restart telegraf
sudo systemctl status telegraf
```

### Follow the Log

```bash
sudo journalctl -u telegraf -f
```

When the device sends its next uplink, the log stays quiet — no errors means
data was written to InfluxDB successfully.

---

## Verifying Data in InfluxDB

### CLI

```bash
influx query 'from(bucket:"kotidata")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "lorawan")'
```

### Grafana — Example Queries

**Temperature:**
```flux
from(bucket: "kotidata")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "lorawan")
  |> filter(fn: (r) => r["_field"] == "temperature")
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
  |> yield(name: "mean")
```

**Humidity:**
```flux
from(bucket: "kotidata")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "lorawan")
  |> filter(fn: (r) => r["_field"] == "humidity")
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
  |> yield(name: "mean")
```

**Air Pressure:**
```flux
from(bucket: "kotidata")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "lorawan")
  |> filter(fn: (r) => r["_field"] == "pressure")
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
  |> yield(name: "mean")
```

**Battery Voltage:**
```flux
from(bucket: "kotidata")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "lorawan")
  |> filter(fn: (r) => r["_field"] == "battery_voltage")
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
  |> yield(name: "mean")
```

**Battery Percentage:**
```flux
from(bucket: "kotidata")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "lorawan")
  |> filter(fn: (r) => r["_field"] == "battery_pct")
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
  |> yield(name: "mean")
```

**Filter by device** (when multiple devices are present):
```flux
  |> filter(fn: (r) => r["dev_eui"] == "a0f262fffea3edb8")
```

**Filter by location:**
```flux
  |> filter(fn: (r) => r["location"] == "koti")
```

# Grafana Alerting — Battery Voltage Example

Grafana v10/v11 uses Unified Alerting. InfluxDB + Flux works natively with alert rules.

1. Create an alert-optimized query
Alerts run on a schedule, not dashboard time ranges. Use a fixed window and return a single value:

from(bucket: "kotidata")
  |> range(start: -5m)
  |> filter(fn: (r) => r["_measurement"] == "lorawan")
  |> filter(fn: (r) => r["_field"] == "battery_voltage")
  |> filter(fn: (r) => r["device_name"] == "Ulkosensori")
  |> last()


2. Configure the alert rule in Grafana

    Open your panel → Alert tab → Create alert rule
    Query: Paste the Flux query above
    Condition: WHEN last() OF query(A, 5m, now) IS BELOW 3.0
        Adjust threshold (e.g., 3.0 V for Li-ion/LiPo warning)
    Evaluation behavior: Every 1m for 3 times (avoids false positives on missed uplinks)
    Labels: Add sensor="Ulkosensori", type="battery"
    Annotations: Summary {{ $labels.sensor }} akku jännite {{ $values.B.Value }} V, Description Tarkista sensori tai vaihda paristo.
    Contact points: Select email, Slack, webhook, etc.
    Save & test: Use Rule evaluation to verify triggers.

Alerting tips for InfluxDB + LoRaWAN

    LoRaWAN devices sleep → use IS BELOW X FOR 10m to tolerate missed uplinks.
    Avoid v.windowPeriod in alerts; use fixed durations (-5m, -15m).
    Reuse the same device_name filter in alerts as in dashboards for consistency.
    Test thresholds with historical data: range(start: -24h) |> filter(...) |> yield()
---

## Troubleshooting

| Problem | Cause | Fix |
|---|---|---|
| `organization not found` | Wrong letter case | `influx org list` → verify exact name |
| `Connected` but no data | Device is sleeping | Wait 10 min or reset the device |
| `connection refused 1893` | Mosquitto not listening on port | Check `mosquitto.conf` |
| `not authorized` | Wrong credentials | Check username and password |
| Fields missing in InfluxDB | Wrong JSON path | `mosquitto_sub` → inspect JSON structure |
| `object.temp` not found | Decoder not enabled | Chirpstack: Device profile → Codec → check decoder |
| `path "deviceInfo.tags.location" doesn't exist` | location-tag missing from ChirpStack device settings | Add tag in ChirpStack (Device → Tags → `location`) **or** add `optional = true` to the field |
| `No metrics were created from a message` | Required field missing from payload | Add `optional = true` to all `object.*` fields |

### Monitoring MQTT Messages

Inspect the raw JSON payload arriving from ChirpStack (wait for the device to send an uplink):

```bash
mosquitto_sub -h localhost -p 1893 \
  -u chirpstack_gw -P YOUR_PASSWORD \
  -t "application/+/device/+/event/up" | python3 -m json.tool
```

Check the `object` block for the exact field names used by your codec.

---

## Notes

- Chirpstack v4 changed the topic structure compared to v3 — verify you are using the correct version
- The `location` tag comes from the device Tags field in Chirpstack (key: `location`)
- RSSI and SNR are stored automatically — useful for monitoring signal quality
- A new device appears automatically in InfluxDB under its own `dev_eui` tag

---

## License

MIT License

Copyright (c) 2026 Jari Hiltunen 

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
