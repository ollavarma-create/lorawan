# LoRaWAN Device Registration Guide

This guide covers registering a new ESP32 + RFM95W + BME280 sensor node
in Chirpstack v4 using automatic devEUI generation.

---

## Overview

```
[ESP32 node] --> (LoRa RF) --> [RPi5 + WM1302 gateway] --> [Chirpstack v4]
```

Every device has a unique **devEUI** generated automatically from the
factory-programmed MAC address of the ESP32. The JoinEUI and AppKey are
the same across all devices.

---

## Step 1 — Arduino Code: Preparation

Open `ESP32_RFM95W_BME280.ino` in Arduino IDE.

### 1a. Select the board

Near the top of the file, activate one line:

```cpp
// Comment out the other line
#define BOARD_SUPERMINI
// #define BOARD_DEVBOARD
```

or for DevBoard:

```cpp
// #define BOARD_SUPERMINI
#define BOARD_DEVBOARD
```

### 1b. Enable DEBUG

```cpp
#define DEBUG true
```

### 1c. Verify joinEUI and appKey

These are found in Chirpstack: **Applications → [application] → [device] → OTAA keys**

```cpp
uint64_t joinEUI = 0xXXXXXXXXXXXXXXXXULL;  // Chirpstack: Join EUI

uint8_t appKey[16] = {
  0xXX, 0xXX, ...  // Chirpstack: Application key (16 bytes)
};
```

> **Note:** The devEUI does NOT need to be set in the code — it is generated automatically.

### 1d. Build and flash

Arduino IDE: **Upload** (Ctrl+U)

---

## Step 2 — Read the devEUI from the Serial Monitor

Open Arduino IDE: **Tools → Serial Monitor**, baud rate **115200**.

Reset the device or press the reset button. The serial monitor shows:

```
=== ESP32 + RFM95W + BME280 + LoRaWAN OTAA (ULP) ===

devEUI (register in Chirpstack): 3C:DC:75:FF:FE:CE:0C:98
Board: DevBoard
Initialising RFM95...
Radio OK
Initialising BME280...
Starting OTAA activation...
Attempt 1 / 5
Activation failed, code: -1116
```

> **Activation fails at this point — this is expected.**
> The device has not yet been registered in Chirpstack.

Copy the devEUI, e.g. `3C:DC:75:FF:FE:CE:0C:98`.

---

## Step 3 — Register the Device in Chirpstack

### 3a. Open your application

Chirpstack web UI → **Applications** → select your application (e.g. *BME280 Sensors*)

### 3b. Add the device

Click **Add device**.

| Field | Value |
|---|---|
| **Name** | e.g. `ESP32-SuperMini-Outdoor` |
| **DevEUI** | Copied from serial monitor, **without colons**, e.g. `3CDC75FFFECE0C98` |
| **Device profile** | Select an existing profile, e.g. `LoraWan_BME280_ULP` |
| **Skip frame-counter checks** | No (leave as default) |

Click **Submit**.

### 3c. Set OTAA keys

On the device form, select the **OTAA keys** tab.

| Field | Value |
|---|---|
| **Application key** | Same as in the code — 32-character hex string |

Click **Submit**.

> **JoinEUI** is filled in automatically from the device profile, or you can
> verify that it matches the `joinEUI` value in the code.

---

## Step 4 — Verify the Join in Chirpstack

### LoRaWAN Frames

Chirpstack → **Applications** → device → **LoRaWAN Frames**

When the device attempts to join, you will see:

| Type | Description |
|---|---|
| `JoinRequest` | Device is sending a join request |
| `JoinAccept` | Chirpstack accepted the join |

If `JoinRequest` appears but `JoinAccept` is missing, check:
- AppKey is identical in the code and in Chirpstack
- Device profile is CLASS_A
- Gateway is online (Chirpstack → **Gateways**)

### Events

Chirpstack → **Applications** → device → **Events**

After a successful join you will see:

```
join  – Device joined (new session)
up    – Uplink received, fPort=1, data decoded
```

The decoded payload looks like:
```json
{
  "temp": 20,
  "hum": 28,
  "pres": 1016
}
```

---

## Step 5 — Prepare for Production

Once the join has succeeded and data is visible in Chirpstack:

### 5a. Disable DEBUG

```cpp
#define DEBUG false
```

### 5b. Re-flash

Arduino IDE: **Upload** (Ctrl+U)

The device now enters ultra low power mode:
- Wakes every 10 minutes
- Reads BME280 in forced mode (~10 ms)
- Transmits a 3-byte payload via LoRaWAN DR5 (SF7)
- Returns to deep sleep

---

## Step 6 — New Device (Repeat)

For each new device, repeat steps 1–5.

- **Only change** `#define BOARD_SUPERMINI` / `#define BOARD_DEVBOARD`
- JoinEUI and AppKey remain **the same**
- DevEUI is generated automatically — always read it from the serial monitor

---

## Troubleshooting

| Problem | Cause | Fix |
|---|---|---|
| `Activation failed, code: -1116` | Device not registered | Register in Chirpstack (step 3) |
| `JoinRequest` visible, no `JoinAccept` | Wrong AppKey | Verify the key in Chirpstack |
| `Radio initialisation failed` | SPI pins incorrect | Check `#define BOARD_` selection |
| `BME280 NOT FOUND` | I2C address or pins | Check wiring, try 0x76/0x77 |
| DR shows SF9 instead of SF7 | Chirpstack remembers old DR | Chirpstack: device → **Flush OTAA session** |
| `RSSI=0 dBm` in serial monitor | RadioLib limitation on confirmed uplinks | No issue — correct RSSI is visible in Chirpstack Frames |

---

## Payload Format (3 bytes)

```
Byte 0: temperature + 40   (0–125  → −40…+85 °C)
Byte 1: humidity            (0–100  → 0…100 %)
Byte 2: pressure − 900      (0–160  → 900…1060 hPa)
```

### Chirpstack Decoder (JavaScript)

```javascript
function decodeUplink(input) {
  if (input.bytes.length < 3) {
    return { data: {}, errors: ['Payload too short'] };
  }
  return {
    data: {
      temp: input.bytes[0] - 40,
      hum:  input.bytes[1],
      pres: input.bytes[2] + 900
    }
  };
}

function encodeDownlink(input) {
  return { bytes: [], fPort: 1 };
}
```

---

## Hardware

| Component | Model | Notes |
|---|---|---|
| Microcontroller | ESP32-C3 SuperMini or DevBoard-M | Selected via `#define` |
| LoRa radio | RFM95W (SX1276) | EU868, SF7–SF12 |
| Environmental sensor | BME280 | I2C, address 0x76 or 0x77 |
| Gateway | Raspberry Pi 5 + RAK WM1302 | EU868 |
| Network server | Chirpstack v4 (Docker) | CLASS_A, OTAA |

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
