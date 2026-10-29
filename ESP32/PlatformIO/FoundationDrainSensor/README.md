# LoRaWAN Drain Sensor

Battery-powered, maintenance-free sensor for monitoring foundation drainage pipes.
Triggers an alert when water rises above the alarm threshold. The device communicates
via a LoRaWAN network to a Chirpstack server — no Wi-Fi or mobile network required.

---

## Idea

Drainage pipes keep a building's foundations dry. Under normal conditions, water
flows through them during spring snowmelt and autumn rainfall — this is expected
and requires no action. A problem arises only when a drain becomes blocked and
water begins to rise toward the foundations.

A blockage can develop slowly over years and may not become apparent until
moisture damage has already progressed. The inspection hatch rarely gets opened
more than once a year.

The sensor solves this passively: it wakes once per hour, measures whether water
is present above the alarm threshold inside the inspection hatch, and transmits
the result over the network. In practice, the device is asleep 99.999% of the time.

---

## Hardware

| Component | Model | Notes |
|---|---|---|
| Microcontroller | ESP32-WROOM-32D | Wi-Fi used only for OTA updates |
| LoRa radio | RFM95W-868S2 (SX1276) | EU868, SF7 (DR5) |
| Battery | 3×AA (e.g. Energizer Ultimate Lithium) | Expected life ~2–3 years |
| Voltage regulator | HT7833 3.3 V LDO | Minimum dropout ~100 mV |
| Water sensor | 2 stainless steel electrodes | ~50 mm gap, 1 m lead |
| Gateway | Raspberry Pi 5 + RAK WM1302 | EU868, shared with other nodes |
| Network server | Chirpstack v4 | CLASS_A, OTAA |

### Why 3×AA lithium?

Standard alkaline batteries lose voltage rapidly in the cold (foundations can be
near freezing in winter). Lithium AA cells retain their capacity down to −40 °C
and self-discharge more slowly — making them ideal for this application.

---

## Operating Principle

### Hourly cycle

```
Wake-up (RTC timer)
    │
    ├── Battery voltage measurement (ADC, ~15 ms)
    │
    ├── Water measurement (GPIO pulse, ~10 ms)
    │
    ├── LoRaWAN transmission (OTAA, SF7, ~3 s)
    │       Payload: [water 0/1, battery %]
    │
    └── Deep sleep → next wake-up in 3600 s
```

Total active time is approximately 4 seconds per hour. The rest of the time
the device sleeps at ~20 µA.

### Water measurement — resistive electrode pair

Two stainless steel electrodes are suspended inside the inspection hatch on a
cable. The installation height of the electrodes sets the alarm threshold: when
water rises to that level, it conducts current between the electrodes and the
GPIO detects the state change.

```
IO26 (EXCITE) ──── Electrode 1
                          ~~~~ water conducts ~~~~
IO27 (SENSE)  ──── Electrode 2
                   (INPUT_PULLUP ~45 kΩ → 3.3 V)
```

During measurement IO26 is set HIGH (~10 ms), then returned to INPUT. If water
is present, IO27 is pulled to ground via conductance → LOW. If dry, the pull-up
keeps the line HIGH.

Because the electrodes are unpowered for 3,599.99 seconds of every hour,
electrolysis is negligible — stainless steel will last for years.

### Battery measurement — GPIO-switched voltage divider

```
VBATT ──── 100 kΩ ──┬──── IO34 (BATT_ADC)
                     │
                    100 kΩ
                     │
                    IO32 (BATT_GND, OUTPUT LOW during measurement only)
```

IO32 is set OUTPUT LOW only during measurement, otherwise INPUT. No current
flows through the resistors while the device is sleeping.

The ADC uses 11 dB attenuation, giving a measurement range of 0–3.6 V. Half of
the 3×AA maximum voltage is 2.25 V — well within range with good margin.

---

## Payload (2 bytes)

```
Byte 0:  0x00 = dry
         0x01 = water detected
Byte 1:  battery % (0–100)
```

The Chirpstack decoder also converts the battery percentage to a voltage
(3.6–4.5 V linearly).

---

## Power Consumption Estimate

| State | Current | Duration (per wake-up) |
|---|---|---|
| Deep sleep | ~20 µA | 3596 s |
| Measurements + boot | ~80 mA | ~0.5 s |
| LoRa transmission (SF7) | ~120 mA | ~0.1 s |
| Wi-Fi (OTA update) | ~150 mA | only when needed |

Hourly consumption: ~**0.17 mAh**
3×AA lithium (effective capacity ~3,000 mAh at 3.3 V): **approx. 2–3 years**

---

## Installation

### 1. Electrode

The electrode is made from two stainless steel bolts or rods (M4–M6) mounted
on a small PCB or plastic strip approximately 50 mm apart. Leads are routed from
the electrodes to the device enclosure.

**Installation height:** Mount the electrode at the level where the alarm should
trigger — not at the bottom of the pipe. Recommendation: 20–30 cm above the
normal water level inside the pipe.

### 2. Enclosure

The device is placed in a plastic drainage inspection hatch. Cable penetrations
must be sealed watertight (e.g. cable gland). The electrodes themselves are
intended to be exposed to moisture — the electronics must not be.

### 3. First Boot

1. Edit `src/settings.h`: set `DEFAULT_WIFI_SSID`, `DEFAULT_WIFI_PASS`, `DEFAULT_JOIN_EUI`, `DEFAULT_APP_KEY`
2. Build and flash via USB (port `/dev/ttyUSB0`):
   ```
   pio run --target upload
   ```
3. To read the devEUI, temporarily enable debug output in `src/DrainSensor.ino` (`#define DEBUG true`),
   then open the serial monitor:
   ```
   pio device monitor
   ```
   Reset the device and copy the devEUI printed at 115200 baud.  
   Set `DEBUG` back to `false` before the final production flash.
4. Register the device in Chirpstack (OTAA, CLASS_A, EU868)
5. Subsequent firmware updates via OTA: send downlink `0205` on fPort 1 to trigger a 5-minute OTA window

---

## Chirpstack — Recommended Alert Configuration

Once the device is registered and transmitting data, configure an alert in Chirpstack:

**Device → Alerts → Add alert**

| Field | Value |
|---|---|
| Measurement | `water` |
| Kind | State change |
| Threshold | `true` |
| Notification | Email or webhook |

This way an alert is sent only when the state changes from dry to wet — not on every transmission.

---

## Downlink Commands

| Action | HEX | Description |
|---|---|---|
| Set sleep time 30 min | `010708` | [0x01, 0x07, 0x08] = 1800 s |
| Set sleep time 1 h | `010E10` | [0x01, 0x0E, 0x10] = 3600 s |
| Start OTA (5 min) | `0205` | [0x02, 0x05] |

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `water: true` always | Electrodes short-circuited | Check wiring and electrode gap |
| `water: false` despite water | Electrodes oxidised | Clean or replace electrodes |
| `batt_pct: 0` immediately | Batteries missing or HT7833 not getting voltage | Check batteries and wiring |
| Join fails (−1116) | Device not registered | Register in Chirpstack |
| DevNonce has already been used | EEPROM erased or re-flashed | Chirpstack → device → **Flush OTAA session** |

---

## File Structure

```
FoundationDrain Sensor/
├── platformio.ini           – PlatformIO build config (ESP32-WROOM-32D, /dev/ttyUSB0)
├── src/
│   ├── DrainSensor.ino      – Main sketch
│   ├── settings.h           – Compile-time constants (Wi-Fi, keys, pins)
│   ├── nvs_config.h         – Runtime NVS configuration (sleep time, LoRaWAN keys)
│   ├── ota_update.h         – ArduinoOTA over Wi-Fi
│   ├── battery.h            – Battery measurement (3×AA, ADC 11 dB)
│   └── water_sensor.h       – Water sensor measurement logic
├── Chirpstack/
│   └── chirpstack_decoder.js – Payload decoder for Chirpstack
└── README.md                – This file
```

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

---

*Firmware: v1.0.0 · Chirpstack v4 · ArduinoOTA · PlatformIO · espressif32 · RadioLib ^6.6.0*
