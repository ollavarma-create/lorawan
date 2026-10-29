# Raspberry Pi 5 — Dedicated ChirpStack Gateway

The goal of this project is to migrate an MQTT broker, InfluxDB, and Grafana
from an old Raspberry Pi 3B to a new Raspberry Pi 5, and to add LoRaWAN sensors.

This guide covers two installation approaches:

- **Dedicated gateway (this guide):** Flash ChirpStack Gateway OS (OpenWrt-based)
  directly to the SD card. Simple and reliable, but **InfluxDB and Grafana cannot
  be installed** — OpenWrt does not support them.
- **Full stack on one Pi:** Install the official Raspberry Pi OS and run ChirpStack
  in Docker alongside InfluxDB and Grafana.

*This guide will be updated as testing progresses.*

---

## What is LoRaWAN?

**LoRaWAN** is a low-power wide-area network (LPWAN) technology designed specifically
for IoT devices such as sensors, meters, and tracking devices. It enables transmission
of small amounts of data over long distances — typically 2–15 km in urban or rural areas —
while devices consume very little power and can operate for years on batteries.

LoRaWAN is built on top of **LoRa** modulation, but the two terms are not interchangeable.

### LoRa vs. LoRaWAN

**LoRa** (Long Range)
- Physical layer — the radio modulation technique (Chirp Spread Spectrum).
- Defines only how signals are transmitted over long distances at low power.
- Does not specify how devices communicate, how the network is structured, or how
  security is handled.
- Can be used point-to-point (P2P), in mesh networks, or with custom protocols.

**LoRaWAN**
- Network protocol (MAC layer) that runs on top of LoRa modulation.
- Defines the rules: how devices join the network (OTAA/ABP), transmission timing,
  packet format, security (AES-128 encryption), and device class types (Class A, B, C).
- Uses a star topology: sensors → gateways → network server → application
  (e.g. ChirpStack, The Things Network).
- Ensures interoperability between devices from different manufacturers.

In short: **LoRa** is the radio, **LoRaWAN** is the protocol that governs how the
radio is used in a network.

### Frequency Bands and Regulations in Finland

In Finland (and across the EU), LoRaWAN operates on the **EU868** band:
**863–870 MHz** (commonly referred to as the 868 MHz band). This is a **licence-free**
ISM band — no individual radio licence is required, provided the applicable rules
are followed.

Key restrictions in Finland / EU (ETSI EN 300.220, LoRaWAN Regional Parameters):

- **Maximum transmit power:** 14 dBm (25 mW) on most channels;
  up to 27 dBm (500 mW) on some sub-bands with very strict duty cycle limits.
- **Duty cycle:** Devices may transmit only a fixed percentage of the time
  (typically 0.1–1 % depending on the sub-band) to prevent channel congestion.
- **Mandatory channels:** Devices must support at least
  868.1 MHz, 868.3 MHz, and 868.5 MHz.
- Additional common uplink channels: 867.1–867.9 MHz.

Traficom (Finnish Transport and Communications Agency) oversees spectrum use in Finland,
but normal LoRaWAN operation within these rules requires no individual permit —
it is a licence-free SRD band.

### Summary

1. LoRa = long-range radio modulation.
2. LoRaWAN = standardised network protocol on top of LoRa (security, scalability,
   interoperability).
3. In Finland, the 863–870 MHz band is used — licence-free within the rules.
   Ideal for DIY IoT projects such as environmental sensors.

---

## LoRaWAN Setup on Raspberry Pi 5

### Hardware Used

A custom enclosure was built for the following components:

- Raspberry Pi 5
- Seeed Studio WM1302 LoRaWAN module
- LoRaWAN HAT for Raspberry Pi (SPI communication)
- GPS/GNSS antenna
- 868 MHz vertical-polarisation antenna

### Why ChirpStack Gateway OS?

On **Raspberry Pi 5**, GPIO control has been moved to the RP1 I/O chip,
which introduces a new offset scheme:

| BCM GPIO | Kernel offset |
|---|---|
| GPIO 5 | gpio-576 |
| GPIO 17 | gpio-588 |
| GPIO 18 | gpio-589 |

Additionally, on Raspberry Pi 5 with kernel 6.6+ (Bookworm), the legacy
`/sys/class/gpio` sysfs interface is **deprecated and partially broken**.

Because sysfs is no longer supported, the recommended approach is to use
**ChirpStack Gateway OS** — an OpenWrt-based image that includes everything needed
out of the box: `concentratord` for SX1302, Basics Station protocol,
MQTT/UDP forwarding, and native Raspberry Pi 5 + Seeed WM1302 HAT support.

---

## Installation

### 1. Download the Correct Image for Raspberry Pi 5

> **Note:** Using this method, you can only install applications supported by OpenWrt.
> InfluxDB and Grafana **cannot** be installed with this approach.

Go to the official page:
[https://www.chirpstack.io/docs/chirpstack-gateway-os/install/raspberry-pi.html](https://www.chirpstack.io/docs/chirpstack-gateway-os/install/raspberry-pi.html)

- Select the **Raspberry Pi 5** section.
- Recommended image: **Full** (includes ChirpStack Network Server, MQTT forwarder,
  Node-RED, etc. — suitable for running everything on a single Pi).
- Download the **SD card factory image** (`.img.gz`).
  Example filename:
  `chirpstack-gateway-os-4.10.1-full-bcm27xx-bcm2712-rpi-5-squashfs-factory.img.gz`
  (always download the latest available version)
- File size is under 50 MB.

### 2. Flash the Image to the SD Card

Use one of the following tools:

- **Raspberry Pi Imager** (recommended): Choose OS → Use custom → select the `.img.gz` file.
- **balenaEtcher**: Select image, select SD card, click Flash.

Wait for flashing to complete.

### 3. Assemble the Hardware Before Booting

- WM1302 module mounted on the Pi HAT.
- HAT attached to the Pi 5 GPIO header (verify that SPI pins align correctly).
- Connect an Ethernet cable (Wi-Fi can be configured later) or use headless mode.
- Insert the SD card and apply power.

### 4. First Boot and Initial Configuration

- The Pi boots directly into ChirpStack Gateway OS (OpenWrt-based).
- Default IP: **192.168.10.1** (via Ethernet), or locate the Pi's IP via your
  router's DHCP lease list or `nmap`.
- Open a browser and go to: [http://192.168.10.1](http://192.168.10.1)
- Log in: **root** / no password (change this immediately).

**Concentratord configuration:**

- Go to **ChirpStack → Concentratord**.
- Under **Global Configuration**, enable **Enabled** and set **Chipset** to
  **SX1302 / SX1303**.
- On the **SX1302/SX1303** tab:
  - **Antenna Gain:** `2` (suitable for a typical vertical antenna)
  - **Shield Model:** `SemTech - LoRa® CoreCell 868 MHz`
- **GNSS:** Enable if using a GPS/GNSS module (I2C-based communication).
- Click **Save & Apply** — the gateway should start automatically.

### 5. Connect to an External LoRaWAN Network Server

**Using your own ChirpStack instance** (on another machine or in the cloud):
- Configure **Basics Station** forwarding (natively supported by Gateway OS).
- Provide the server URL, authentication mode (TLS or none), and gateway EUI.

**Alternatives:**
- MQTT integration (available in the Full image).
- UDP forwarding to The Things Network or Helium.

### 6. Testing

Check the logs (via the LuCI web interface or SSH with `logread`) to verify that
`concentratord` starts successfully and detects the SX1302
(e.g. `"chip version 0x10"`, SPI OK).

If the gateway appears in the network and receives packets — setup is complete.

**Verifying LoRaWAN sensor data:**

ChirpStack v4 publishes uplink events to MQTT using the following topic structure:

```
application/{applicationID}/device/{devEUI}/event/up
```

Monitor incoming messages:

```bash
mosquitto_sub -h localhost -p 1893 -u chirpstack_gw -P YOUR_PASSWORD \
  -t "application/+/device/+/event/up" -v
```

Signal quality (RSSI, SNR) and channel statistics are available under
**Status → Channel Analysis** and **Bandwidth** in the LuCI interface.

---

## Known Issues — Pi 5 + WM1302

| Problem | Likely Cause | Fix |
|---|---|---|
| `concentratord` does not start | SPI not enabled or wrong shield model | Verify settings in web UI; reboot if needed |
| GPS not working | I2C not configured | Gateway OS usually handles this automatically |
| Custom configuration needed | Default config does not match your setup | SSH in as root, edit `/etc/config/chirpstack-concentratord` |

---

## Power Consumption

Power consumption is modest. The Raspberry Pi 5 comes with a 5 V / 5 A power supply,
which is more than sufficient even when running MQTT, InfluxDB, and Grafana alongside
the gateway services.

---

## 3D printable case
- Fusion360 drawing at https://a360.co/4tKQ2nZ

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
