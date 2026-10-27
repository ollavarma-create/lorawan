# MQTT Chirpstack -integraatio – LoRaWAN data InfluxDB:hen

Tama ohje kattaa Chirpstack v4 MQTT-integraation Telegrafillla InfluxDB v2:een.
Data kulkee: ESP32-node -> LoRa -> Gateway -> Chirpstack -> MQTT -> Telegraf -> InfluxDB

---

## Arkkitehtuuri

```
ESP32 + BME280  (3-tavu LoRaWAN-payload)
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
Mosquitto MQTT broker (localhost:1893, kayttaja: chirpstack_gw)
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

## Esivalmistelut

### Mosquitto – Chirpstack-kayttaja

Lisaa Chirpstackille oma kayttaja Mosquittoon:

```bash
sudo mosquitto_passwd /etc/mosquitto/passwd chirpstack_gw
# Syota salasana kahdesti kehotteessa
sudo systemctl restart mosquitto
```

Varmista etta Mosquitto kuuntelee oikeassa portissa.
Tarkista `/etc/mosquitto/mosquitto.conf` tai `/etc/mosquitto/conf.d/`:

```
listener 1893
password_file /etc/mosquitto/passwd
```

### Chirpstack – MQTT-integraatio

Chirpstack julkaisee uplinkit automaattisesti MQTT:hen. Varmista etta
integraatio on paalla: **Chirpstack UI -> Network -> Gateway -> MQTT**.

Topic-rakenne (Chirpstack v4):
```
application/{applicationID}/device/{devEUI}/event/up
```

Testaa etta viesteja tulee:
```bash
mosquitto_sub -h localhost -p 1893 -u chirpstack_gw -P salasana \
  -t "application/#" -v
```

Kun laite lahettaa, terminaaliin ilmestyy JSON-viesti.

### InfluxDB v2 – API token

1. Avaa InfluxDB UI: http://localhost:8086
2. Valitse **Data -> API Tokens -> Generate API Token**
3. Anna token kirjoitusoikeus buckettiin `databuckettisi nimi`
4. Kopioi token talteen – se nakyy vain kerran

Tarkista organisaation tarkka nimi (isot/pienet kirjaimet merkitsevat!):
```bash
influx org list
```

---

## Telegraf-konfiguraatio

Tallenna tiedosto `/etc/telegraf/telegraf.d/chirpstack.conf`.
Korvaa `SINUN_SALASANA` ja `SINUN_TOKEN` oikeilla arvoilla.

```toml
[[inputs.mqtt_consumer]]
  servers = ["tcp://localhost:1893"]
  username = "chirpstack_gw"
  password = "SINUN_SALASANA"

  # Chirpstack v4 topic-rakenne
  topics = ["application/+/device/+/event/up"]

  data_format = "json_v2"
  name_override = "lorawan"

  [[inputs.mqtt_consumer.json_v2]]

    # Aikaleima Chirpstackin omasta kentasta
    timestamp_path = "time"
    timestamp_format = "2006-01-02T15:04:05.999999999Z07:00"

    # Tagit – indeksoitu, tehokas suodatus Grafanassa
    [[inputs.mqtt_consumer.json_v2.tag]]
      path = "deviceInfo.devEui"
      rename = "dev_eui"

    [[inputs.mqtt_consumer.json_v2.tag]]
      path = "deviceInfo.deviceName"
      rename = "device_name"

    [[inputs.mqtt_consumer.json_v2.tag]]
      path = "deviceInfo.applicationName"
      rename = "application"

    [[inputs.mqtt_consumer.json_v2.tag]]
      path = "deviceInfo.tags.location"
      rename = "location"
      optional = true

    # Sensoriarvo: lampotila
    [[inputs.mqtt_consumer.json_v2.field]]
      path = "object.temp"
      rename = "temperature"
      type = "float"
      optional = true

    # Sensoriarvo: kosteus
    [[inputs.mqtt_consumer.json_v2.field]]
      path = "object.hum"
      rename = "humidity"
      type = "float"
      optional = true

    # Sensoriarvo: ilmanpaine
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
      optional = true

    # Battery percentage
    [[inputs.mqtt_consumer.json_v2.field]]
      path = "object.batt_pct"
      rename = "battery_pct"
      type = "float"
      optional = true

    # Radiolinkin laatu
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

    # Data rate (SF7=5, SF9=3 jne.)
    [[inputs.mqtt_consumer.json_v2.field]]
      path = "dr"
      rename = "data_rate"
      type = "int"
      optional = true

[[outputs.influxdb_v2]]
  urls = ["http://localhost:8086"]
  token = "SINUN_TOKEN"
  organization = "Home"
  bucket = "kotidata"
```

---

## Kaynnistys

### Testaa ennen kaynnistystä

```bash
telegraf --config /etc/telegraf/telegraf.d/chirpstack.conf --test
```

Odotettu tulos:
```
I! [inputs.mqtt_consumer] Connected [tcp://localhost:1893]
```

Ei virheita = konfiguraatio OK.

### Kaynnista Telegraf

```bash
sudo systemctl restart telegraf
sudo systemctl status telegraf
```

### Seuraa lokia

```bash
sudo journalctl -u telegraf -f
```

Kun laite lahettaa seuraavan uplinkin, loki on hiljainen – ei virheita
tarkoittaa onnistunutta kirjoitusta InfluxDB:hen.

---

## Datan tarkistus InfluxDB:ssa

### CLI

```bash
influx query 'from(bucket:"kotidata")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "lorawan")'
```

### Grafana – esimerkkiqueryt

**Lampotila:**
```flux
from(bucket: "kotidata")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "lorawan")
  |> filter(fn: (r) => r["_field"] == "temperature")
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
  |> yield(name: "mean")
```

**Kosteus:**
```flux
from(bucket: "kotidata")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "lorawan")
  |> filter(fn: (r) => r["_field"] == "humidity")
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
  |> yield(name: "mean")
```

**Ilmanpaine:**
```flux
from(bucket: "kotidata")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "lorawan")
  |> filter(fn: (r) => r["_field"] == "pressure")
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
  |> yield(name: "mean")
```

**Laitekohtainen suodatus** (kun useita laitteita):
```flux
  |> filter(fn: (r) => r["dev_eui"] == "a0f262fffea3edb8")
```

**Sijainnin mukaan:**
```flux
  |> filter(fn: (r) => r["location"] == "koti")
```

---

## Vianmaaritys

| Ongelma | Syy | Ratkaisu |
|---|---|---|
| `organization not found` | Iso/pieni kirjain vaarin | `influx org list` -> tarkista tarkka nimi |
| `Connected` mutta ei dataa | Laite nukkuu | Odota 10 min tai resetoi laite |
| `connection refused 1893` | Mosquitto ei kuuntele portissa | Tarkista mosquitto.conf |
| `not authorized` | Vaaraa tunnuksia | Tarkista kayttaja/salasana |
| Kentat puuttuvat InfluxDB:sta | JSON-polku vaarin | `mosquitto_sub` -> tarkista JSON-rakenne |
| `object.temp` ei loydy | Decoder ei ole paalla | Chirpstack: Device profile -> Codec -> tarkista decoder |

---

## Huomioita

- Chirpstack v4 muutti topic-rakennetta v3:een verrattuna – varmista oikea versio
- `location`-tagi tulee Chirpstackin laitteen Tags-kentasta (key: location)
- RSSI ja SNR tallentuvat automaattisesti – hyodyllinen signaalin seurantaan
- Uusi laite nakyy automaattisesti InfluxDB:ssa oman `dev_eui`-tagin alla
