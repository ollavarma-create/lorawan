# LoRaWAN Project

## Changelog

**12 Feb 2026:**
- Initial commit

**13 Feb 2026**
- Added variables for the ESP32 SuperMini development board pin layout to the Arduino code. Comment out in the code whether you are using DevBoardM or SuperMini. If you are developing on a different platform, remember to verify working pins/GPIOs (the code uses GPIO numbers as printed on the PCB).
- Added screenshots under `_images`

**15 Feb 2026**
- Added JavaScript that decodes the device payload for temp, hum, and pres
- Added example images of the setup under the Chirpstack path for reference

**03 Mar 2026**
- Arduino code rewritten as an Ultra Low Power (ULP) implementation
  - BME280 forced mode: measurement → automatic sleep (~0.1 µA standby)
  - Radio fully shut down before deep sleep (`radio.sleep()` instead of the previous `standby()`)
  - SPI and I2C shut down before deep sleep
  - ADR disabled, DR5 (SF7) forced for shorter airtime and lower power consumption
- JSON payload (~40 bytes) replaced by a 3-byte binary payload
  - Byte 0: temperature + 40 (covers −40…+85 °C)
  - Byte 1: humidity 0–100 %
  - Byte 2: pressure − 900 (covers 900–1060 hPa)
- DevEUI generated automatically from the ESP32 MAC address (IEEE EUI-64)
  - The same firmware can be flashed to all devices without modifying the devEUI
  - DevEUI is read from the serial monitor and registered in Chirpstack
- Board selection with a single `#define` line (`BOARD_SUPERMINI` / `BOARD_DEVBOARD`)
- Chirpstack payload decoder updated to match the new binary format
- Added `DEVICE_REGISTRATION.md` — comprehensive guide to registering a new device
- Added instructions for integrating MQTT with InfluxDB (via Telegraf) and Grafana
- Added KiCad schematics for the Ultra Low Power setup

**18 Mar 2026**
- Arduino code refactored into a modular multi-platform structure for ESP32 (v1.1.0)
  - `settings.h` — all compile-time constants and defaults in one place (Wi-Fi, keys, pins, battery)
  - `nvs_config.h` — runtime NVS configuration: sleep time, Wi-Fi, pins, and LoRaWAN keys persist in flash across deep sleep cycles
  - `pin_probe.h` — automatic I2C and SPI pin detection from candidate lists; result saved to NVS so probing is done only once
  - `battery.h` — battery voltage measurement via GPIO-controlled voltage divider (100k+100k); GPIO set to INPUT during sleep → zero current draw
  - `ota_update.h` — ArduinoOTA over Wi-Fi; triggered by downlink command
- Payload extended from 3 to 4 bytes
  - Byte 3: battery % (0–100), voltage calculated in the decoder (LiPo: 3.0–4.2 V)
- Chirpstack decoder updated to v2 (`chirpstack_decoder_v2.js`), backwards compatible with 3-byte payloads
- OTA update via ArduinoOTA over Wi-Fi
  - Triggered by downlink `[0x02, timeout_min]` from Chirpstack
  - Device sets an RTC flag, wakes up normally, connects to Wi-Fi, and waits for update
  - Hostname derived automatically from MAC: `lorawan-XXXX` (visible in Arduino IDE Ports menu)
  - Recommended method on Linux: `espota.py -i <ESP-IP> -p 3232 -a <host-IP> -f firmware.bin`
  - Partition scheme: **Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)** is required
- Downlink command structure (fPort=1):
  - `[0x01, high, low]` → change sleep time in seconds
  - `[0x02]` / `[0x02, min]` → start OTA mode
  - `[0x03]` → clear pin cache (forces a new probe)
- Added `ota_manager.py` — Python script for device management with MQTT monitoring and downlink delivery via Chirpstack REST API
- Added `OTA-UPDATE.md` — comprehensive guide from first flash to OTA update, including HEX command reference tables and common troubleshooting

**20 Mar 2026**
- Fixed DevNonce bug: nonces are now ALWAYS loaded from EEPROM before OTAA activation
  - Previously nonces were loaded only if `isJoined = true` (RTC memory had survived)
  - RTC memory is cleared on full power loss (battery depleted or disconnected) → device restarted nonce counter from zero → Chirpstack rejected the join with `DevNonce has already been used`
  - After this fix, power loss no longer causes join failures
  - Session is still restored only if RTC memory survived (deep sleep without power loss)

**21 Apr 2026 — PlatformIO migration and custom PCB bring-up**

- All source files moved to `ESP32/PlatformIO/src/` (PlatformIO requires sources under `src/`; Arduino IDE compiled from the project root)
- Added `static void clearNonces();` forward declaration in the main `.cpp` file — Arduino IDE auto-generates these, PlatformIO does not
- `platformio.ini` created with `board_build.partitions = min_spiffs.csv` and `upload_speed = 115200` (required for reliable upload on custom PCB)
- **Custom PCB boot mode issues found and fixed during bring-up:**
  - GPIO0 needs a 10 kΩ pull-up to 3.3 V for normal boot
  - GPIO2 has an internal pull-down and must NOT have an external pull-up — download mode requires GPIO0=LOW AND GPIO2=LOW simultaneously; adding a pull-up to GPIO2 caused the chip to enter `HSPI_FLASH_BOOT` instead of download mode
  - Manual download procedure: pull IO0 to GND, pulse EN to GND, start upload, release IO0 after upload completes
  - PCB next revision: add BOOT button (IO0→GND) and RESET button (EN→GND)
- **Bug fix: `RADIOLIB_ERR_NETWORK_NOT_JOINED` (-1101) after deep sleep**
  - Root cause: `setBufferSession()` and `setBufferNonces()` restore data buffers but do not activate RadioLib's internal session state; `activateOTAA()` must always be called — when both buffers are pre-loaded it returns `RADIOLIB_LORAWAN_SESSION_RESTORED` immediately without any over-the-air join
  - Fix: removed the `if (!isJoined)` guard around `performActivation()` so `activateOTAA()` is always called regardless of RTC session state
- **Battery ADC calibration:** replaced `analogRead()` + manual linear conversion with `analogReadMilliVolts()`, which uses ESP32's built-in eFuse ADC calibration (result: 4.14 V measured vs 4.09 V on a Fluke, 50 mV / 1.2 % error — no manual calibration factor needed)
- `OTA-UPDATE-ESP32_RFM95W_BME280.md` rewritten for PlatformIO: binary path, serial monitor, and upload commands updated; boot mode section added for custom PCB

**28 Apr 2026**
- Added Grafana dashboard and alert rule example screenshots under `_images/Grafana/`
- Added InfluxDB measurement and tag configuration screenshots under `_images/InfluxDB/`
- Moved old Finnish-language documentation to `Chirpstack/Finnish (old versions)/`
- Updated `IMAGES.md` with Grafana and InfluxDB sections

**29 Apr 2026**
- Fusion360 drawing for Solar panel and Stevenson Shield with ESP32+BME280+RFM95W LoraWan https://a360.co/4tpuix3
- Fusion360 drawing for Raspberry Pi5 + LoraWan module and antenna: https://a360.co/4tKQ2nZ

**5 May 2026**
- Blog  story: [Stevenson Shield for ESP32 LoRaWAN Weather Station — Parametric 3D-Printed Enclosure](hhttps://ollavarma-create.github.io/blog/en/posts/stevenson-shield-esp32-lorawan/)

**29 October 2026**
- Blog  story: [LoRaWAN Outdoor Sensor Half-Year Checkup](https://ollavarma-create.github.io/blog/en/posts/lorawan-ulp-sensor-half-year/)

---

This repository contains files and resources for my personal **LoRaWAN** project. The goal is to build a complete LoRaWAN hardware and software stack covering the field device (node), the gateway, and network management.

The main components used:

- **ESP32-WROOM-32D + RFM95W + BME280** → field device (sensor nodes)
- **Raspberry Pi 5 + RAK WM1302** → LoRaWAN gateway
- **ChirpStack v4** → open-source LoRaWAN network server

---

## Hardware

> 📷 Hardware photos, ChirpStack screenshots, and schematic images are documented in **[IMAGES.md](IMAGES.md)**.

| Component | Role |
|---|---|
| ESP32-WROOM-32D | MCU |
| RFM95W (SX1276) | LoRa radio, EU868 |
| BME280 | Temperature, humidity, pressure |
| CP2102 | USB-UART bridge for flashing |
| LiPo battery | Power source |

### Pin Assignment

| Signal | GPIO |
|---|---|
| SPI SCK | 18 |
| SPI MOSI | 23 |
| SPI MISO | 19 |
| RFM95W CS (NSS) | 5 |
| RFM95W DIO0 | 4 |
| I2C SDA (BME280) | 21 |
| I2C SCL (BME280) | 22 |
| Battery ADC | 34 (ADC1) |
| Battery GND switch | 32 |

---

## Payload Format (4 bytes)

| Byte | Field | Encoding |
|---|---|---|
| 0 | Temperature | value + 40 (covers −40…+85 °C) |
| 1 | Humidity | 0–100 % |
| 2 | Pressure | value − 900 (covers 900–1060 hPa) |
| 3 | Battery | 0–100 % |

Decoder: `Chirpstack/Tenants/Device profiles/chirpstack_decoder_v2.js`

---

## Downlink Commands (fPort 1)

| HEX | Action |
|---|---|
| `01 HH LL` | Set sleep time: seconds = HH×256 + LL |
| `02` / `02 MM` | Start OTA mode (MM = timeout minutes, default 5) |
| `03` | Clear pin cache (forces re-probe on next boot) |

---

## Repository Structure

```
ESP32/
  PlatformIO/                          Active build environment (PlatformIO + VSCode)
    platformio.ini
    src/
      AutoDevID-ESP32_RFM95W_BME280.cpp
      settings.h                       Compile-time constants, pin config
      nvs_config.h                     Runtime NVS configuration
      pin_probe.h                      Automatic I2C + SPI pin detection
      battery.h                        GPIO-controlled battery measurement
      ota_update.h                     ArduinoOTA over Wi-Fi
    OTA-UPDATE-ESP32_RFM95W_BME280.md  OTA update guide (PlatformIO)
  ArduinoIDE/                          Legacy Arduino IDE sketches
    AutoDevID-ESP32_RFM95W_BME280/     Latest sketch (same modules as PlatformIO)
    UltraLowPower-ESP32_RFM95W_BME280/ Earlier ULP version
    ESP32_RFM95W_BME280/               Initial version
    salaojahalytys/                    Foundation drain sensor node
    Arduino IDE - OTA-UPDATE.md        OTA guide for Arduino IDE
  Addon/
    ota_manager.py                     Python script: MQTT monitoring + downlink delivery
  OTA-UPDATE-ESP32_RFM95W_BME280.md    OTA guide (root, kept for backwards compatibility)
  ULP Electrnic Design/
    Schematics.png                     ULP circuit schematic
    KiCad/                             KiCad PCB project files

Chirpstack/
  chirpstack-installation.md          Installation guide (Raspberry Pi 5 + WM1302)
  MQTT_Chirpstack_integration.md      MQTT → Telegraf → InfluxDB integration guide
  raspberrypi5-chirpstack-dedicated.md Dedicated Pi setup notes
  Finnish (old versions)/             Archived Finnish-language documentation
    chirpstack-asennus.MD
    MQTT_Chirpstack_integraatio.md
    raspberrypi5-chirpstack-dedicated.MD
  Tenants/
    Device profiles/
      chirpstack_decoder_v2.js        Payload decoder (4 bytes, battery included)
      chirpstack_decoder_OTA_Battery.js
    Applications/                     Example screenshots
    Gateway/                          Gateway dashboard screenshots

3D tulostus/
  RaspberryPi5_WM1302_V1/            Gateway enclosure for Raspberry Pi 5 + WM1302 HAT
    Base.stl / Middle.stl / Lid.stl
    RaspberryPI5 Case + LoraWan1302 Hat.f3z
    Additional enclosures: https://codeberg.org/hiltsu/fusion360-piirrokset-3D-tulostusobjektit

_images/
  Chirpstack/                         ChirpStack dashboard and frame screenshots
  ESP32_RFM95W_BME280/               Hardware photos and power consumption graph
  Grafana/                            Grafana dashboard and alert rule examples
  InfluxDB/                           InfluxDB measurement and tag configuration examples
  RaspberryPi_SeedStudio1302/        Gateway assembly photos
```

---

## Getting Started

### 1. Field Device (ESP32 Node) — PlatformIO

**Requirements:** PlatformIO CLI or VSCode PlatformIO extension, RadioLib >= 7.3.0

1. Edit `ESP32/PlatformIO/src/settings.h`:
   ```cpp
   #define DEFAULT_WIFI_SSID   "your-ssid"
   #define DEFAULT_WIFI_PASS   "your-password"
   #define DEFAULT_JOIN_EUI    0x...ULL
   #define DEFAULT_APP_KEY     { 0x83, 0x0c, ... }
   ```
2. Build and flash via USB:
   ```bash
   ~/.platformio/penv/bin/pio run --target upload
   ```
3. Read the devEUI from the serial monitor and register it in ChirpStack:
   ```bash
   ~/.platformio/penv/bin/pio device monitor --port /dev/ttyUSB0 --baud 115200
   ```
   ```
   devEUI : 9C:9C:1F:FF:FE:C5:C8:18
   ```
4. Subsequent updates via OTA — see `ESP32/PlatformIO/OTA-UPDATE-ESP32_RFM95W_BME280.md`

> **Custom PCB without auto-reset circuit:** pull IO0 to GND, pulse EN to GND, start
> upload — keep IO0 grounded until `Connecting...` appears, then release.

### 1b. Field Device — Arduino IDE (legacy)

1. Open a sketch from `ESP32/ArduinoIDE/AutoDevID-ESP32_RFM95W_BME280/`
2. Edit `settings.h` as above
3. Set partition scheme: **Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)**
4. Set `#define DEBUG true`, compile, and flash via USB
5. Read the devEUI from the serial monitor and register in ChirpStack
6. Subsequent updates via OTA — see `ESP32/ArduinoIDE/Arduino IDE - OTA-UPDATE.md`

For detailed device registration steps see `DEVICE_REGISTRATION.md` in the relevant sketch folder.

### 2. Gateway (Raspberry Pi 5 + WM1302)

1. Install Raspberry Pi OS Lite
2. Follow `Chirpstack/chirpstack-installation.md` for ChirpStack Concentratord + MQTT Forwarder setup
3. Print and assemble the enclosure from `3D tulostus/RaspberryPi5_WM1302_V1/`
4. Attach the WM1302 HAT to the Pi and configure SPI + server address

### 3. ChirpStack Server

1. See `Chirpstack/chirpstack-installation.md`
2. Typical installation: `docker-compose up -d`
3. Log in to the web interface (usually http://localhost:8080)
4. Add a gateway and application, then register devices
5. For MQTT → InfluxDB → Grafana pipeline see `Chirpstack/MQTT_Chirpstack_integration.md`

---

## Troubleshooting

### DevNonce has already been used

Chirpstack rejects a join with `DevNonce has already been used` if the device attempts to join with a nonce value it has already used. This is replay-attack protection defined in the LoRaWAN specification.

**When this can occur:**
- In older firmware (before 20 Mar 2026): whenever the battery runs out or is disconnected, because RTC memory is cleared on full power loss
- If the EEPROM / NVS is erased or the device is re-flashed without resetting Chirpstack

**Quick fix (Flush OTAA session):**

Chirpstack UI → **Applications** → select device → **Flush OTAA session**

Needs to be done only once — firmware v1.1.0 (20 Mar 2026) prevents the issue from recurring.

**When a Flush will be needed in the future:**
- The device NVS has been erased or corrupted
- The device has been fully re-flashed (nonces reset)
- The device has been replaced with a new unit (new devEUI → new registration in any case)

### sendReceive returns -1101 after deep sleep

Cause: `activateOTAA()` was not called after restoring session buffers from RTC memory. Fixed in v1.1.0 (21 Apr 2026). See changelog for details.

### Upload fails on custom PCB

1. Close the serial monitor (port cannot be shared)
2. Pull IO0 to GND
3. Pulse EN to GND and release
4. Run upload — keep IO0 grounded until `Connecting...` appears
5. Release IO0 after upload completes

---

## Dependencies / Tools

| Tool | Notes |
|---|---|
| PlatformIO | Recommended build environment |
| Arduino IDE | Legacy support |
| ESP32 Arduino core (espressif32) | >= 3.x |
| RadioLib | >= 7.3.0 |
| Adafruit BME280 Library | Current |
| ChirpStack | v4 or later |
| Docker & Docker Compose | Container-based ChirpStack installation |
| Python 3 + `paho-mqtt` + `requests` | For `ota_manager.py` |
| Fusion 360 or other 3D software | For editing gateway enclosure models |

---

## Planned Work / TODO

- Add more sensors (e.g. soil moisture)
- Document gateway external antenna and power connections
- Possibly add CI/CD or automated tests

---

## Related Projects

### Foundation Drain Sensor Node (`salaojahalytys`)

A specialised LoRaWAN node that monitors foundation drain water levels and triggers alerts via ChirpStack integrations. Built on the same ESP32 + RFM95W hardware stack.

- Source and documentation: [`ESP32/PlatformIO/FoundationDrainSensor/`](ESP32/PlatformIO/FoundationDrainSensor/README.md)

### ULP Node — PlatformIO Build, Custom PCB & OTA

Full documentation of the Ultra Low Power node including PlatformIO setup, custom PCB bring-up notes, and OTA update procedures:

- [`ESP32/PlatformIO/Temp_Moisture_Pressure/OTA-UPDATE-ESP32_RFM95W_BME280.md`](ESP32/PlatformIO/Temp_Moisture_Pressure/OTA-UPDATE-ESP32_RFM95W_BME280.md)

### Mailbox Alerter
Full documentation of Battery-powered mailbox alert system using ESP32-WROOM-32D and LoRaWAN:

- [`ESP32/PlatformIO/PostboxAlarm/README.md`](ESP32/PlatformIO/PostboxAlarm/README.md)

---

## License

MIT License

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
