# OTA-Updatable ESP32 LoRaWAN Sensor – User Guide

This guide covers everything from the first USB flash to performing an OTA update.
The device is an ESP32-WROOM-32D + RFM95W + BME280, using the ArduinoOTA library
and Chirpstack v4 for network management.

**Build environment: PlatformIO (VSCode extension)**

---

## Contents

1. [First USB Flash](#1-first-usb-flash)
2. [PlatformIO – Required Settings](#2-platformio--required-settings)
3. [Registering the Device in Chirpstack](#3-registering-the-device-in-chirpstack)
4. [Downlink Commands – Quick Reference](#4-downlink-commands--quick-reference)
5. [OTA Update Step by Step](#5-ota-update-step-by-step)
6. [espota.py – Update via Direct IP](#6-espotapy--update-via-direct-ip)
7. [Common Errors and Solutions](#7-common-errors-and-solutions)
8. [Downlink Command HEX Reference](#8-downlink-command-hex-reference)

---

## 1. First USB Flash

OTA requires the device to have been flashed via USB at least once with the correct
partition scheme. Subsequent updates can be done wirelessly.

### Boot Mode (custom PCB without auto-reset circuit)

The custom PCB does not have a USB-to-serial chip's DTR/RTS auto-reset circuit.
To enter download mode manually:

1. Connect a wire from **IO0** to **GND**
2. Pull **EN** to GND momentarily (reset), then release
3. The chip enters download mode — start the upload within a few seconds
4. After upload completes, **remove the IO0–GND wire** before the next normal boot

> **Tip for next PCB revision:** Add a momentary BOOT button (IO0→GND) and a RESET
> button (EN→GND) to avoid the manual wire procedure.

### Steps

1. Connect the ESP32 to your development machine via USB (`/dev/ttyUSB0`)
2. Open the project in VSCode with PlatformIO
3. Edit `src/settings.h` before building:
   - `DEFAULT_WIFI_SSID` and `DEFAULT_WIFI_PASS` – your network credentials
   - `DEFAULT_JOIN_EUI` and `DEFAULT_APP_KEY` – values from Chirpstack
4. Build and upload: **PlatformIO sidebar → Project Tasks → Upload**
   or in the terminal:
   ```bash
   ~/.platformio/penv/bin/pio run --target upload
   ```

### settings.h – AppKey Format

The 32-character hex string from Chirpstack is converted to a C array in pairs:

```
Chirpstack:  830c0fd67327528c293e05a931c088ab

settings.h:  { 0x83, 0x0c, 0x0f, 0xd6, 0x73, 0x27, 0x52, 0x8c,
               0x29, 0x3e, 0x05, 0xa9, 0x31, 0xc0, 0x88, 0xab }
```

---

## 2. PlatformIO – Required Settings

### platformio.ini

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
monitor_port = /dev/ttyUSB0
upload_port  = /dev/ttyUSB0
board_build.partitions = min_spiffs.csv
upload_speed = 115200
lib_deps =
  jgromes/RadioLib
  adafruit/Adafruit BME280 Library
```

### Partition Scheme (critical for OTA)

The default partition scheme splits the flash evenly (~630 kB per slot).
The sketch exceeds 1 MB and will not fit in the OTA partition with default settings.

`board_build.partitions = min_spiffs.csv` selects
**Minimal SPIFFS (1.9 MB APP with OTA / 128 KB SPIFFS)** — required for OTA.

| Setting | Value |
|---|---|
| Board | `esp32dev` (ESP32-WROOM-32D) |
| Partition scheme | `min_spiffs.csv` |
| Upload speed | 115200 (reliable on custom PCB) |
| Flash size | 4 MB |

> **Note:** If the partition scheme is wrong, the OTA update will fail silently
> or the device will boot with the old firmware after an update.

### Serial Monitor

Open the monitor from the terminal:

```bash
~/.platformio/penv/bin/pio device monitor --port /dev/ttyUSB0 --baud 115200
```

Or via PlatformIO sidebar → **Monitor**.

Close the monitor before uploading — the port cannot be shared.

### Battery ADC Pins on WROOM-32D

Wi-Fi blocks ADC2 pins (GPIO 0, 2, 4, 12–15, 25–27) during OTA mode.

Use the following in `src/settings.h`:
```cpp
#define BATT_GND_GPIO   32    // GPIO-controlled GND
#define BATT_ADC_GPIO   34    // ADC1, no conflict with Wi-Fi
#define BATT_ADC_MAX  4095
```

---

## 3. Registering the Device in Chirpstack

### 3a. Read the devEUI from the Serial Monitor

Open the serial monitor (baud rate 115200).
Reset the device — the following appears:

```
  devEUI             : 9C:9C:1F:FF:FE:C5:C8:18
```

Copy the devEUI.

### 3b. Add the Device in Chirpstack

**Applications → [your application] → Add device**

| Field | Value |
|---|---|
| Name | e.g. `TestNode` |
| DevEUI | devEUI without colons, e.g. `9C9C1FFFFEC5C818` |
| Device profile | Your profile, e.g. `OTA_ESP32_ULP_BME280` |

### 3c. Set OTAA Keys

**Device → OTAA keys → Application key** = the same 32-character hex string as in `src/settings.h`.

### 3d. Add a Codec

**Device Profiles → [profile] → Codec → Custom JavaScript**

Paste the contents of `chirpstack_decoder_v2.js` into the field and save.

### 3e. Measurements Tab

Once the device has sent its first uplink, go to
**Device Profiles → [profile] → Measurements**.

Set all fields to type **Gauge** with the following names:

| Key | Kind | Measurement name |
|---|---|---|
| `temp` | Gauge | Temperature |
| `hum` | Gauge | Humidity |
| `pres` | Gauge | Air pressure |
| `batt_pct` | Gauge | Battery % |
| `batt_v` | Gauge | Battery V |

### 3f. Location Tag

**Device → Configuration → Tags → Add:**

| Key | Value |
|---|---|
| `location` | e.g. `office` |

The tag appears automatically in InfluxDB via the Telegraf integration.

---

## 4. Downlink Commands – Quick Reference

Commands are sent via: **Applications → device → Queue → Enqueue**

- **fPort:** `1`
- **Confirmed:** no (disabled)
- **Format:** HEX tab

### All Commands

| Action | HEX | Description |
|---|---|---|
| Set sleep time 60 s | `01003C` | [0x01, 0x00, 0x3C] |
| Set sleep time 5 min | `010132` | [0x01, 0x01, 0x32] = 306 s |
| Set sleep time 10 min | `010258` | [0x01, 0x02, 0x58] = 600 s |
| Start OTA (5 min) | `0205` | [0x02, 0x05] |
| Start OTA (10 min) | `020A` | [0x02, 0x0A] |
| Clear pin cache | `03` | forces a new pin probe |

### Calculating Sleep Time

Sleep time is sent as a 16-bit big-endian value (bytes 1 and 2):

```
seconds → hex:
 60 s = 0x003C → HEX: 01 00 3C
300 s = 0x012C → HEX: 01 01 2C
600 s = 0x0258 → HEX: 01 02 58
```

---

## 5. OTA Update Step by Step

### Step 1 – Build the binary

PlatformIO: **Project Tasks → Build**, or in the terminal:

```bash
~/.platformio/penv/bin/pio run
```

The firmware binary is saved to:
```
.pio/build/esp32dev/firmware.bin
```

> Always use `firmware.bin` — not `firmware.elf` or any merged variant.
> This file contains only the application partition and is safe for OTA.

### Step 2 – Queue a sleep time reduction (optional)

If the device sleeps for 10 minutes, you can shorten the wait:

**Queue → Enqueue → HEX: `01003C`** (60 seconds)

### Step 3 – Send the OTA command

**Queue → Enqueue → HEX: `0205`**

The device processes commands in order within the same uplink —
both can be queued at once.

> **Important:** If there is already an incorrect command in the queue, press **Flush queue** first.

### Step 4 – Wait for wake-up

The device receives the downlink in the RX1 window of the next uplink (1 s after uplink).
The serial monitor shows a confirmation:

```
[dl] 2 bytes: 02 05
[dl] OTA-tila asetettu: 5 min
```

### Step 5 – Next wake-up starts OTA mode

```
[ota] OTA-tila aktiivinen – haetaan päivitystä
[OTA] Hostname: lorawan-c5c818 (FW: 1.1.0)
[OTA] Connected: 192.168.x.x
[OTA] Server running – waiting up to 300 seconds...
```

### Step 6 – Upload firmware via espota.py

On Linux, mDNS is often unreliable. Use `espota.py` with a direct IP address (see section 6).

### Step 7 – Successful update indication

```
[OTA] 10%
[OTA] 20%
...
[OTA] 100%
[OTA] Update complete – rebooting
```

The device reboots automatically.

### Step 8 – Nonces and re-join after OTA

After OTA, the device reboots with `SW_CPU_RESET`, which clears RTC memory
(`isJoined = false`). The device performs a fresh OTAA join on the next wake-up.

Nonces are **not lost** — they are stored in NVS (flash), not in RTC memory.
The DevNonce counter is loaded from NVS and the join uses the correct next value.
No action required.

### Step 9 – Restore sleep time

If you shortened the sleep time for testing:

**Queue → Enqueue → HEX: `010258`** (600 s = 10 min)

---

## 6. espota.py – Update via Direct IP

On Linux, Arduino IDE 2.x and PlatformIO may not find the ESP32 via mDNS.
`espota.py` bypasses mDNS and connects directly to the IP address.

### Locate espota.py

```bash
find ~/.arduino15 -name "espota.py" 2>/dev/null
```

Use the newest version, e.g.:
```
/home/user/.arduino15/packages/esp32/hardware/esp32/3.3.7/tools/espota.py
```

### Run the Update

```bash
python3 ~/.arduino15/packages/esp32/hardware/esp32/3.3.7/tools/espota.py \
  -i 192.168.x.x \
  -p 3232 \
  -a 192.168.x.y \
  -f /path/to/project/.pio/build/esp32dev/firmware.bin
```

| Parameter | Description |
|---|---|
| `-i` | ESP32 IP address (shown in serial output during OTA mode) |
| `-p` | OTA port, always `3232` |
| `-a` | Development machine IP address (ESP32 connects back to this) |
| `-f` | Path to `.pio/build/esp32dev/firmware.bin` |

### Find Your Machine's IP

```bash
ip addr | grep inet | grep -v 127
```

---

## 7. Common Errors and Solutions

### Upload Fails (USB / first flash)

| Symptom | Cause | Fix |
|---|---|---|
| `Failed to connect to ESP32` | Not in download mode | Pull IO0→GND, reset EN, retry upload |
| Port busy | Serial monitor is open | Close monitor (Ctrl+C), then upload |
| `wrong boot mode` | IO0 released too early | Keep IO0→GND until `Connecting...` appears |

### OTA Mode Does Not Start

| Symptom | Cause | Fix |
|---|---|---|
| No `[dl] OTA-tila asetettu` | Wrong payload in queue | Check HEX: `0205`, not `AgU=` or similar |
| Command disappears from queue | Device woke up before command was queued | Re-queue the command |
| Wrong command was first in queue | Stale message | **Flush queue** → re-queue correct command |

### espota.py Fails

| Error | Cause | Fix |
|---|---|---|
| `No response from device` | Firewall blocking return connection | `sudo ufw disable` or allow subnet |
| `No response from device` | `-a` missing or wrong IP | Add `-a <machine IP>` |
| `Connection refused` | ESP32 not in OTA mode | Queue OTA command and wait for wake-up |

### Firewall (Ubuntu/Debian)

```bash
# Check status
sudo ufw status

# Temporarily allow entire subnet
sudo ufw allow from 192.168.x.0/24

# Or disable for the duration of the test
sudo ufw disable

# Re-enable after the test
sudo ufw enable
```

### mDNS Not Working

```bash
# Check avahi
sudo systemctl status avahi-daemon

# Install if missing
sudo apt install avahi-daemon
sudo systemctl enable --now avahi-daemon
```

If avahi is running but the device is still not visible, use `espota.py`
with a direct IP address (section 6) — this is the more reliable option on Linux.

### Watchdog Reset (TG1WDT_SYS_RESET) During Boot

Pin probing takes too long and the WDT fires.

Fix: `BOARD_PINS_KNOWN` is already defined in `src/settings.h` — pins are
hard-coded and probing is skipped. Check that `getDefaultPinConfig()` returns
the correct values for the PCB.

### Sketch Does Not Fit in the OTA Partition

```
RAM:   [=         ]  13.0% (used 42556 bytes from 327680 bytes)
Flash: [========= ]  85.9% (used 1126929 bytes from 1310720 bytes)
```

This is a normal sketch size with `min_spiffs.csv`. If the build fails with
"region `iram0_0_seg' overflowed", check that `platformio.ini` has:
```
board_build.partitions = min_spiffs.csv
```

---

## 8. Downlink Command HEX Reference

### Command 0x01 – Change Sleep Time

```
[0x01, high_byte, low_byte]
seconds = high_byte × 256 + low_byte
```

| Sleep time | Seconds | HEX |
|---|---|---|
| 1 minute | 60 | `01003C` |
| 5 minutes | 300 | `01012C` |
| 10 minutes | 600 | `010258` |
| 15 minutes | 900 | `010384` |
| 30 minutes | 1800 | `010708` |
| 1 hour | 3600 | `010E10` |

### Command 0x02 – OTA Mode

```
[0x02]         → 5-minute timeout (default)
[0x02, 0x05]   → 5 minutes
[0x02, 0x0A]   → 10 minutes
[0x02, 0x1E]   → 30 minutes
```

The ESP32 connects to Wi-Fi and waits for an `espota.py` update.
After the timeout, the device resumes normal operation.

### Command 0x03 – Clear Pin Cache

```
[0x03]
HEX: 03
```

Forces I2C and SPI pin re-detection on the next wake-up.
Useful if components have been replaced or pins have changed.

---

## Payload Format (4 bytes)

```
Byte 0: temperature + 40   (0–125  → -40…+85 °C)
Byte 1: humidity            (0–100 %)
Byte 2: pressure – 900      (0–160 → 900–1060 hPa)
Byte 3: battery %           (0–100 %)
```

Example: `42 12 76 00`
- `0x42` = 66 → 66 − 40 = **26 °C**
- `0x12` = 18 → **18 %**
- `0x76` = 118 → 118 + 900 = **1018 hPa**
- `0x00` = 0 → **0 % battery** (not connected)

---

## Updates
29.04.2026: Solar panel and Stevenson Shield with ESP32+BME280+RFM95W LoraWan https://a360.co/4tpuix3

## License

MIT License

Copyright (c) 2026 Jari Hiltunen (Codeberg: [hiltsu](https://codeberg.org/hiltsu))

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

*Firmware: v1.1.0 | Chirpstack: v4 | ArduinoOTA | ESP32 Arduino Core 3.3.7 | PlatformIO*
