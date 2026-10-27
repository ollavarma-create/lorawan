# OTA-Updatable ESP32 LoRaWAN Sensor — User Guide

This guide covers everything from the first USB flash to performing an OTA update.
The device is an ESP32-WROOM-32D + RFM95W + BME280, using the ArduinoOTA library
and Chirpstack v4 for network management.

---

## Contents

1. [First USB Flash](#1-first-usb-flash)
2. [Arduino IDE — Required Settings](#2-arduino-ide--required-settings)
3. [Registering the Device in Chirpstack](#3-registering-the-device-in-chirpstack)
4. [Downlink Commands — Quick Reference](#4-downlink-commands--quick-reference)
5. [OTA Update Step by Step](#5-ota-update-step-by-step)
6. [espota.py — Update via Direct IP](#6-espotapy--update-via-direct-ip)
7. [Common Errors and Solutions](#7-common-errors-and-solutions)
8. [Downlink Command HEX Reference](#8-downlink-command-hex-reference)

---

## 1. First USB Flash

OTA requires the device to have been flashed via USB at least once with the correct
partition scheme. Subsequent updates can be done wirelessly.

### Steps

1. Connect the ESP32 to your development machine via USB
2. Open the sketch in Arduino IDE
3. Edit `settings.h` before building:
   - `DEFAULT_WIFI_SSID` and `DEFAULT_WIFI_PASS` → your network credentials
   - `DEFAULT_JOIN_EUI` and `DEFAULT_APP_KEY` → values from Chirpstack
4. Set the partition scheme (see section 2)
5. Select the correct port: **Tools → Port → /dev/ttyUSB0** (or equivalent)
6. Press **Upload** (Ctrl+U)

### settings.h — AppKey Format

The 32-character hex string from Chirpstack is converted to a C array in pairs:

```
Chirpstack:  830c0fd67327528c293e05a931c088ab

settings.h:  { 0x83, 0x0c, 0x0f, 0xd6, 0x73, 0x27, 0x52, 0x8c,
               0x29, 0x3e, 0x05, 0xa9, 0x31, 0xc0, 0x88, 0xab }
```

---

## 2. Arduino IDE — Required Settings

### Partition Scheme (critical for OTA)

The default partition scheme splits the flash evenly (~630 kB per slot).
The sketch exceeds 1 MB and will not fit in the OTA partition with default settings.

**Tools → Partition Scheme → `Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)`**

| Setting | Value |
|---|---|
| Board | ESP32-WROOM-DA Module |
| Partition Scheme | **Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)** |
| Upload Speed | 921600 |
| Flash Size | 4MB (32Mb) |

> **Note:** If the partition scheme is wrong, the OTA update will fail silently
> or the device will boot with the old firmware after an update.

### Battery ADC Pins on WROOM-32D

GPIO1 = TXD0 (serial port) — not suitable for ADC.
Wi-Fi blocks ADC2 pins (GPIO 0, 2, 4, 12–15, 25–27) during OTA mode.

Use the following in `settings.h`:
```cpp
#define BATT_GND_GPIO   32    // GPIO-controlled GND
#define BATT_ADC_GPIO   34    // ADC1, no conflict with Wi-Fi
#define BATT_ADC_MAX  4095
```

---

## 3. Registering the Device in Chirpstack

### 3a. Read the devEUI from the Serial Monitor

Open **Tools → Serial Monitor**, baud rate **115200**.
Reset the device — the following appears in the serial monitor:

```
=== LoRaWAN OTAA + OTA | FW 1.1.0 ===
devEUI (register in Chirpstack): 9C:9C:1F:FF:FE:C5:D4:A8
```

Copy the devEUI.

### 3b. Add the Device in Chirpstack

**Applications → [your application] → Add device**

| Field | Value |
|---|---|
| Name | e.g. `TestNode` |
| DevEUI | devEUI without colons, e.g. `9C9C1FFFFEC5D4A8` |
| Device profile | Your profile, e.g. `OTA_ESP32_ULP_BME280` |

### 3c. Set OTAA Keys

**Device → OTAA keys → Application key** = the same 32-character hex string as in `settings.h`.

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

## 4. Downlink Commands — Quick Reference

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

### Step 1 — Export the binary

Arduino IDE: **Sketch → Export Compiled Binary**

The file is saved in the sketch folder:
```
build/esp32.esp32.esp32da/SketchName.ino.bin
```

You can also find it with:
```bash
find ~/Arduino -name "*.ino.bin" 2>/dev/null
```

> Always use `.ino.bin` — not `.merged.bin` (which includes the bootloader
> and is not suitable for OTA updates).

### Step 2 — Queue a sleep time reduction (optional)

If the device sleeps for 10 minutes, you can shorten the wait:

**Queue → Enqueue → HEX: `01003C`** (60 seconds)

### Step 3 — Send the OTA command

**Queue → Enqueue → HEX: `0205`**

The device processes commands in order within the same uplink —
both can be queued at once.

> **Important:** If there is already an incorrect command in the queue, press **Flush queue** first.

### Step 4 — Wait for wake-up

The device receives the downlink in the RX1 window of the next uplink (1 s after uplink).
The serial monitor shows a confirmation:

```
[dl] 2 bytes: 02 05
[dl] OTA mode set: 5 min
```

### Step 5 — Next wake-up starts OTA mode

```
[ota] OTA mode active – waiting for update
[OTA] Hostname: lorawan-c5d4a8 (FW: 1.1.0)
[OTA] Connected: 192.168.64.207
[OTA] Server running – open Arduino IDE → Ports
[OTA] Waiting up to 300 seconds...
```

### Step 6 — Upload firmware

**Option A — Arduino IDE (if mDNS works):**

Tools → Port → `lorawan-c5d4a8 (ESP32)` → Upload

**Option B — espota.py via direct IP (recommended on Linux):**

See section 6.

### Step 7 — Successful update indication

```
[OTA] 10%
[OTA] 20%
...
[OTA] 100%
[OTA] Update complete – rebooting
```

The device reboots automatically. The first join after OTA will fail with code
`-1116` — this is expected because an OTA reset clears RTC memory, setting
`isJoined = false`. The device will join successfully on the next wake-up.
No action required.

### Step 8 — Restore sleep time

If you shortened the sleep time for testing:

**Queue → Enqueue → HEX: `010258`** (600 s = 10 min)

---

## 6. espota.py — Update via Direct IP

On Linux, Arduino IDE may not find the ESP32 via mDNS.
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
  -i 192.168.64.207 \
  -p 3232 \
  -a 192.168.64.21 \
  -f ~/Arduino/Project/build/esp32.esp32.esp32da/Sketch.ino.bin
```

| Parameter | Description |
|---|---|
| `-i` | ESP32 IP address (shown in serial output during OTA mode) |
| `-p` | OTA port, always `3232` |
| `-a` | Development machine IP address (ESP32 connects back to this) |
| `-f` | Path to the `.ino.bin` file |

### Find Your Machine's IP

```bash
ip addr | grep inet | grep -v 127
```

### Find the .bin File

```bash
find ~/Arduino -name "*.ino.bin" 2>/dev/null
```

---

## 7. Common Errors and Solutions

### OTA Mode Does Not Start

| Symptom | Cause | Fix |
|---|---|---|
| No `[dl] OTA mode set` | Wrong payload in queue | Check HEX: `0205`, not `AgU=` or similar |
| Command disappears from queue | Device woke up before command was queued | Re-queue the command |
| Wrong command was first in queue | Stale message | **Flush queue** → re-queue correct command |

### espota.py Fails

| Error | Cause | Fix |
|---|---|---|
| `No response from device` | Firewall blocking return connection | `sudo ufw disable` or `sudo ufw allow from 192.168.x.0/24` |
| `No response from device` | `-a` missing or wrong IP | Add `-a <machine IP>` |
| `Connection refused` | ESP32 not in OTA mode | Queue OTA command and wait for wake-up |
| Connection error in serial | Firewall or wrong IP | Same as above |

### Firewall (Ubuntu/Debian)

```bash
# Check status
sudo ufw status

# Temporarily allow entire subnet
sudo ufw allow from 192.168.64.0/24

# Or disable for the duration of the test
sudo ufw disable

# Re-enable after the test
sudo ufw enable
```

### mDNS Not Working (Device Not Visible in Arduino IDE)

Arduino IDE 2.x on Linux uses its own mDNS implementation, which is known to be unreliable.

```bash
# Check avahi
sudo systemctl status avahi-daemon

# Install if missing
sudo apt install avahi-daemon
sudo systemctl enable --now avahi-daemon

# Check whether the ESP32 is visible
avahi-browse _arduino._tcp
```

If avahi is running but the IDE still does not show the device, use `espota.py`
with a direct IP address (section 6) — this is the more reliable option on Linux.

### Join Fails After OTA (-1116)

An OTA reset (`SW_CPU_RESET`) clears RTC memory, so `isJoined = false`.
The device attempts a new OTAA join — this succeeds automatically on the next wake-up.
No action required.

### Watchdog Reset (TG1WDT_SYS_RESET) During Boot

Pin probing takes too long and the WDT fires.

Fix: add `BOARD_PINS_KNOWN` define to `settings.h` and hard-code the pins
(see comments in `settings.h`). Probing is done only once and saved to NVS —
it is skipped on all subsequent boots.

### Sketch Does Not Fit in the OTA Partition

```
Sketch uses 1098870 bytes (83%) of program storage space.
```

This is a normal sketch size. OTA requires the correct partition scheme:

**Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)**

---

## 8. Downlink Command HEX Reference

### Command 0x01 — Change Sleep Time

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

### Command 0x02 — OTA Mode

```
[0x02]         → 5-minute timeout (default)
[0x02, 0x05]   → 5 minutes
[0x02, 0x0A]   → 10 minutes
[0x02, 0x1E]   → 30 minutes
```

The ESP32 connects to Wi-Fi and waits for an `espota.py` or Arduino IDE update.
After the timeout, the device resumes normal operation.

### Command 0x03 — Clear Pin Cache

```
[0x03]
HEX: 03
```

Forces I2C and SPI pin re-detection on the next wake-up.
Useful if components have been replaced or pins have changed.

---

## Payload Format (4 bytes)

```
Byte 0: temperature + 40   (0–125  → −40…+85 °C)
Byte 1: humidity            (0–100 %)
Byte 2: pressure − 900      (0–160 → 900–1060 hPa)
Byte 3: battery %           (0–100 %)
```

Example: `3D 1C 7F 00`
- `0x3D` = 61 → 61 − 40 = **21 °C**
- `0x1C` = 28 → **28 %**
- `0x7F` = 127 → 127 + 900 = **1027 hPa**
- `0x00` = 0 → **0 % battery** (not connected)

---

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

*Firmware: v1.1.0 | Chirpstack: v4 | ArduinoOTA | ESP32 Arduino Core 3.3.7*
