# ChirpStack Installation Guide — Raspberry Pi 5 + RAK WM1302

This is a basic guide for a straightforward installation. Note that this configuration gets the essentials working. You can also use [sx1302_hal](https://github.com/Lora-net/sx1302_hal) as a starting point for SeedStudio WM1302 boards and define UDP-based communication, but in my experience — especially on Raspberry Pi 5 — sx1302_hal only works correctly on the first boot. For some reason, on a second start (soft reboot) the SPI connection is lost or disrupted. Additionally, the default reset pin 17 does not work at all, but pins 23 or 13 do. For these reasons, the [ChirpStack Concentratord](https://github.com/chirpstack/chirpstack-concentratord) described below is the better alternative.

The project documentation is available at [chirpstack.io](https://www.chirpstack.io/).

The easiest installation is to use a ready-made OpenWRT-based image written directly to an SD card — everything is pre-configured. 

However, I want Grafana and various databases such as InfluxDB on the same device, which rules out OpenWRT. The operating system of choice is therefore **Raspberry Pi OS Lite 64-bit**.

---

## Installing Raspberry Pi OS

Download Pi Imager and install **Other → Raspberry Pi OS Lite 64-bit**.

Connect a keyboard, ethernet cable, and monitor. The following settings are recommended at this stage:

1. Keyboard → Other → Finnish → Finnish *(or your locale)*
2. New username and password (remember these!)
3. Log in with the account you just created
4. `sudo apt update && sudo apt upgrade`
5. `sudo raspi-config`, then configure the following:
6. Menu 1: System → Hostname: choose a distinctive name for your device
7. Menu 3: I1: SSH → Enable (allows remote SSH access)
8. Menu 3: I4: SPI → Enable
9. Menu 3: I5: I2C → Enable
10. Menu 3: I6: No login shell, Yes serial port interface
11. Menu 5: L1: your locale (e.g. `en_GB.UTF-8`)
12. Menu 5: L2: your timezone
13. Menu 6: A1: Expand Filesystem
14. Reboot
15. Check what IP address the Raspberry received and connect via SSH: `ssh username@device-ip`

If you want to set a static IP address, use the `nmtui` tool. Disable IPv6 — the kernel sometimes struggles to handle an IPv4-only DNS server alongside IPv6 and behaves unreliably:

```bash
nmcli connection show
sudo nmcli connection modify "Wired connection 1" ipv6.method "disabled"
sudo nmcli connection up "Wired connection 1"
```

Create a dedicated `lorawan` user so that services do not need to run as root:

```bash
sudo adduser lorawan
# Enter a password and name, press Enter through the remaining prompts
sudo usermod -a -G spi,i2c,gpio,dialout,plugdev lorawan
sudo nano /etc/udev/rules.d/99-spi.rules
# Add this line: SUBSYSTEM=="spidev", GROUP="spi", MODE="0660"
# Save: Ctrl+O → Enter → Ctrl+X
sudo udevadm control --reload-rules && sudo udevadm trigger
sudo -u lorawan ls /dev/spidev*   # should list devices without "Permission denied"
sudo -u lorawan ls /dev/i2c*      # should show i2c-1
sudo -u lorawan ls /dev/gpiochip* # GPIO chips
sudo adduser lorawan sudo
sudo -u lorawan -s                # or: su - lorawan
```

You are now acting as a superuser under the `lorawan` account. Install git and other base packages:

```bash
sudo apt install git build-essential -y
sudo apt install git build-essential libgpiod-dev -y
```

---

## ChirpStack via Docker

If you want the quickest path and do not need Grafana/InfluxDB on the same device, use Docker:

```bash
# Install Docker + Compose (if not already installed)
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh
sudo usermod -aG docker $USER
# Re-login or: newgrp docker
sudo apt install docker-compose-plugin -y   # Compose v2
```

The sections below describe the more traditional installation method.

---

## ChirpStack with SQLite

Install [ChirpStack](https://www.chirpstack.io/docs/chirpstack/downloads.html) using an SQLite database. PostgreSQL is an alternative for larger deployments, but version 17 can cause issues during setup — in my tests ChirpStack was unable to create the required tables in a new database. **Always download the latest release.**

```bash
wget https://artifacts.chirpstack.io/downloads/chirpstack/chirpstack_4.16.2_sqlite_linux_arm64.tar.gz
tar xvfz chirpstack_4.16.2_sqlite_linux_arm64.tar.gz chirpstack
sudo cp chirpstack /usr/bin/
wget https://raw.githubusercontent.com/chirpstack/chirpstack/master/chirpstack/configuration/chirpstack.toml \
  -O /tmp/chirpstack.toml
wget https://github.com/chirpstack/chirpstack-concentratord/blob/master/chirpstack-concentratord-sx1302/config/channels_eu868.toml \
  -O /tmp/channels_eu868.toml
sudo mkdir -p /etc/chirpstack
sudo cp /tmp/chirpstack.toml /etc/chirpstack/chirpstack.toml
sudo cp /tmp/channels_eu868.toml /etc/chirpstack/channels_eu868.toml
sudo nano /etc/chirpstack/chirpstack.toml
```

You can alternatively append the contents of `channels_eu868.toml` directly to the end of `chirpstack.toml`.

> **Note:** If you want to use a region other than EU868, copy the channel files for that region and configure accordingly.

In the file, comment out all active lines under the `[postgresql]` section with `#`. Also verify:

1. Redis is configured: `[redis] servers = ["localhost:6379"]`
2. Create the database directory: `sudo mkdir -p /var/lib/chirpstack`
3. Set ownership: `sudo chown lorawan:lorawan /var/lib/chirpstack`

Append to the end of the file:

```toml
[sqlite]
# SQLite DB path.
# Make sure the path exists and that the ChirpStack process has read-write access.
# If the database file does not exist, it will be created the first time ChirpStack starts.
path = "sqlite:///var/lib/chirpstack/chirpstack.db"
```

At this stage, keep integrations limited to MQTT only:

```toml
[integration]
enabled = ["mqtt"]

[integration.mqtt]
server = "tcp://127.0.0.1:1883"
json = true

[gateway.backend.mqtt]
topic_prefix = "eu868"
```

Test the installation:

```bash
chirpstack --config /etc/chirpstack
```

Expected output:

```
2026-02-07T10:03:46.706768Z  INFO chirpstack::cmd::root: Starting ChirpStack LoRaWAN Network Server version="4.16.2" docs="https://www.chirpstack.io/"
2026-02-07T10:03:46.706906Z  INFO chirpstack::storage::sqlite: Setting up SQLite connection pool
2026-02-07T10:03:46.707141Z  INFO chirpstack::storage: Applying schema migrations
2026-02-07T10:03:46.708881Z  INFO chirpstack::storage: Setting up Redis client
2026-02-07T10:03:46.709002Z  INFO chirpstack::region: Setting up regions
2026-02-07T10:03:46.709066Z  INFO chirpstack::backend::joinserver: Setting up Join Server clients
2026-02-07T10:03:46.709140Z  INFO chirpstack::backend::roaming: Setting up roaming clients
2026-02-07T10:03:46.709201Z  INFO chirpstack::adr: Setting up adr algorithms
2026-02-07T10:03:46.709282Z  INFO chirpstack::integration: Setting up global integrations
2026-02-07T10:03:46.709304Z  INFO chirpstack::integration::redis: Initializing Redis integration
2026-02-07T10:03:46.709368Z  INFO chirpstack::gateway::backend: Setting up gateway backends for the different regions
2026-02-07T10:03:46.709428Z  INFO chirpstack::downlink: Setting up Class-B/C scheduler loop
2026-02-07T10:03:46.709470Z  INFO chirpstack::downlink: Setting up multicast scheduler loop
2026-02-07T10:03:46.709535Z  INFO chirpstack::applayer::fuota: Setting up FUOTA scheduler loop
2026-02-07T10:03:46.709609Z  INFO chirpstack::api: Setting up API interface bind=0.0.0.0:8080
2026-02-07T10:03:46.722669Z  INFO chirpstack::api::backend: Backend interfaces API interface is disabled
```

Install Redis:

```bash
sudo apt update
sudo apt install redis-server
sudo systemctl start redis-server
sudo systemctl enable redis-server
sudo systemctl status redis-server
```

Create `/etc/chirpstack/region_eu868.toml` with the following content:

```toml
# EU868 region configuration example.
[[regions]]

  # Name is a user-defined identifier for this region.
  name="eu868"

  # Common-name refers to the common-name of this region as defined by the LoRa Alliance.
  common_name="EU868"

  # Gateway configuration.
  [regions.gateway]

    # Force gateways as private.
    # If enabled, gateways can only be used by devices under the same tenant.
    force_gws_private=false

    # Gateway backend configuration.
    [regions.gateway.backend]

      # The enabled backend type.
      enabled="mqtt"

      # MQTT configuration.
      [regions.gateway.backend.mqtt]

        # Event topic template.
        event_topic="eu868/gateway/+/event/+"

        # Command topic template.
        command_topic="eu868/gateway/{{ gateway_id }}/command/{{ command }}"

        # MQTT server (e.g. scheme://host:port where scheme is tcp, ssl or ws)
        server="tcp://localhost:1883"

        # Connect with the given username (optional)
        username=""

        # Connect with the given password (optional)
        password=""

        # Quality of service level
        # 0: at most once / 1: at least once / 2: exactly once
        qos=0

        # Clean session
        clean_session=true

        # Client ID (leave blank for a random ID — requires clean_session=true)
        client_id=""

        ca_cert=""
        tls_cert=""
        tls_key=""

    # Gateway channel configuration.
    # Note: only used with ChirpStack Concentratord; ignored otherwise.
    [[regions.gateway.channels]]
      frequency=868100000
      bandwidth=125000
      modulation="LORA"
      spreading_factors=[7, 8, 9, 10, 11, 12]

    [[regions.gateway.channels]]
      frequency=868300000
      bandwidth=125000
      modulation="LORA"
      spreading_factors=[7, 8, 9, 10, 11, 12]

    [[regions.gateway.channels]]
      frequency=868500000
      bandwidth=125000
      modulation="LORA"
      spreading_factors=[7, 8, 9, 10, 11, 12]

    [[regions.gateway.channels]]
      frequency=867100000
      bandwidth=125000
      modulation="LORA"
      spreading_factors=[7, 8, 9, 10, 11, 12]

    [[regions.gateway.channels]]
      frequency=867300000
      bandwidth=125000
      modulation="LORA"
      spreading_factors=[7, 8, 9, 10, 11, 12]

    [[regions.gateway.channels]]
      frequency=867500000
      bandwidth=125000
      modulation="LORA"
      spreading_factors=[7, 8, 9, 10, 11, 12]

    [[regions.gateway.channels]]
      frequency=867700000
      bandwidth=125000
      modulation="LORA"
      spreading_factors=[7, 8, 9, 10, 11, 12]

    [[regions.gateway.channels]]
      frequency=867900000
      bandwidth=125000
      modulation="LORA"
      spreading_factors=[7, 8, 9, 10, 11, 12]

    [[regions.gateway.channels]]
      frequency=868300000
      bandwidth=250000
      modulation="LORA"
      spreading_factors=[7]

    [[regions.gateway.channels]]
      frequency=868800000
      bandwidth=125000
      modulation="FSK"
      datarate=50000

  # Region-specific network configuration.
  [regions.network]
    installation_margin=10
    rx_window=0
    rx1_delay=1
    rx1_dr_offset=0
    rx2_dr=0
    rx2_frequency=869525000
    rx2_prefer_on_rx1_dr_lt=0
    rx2_prefer_on_link_budget=false
    downlink_tx_power=-1
    adr_disabled=false
    min_dr=0
    max_dr=5

    [regions.network.rejoin_request]
      enabled=false
      max_count_n=0
      max_time_n=0

    [regions.network.class_b]
      ping_slot_dr=0
      ping_slot_frequency=0

    [[regions.network.extra_channels]]
    frequency=867100000
    min_dr=0
    max_dr=5

    [[regions.network.extra_channels]]
    frequency=867300000
    min_dr=0
    max_dr=5

    [[regions.network.extra_channels]]
    frequency=867500000
    min_dr=0
    max_dr=5

    [[regions.network.extra_channels]]
    frequency=867700000
    min_dr=0
    max_dr=5

    [[regions.network.extra_channels]]
    frequency=867900000
    min_dr=0
    max_dr=5
```

Open your browser and navigate to your Raspberry's address with port 8080, e.g. `http://192.168.x.y:8080`. Default credentials are `admin` / `admin`.


**Change the admin password immediately under Users → Admin → Change password.**

Once you have verified everything is working, create a systemd service:

```bash
sudo nano /etc/systemd/system/chirpstack.service
```

```ini
[Unit]
Description=ChirpStack LoRaWAN Network Server
After=network.target redis-server.service

[Service]
ExecStart=/usr/bin/chirpstack --config /etc/chirpstack
Restart=always
User=lorawan

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now chirpstack
sudo systemctl status chirpstack
```

Expected status output:

```
chirpstack.service - ChirpStack LoRaWAN Network Server
     Loaded: loaded (/etc/systemd/system/chirpstack.service; enabled; preset: enabled)
     Active: active (running) since Sat 2026-02-07 12:58:56 EET; 2s ago
   Main PID: 3914 (chirpstack)
      Tasks: 10 (limit: 9577)
        CPU: 44ms
     CGroup: /system.slice/chirpstack.service
             └─3914 /usr/bin/chirpstack --config /etc/chirpstack
```

---

## Installing the MQTT Broker

Next, install the Mosquitto MQTT broker. Data flows as follows:
Concentratord → MQTT Forwarder → MQTT broker → ChirpStack.
MQTT is also used for Wi-Fi based sensors and for routing data to InfluxDB and Grafana.
MQTT is the de facto standard in IoT systems. This guide configures the broker without
authentication or TLS — enable both for production deployments.

```bash
sudo apt install mosquitto mosquitto-clients -y
sudo nano /etc/mosquitto/conf.d/local.conf
```

```
listener 1883
allow_anonymous true
```

```bash
sudo systemctl restart mosquitto
sudo systemctl enable --now mosquitto
sudo systemctl status mosquitto
```

Test that ChirpStack connects to the broker:

```bash
chirpstack --config /etc/chirpstack
```

Look for this line in the output:

```
2026-02-07T10:11:29.452233Z INFO chirpstack::integration::mqtt: Connecting to MQTT broker server_uri=tcp://localhost:1883/ client_id=a118dd346c97cfbf clean_session=false
```

---

## ChirpStack Concentratord for WM1302

Download the latest [Concentratord](https://github.com/chirpstack/chirpstack-concentratord) for arm64:

```bash
wget https://artifacts.chirpstack.io/downloads/chirpstack-concentratord/chirpstack-concentratord-sx1302_4.6.0_linux_arm64.tar.gz
tar xvfz chirpstack-concentratord-sx1302_4.6.0_linux_arm64.tar.gz
sudo cp chirpstack-concentratord-sx1302 /usr/bin/
sudo mkdir -p /etc/chirpstack-concentratord
sudo nano /etc/chirpstack-concentratord/concentratord.toml
```

Minimum required configuration:

```toml
# Concentratord configuration for Seeed WM1302 Pi HAT (SPI)

[gateway]
  antenna_gain = 1
  lorawan_public = false
  region = "EU868"
  model = "seeed_wm1302"
  gateway_id = "your-gateway-id-here"
  time_fallback_enabled = true
  model_flags = ["GNSS"]
  gnss_dev_path = "/dev/ttyAMA0"

[concentrator]
  model = "seeed_wm1302"
  com_dev_path = "/dev/spidev0.0"

[concentrator.reset_pin]
  chip = "/dev/gpiochip0"
  pin = 17

[gateway.concentrator]
  multi_sf_channels = [
    868100000,
    868300000,
    868500000,
    867100000,
    867300000,
    867500000,
    867700000,
    867900000
  ]
```

> **Note:** Setting `lorawan_public = false` makes the gateway truly private and ignores public sync word headers (0x34), but may cause issues with some devices.

### Important Note Regarding End Devices

- **OTAA devices:** Most LoRaWAN stacks (e.g. LMIC, Arduino-LMIC, STM32WL, Zephyr, mbed-os) default to the **public sync word (0x34)**. If you set the gateway to `false`, devices must also use the **private sync word (0x12)**. In LMIC, set it in a custom init: `LMIC.radio.syncWord = 0x12;`. If a device does not support a custom sync word, it will not work with `lorawan_public = false` — test with a single device first.
- **ABP devices:** The same requirement applies — both gateway and device must use the same sync word.
- If your devices do not support the private sync word (rare today), leave the setting as `true` and accept the slightly reduced isolation.

Test the concentratord:

```bash
sudo chirpstack-concentratord-sx1302 --config /etc/chirpstack-concentratord/concentratord.toml
```

Expected output:

```
2026-02-07T10:38:03.720Z INFO  [libconcentratord::reset] Configuring reset pin, dev: /dev/gpiochip0, pin: 17
2026-02-07T10:38:03.721Z INFO  [libconcentratord::reset] Configuring sx1302 power enable pin, dev: /dev/gpiochip0, pin: 18
...
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Starting the concentrator
Opening SPI communication interface
Note: chip version is 0x10 (v1.0)
INFO: using legacy timestamp
```

Once verified, create the service:

```bash
sudo nano /etc/systemd/system/chirpstack-concentratord.service
```

```ini
[Unit]
Description=ChirpStack Concentratord SX1302
After=network.target

[Service]
ExecStart=/usr/bin/chirpstack-concentratord-sx1302 --config /etc/chirpstack-concentratord/concentratord.toml
Restart=always
User=lorawan

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now chirpstack-concentratord
sudo journalctl -u chirpstack-concentratord -f
```

Note the Gateway ID from the journal output (`gateway_id: "0016c001f11a4b01"`) — you will need it for the Concentratord web interface entry and the MQTT Forwarder.

---

### ChirpStack Gateway Bridge (reference only)

You can also use a pure UDP-based bridge. Sample configuration:

```bash
wget https://artifacts.chirpstack.io/downloads/chirpstack-gateway-bridge/chirpstack-gateway-bridge_4.1.0_linux_arm64.tar.gz
tar xvfz chirpstack-gateway-bridge_4.1.0_linux_arm64.tar.gz
sudo cp chirpstack-gateway-bridge /usr/bin/
sudo chmod +x /usr/bin/chirpstack-gateway-bridge
sudo nano /etc/chirpstack-gateway-bridge/chirpstack-gateway-bridge.toml
```

```toml
[integration.mqtt]
servers = ["tcp://127.0.0.1:1883"]
topic_prefix = "eu868"

[backend]
type = "semtech_udp"

[backend.semtech_udp]
bind = "0.0.0.0:1700"
```

```bash
sudo nano /etc/systemd/system/chirpstack-gateway-bridge.service
```

```ini
[Unit]
Description=ChirpStack Gateway Bridge
After=network.target mosquitto.service

[Service]
ExecStart=/usr/bin/chirpstack-gateway-bridge --config /etc/chirpstack-gateway-bridge/chirpstack-gateway-bridge.toml
Restart=always
User=lorawan

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable chirpstack-gateway-bridge
sudo systemctl start chirpstack-gateway-bridge
sudo systemctl status chirpstack-gateway-bridge
```

Expected output:

```
INFO[0000] starting ChirpStack Gateway Bridge            docs="https://www.chirpstack.io/gateway-bridge/" version=4.1.0
INFO[0000] backend/semtechudp: starting gateway udp listener  addr="0.0.0.0:1700"
WARN[0000] [store]    memorystore wiped                  module=mqtt
INFO[0000] integration/mqtt: connected to mqtt broker
```

---

## ChirpStack MQTT Forwarder

Note that from Gateway Bridge version 4.1.0 onward the direct Concentratord backend is not supported — do not use the Gateway Bridge for that purpose. The data flow is: Concentratord → MQTT Forwarder → MQTT broker → ChirpStack. See the [architecture diagram](https://deepwiki.com/chirpstack/chirpstack-docs/1-chirpstack-overview).

```bash
sudo nano /etc/chirpstack-mqtt-forwarder/chirpstack-mqtt-forwarder.toml
```

Configuration for the Concentratord backend:

```toml
# Logging settings.
[logging]
  level="info"
  log_to_syslog=false

# MQTT settings.
[mqtt]
  topic_prefix="eu868"
  json=false
  server="tcp://127.0.0.1:1883"
  username=""
  password=""
  qos=0
  clean_session=false
  client_id=""
  ca_cert=""
  tls_cert=""
  tls_key=""

# Backend configuration.
[backend]
  enabled="concentratord"

  [backend.filters]
    forward_crc_ok=true
    forward_crc_invalid=false
    forward_crc_missing=false
    dev_addr_prefixes=[]
    join_eui_prefixes=[]

  [backend.concentratord]
    event_url="ipc:///tmp/concentratord_event"
    command_url="ipc:///tmp/concentratord_command"

  [backend.semtech_udp]
    bind="0.0.0.0:1700"
    time_fallback_enabled=false

[metadata]
  [metadata.static]
  [metadata.commands]

[commands]

[callbacks]
  on_mqtt_connected=[]
  on_mqtt_connection_error=[]

[concentratord]
event_url = "ipc:///tmp/concentratord_event"
command_url = "ipc:///tmp/concentratord_command"

[mqtt]
server = "tcp://localhost:1883"
topic_prefix = "eu868"

[mqtt]
gateway_id = "your-gateway-id-here"
```

Test the forwarder:

```bash
chirpstack-mqtt-forwarder --config /etc/chirpstack-mqtt-forwarder/chirpstack-mqtt-forwarder.toml
```

Expected output:

```
2026-02-07T12:56:43.522Z INFO  [chirpstack_mqtt_forwarder] Starting ChirpStack MQTT Forwarder (version: 4.5.1, docs: https://www.chirpstack.io/)
2026-02-07T12:56:43.522Z INFO  [chirpstack_mqtt_forwarder::backend] Setting up ChirpStack Concentratord backend
...
2026-02-07T12:56:43.522Z INFO  [chirpstack_mqtt_forwarder::backend::concentratord] Received gateway id, gateway_id: 0016c001f11a4b01
2026-02-07T12:56:43.526Z INFO  [chirpstack_mqtt_forwarder::mqtt] Starting MQTT event loop
```

Create the service:

```bash
sudo nano /etc/systemd/system/chirpstack-mqtt-forwarder.service
```

```ini
[Unit]
Description=ChirpStack MQTT Forwarder
After=network.target

[Service]
ExecStart=/usr/bin/chirpstack-mqtt-forwarder --config /etc/chirpstack-mqtt-forwarder/chirpstack-mqtt-forwarder.toml
Restart=always
User=lorawan

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable chirpstack-mqtt-forwarder
sudo systemctl start chirpstack-mqtt-forwarder
```

Verify:

```bash
sudo systemctl status chirpstack-mqtt-forwarder
mosquitto_sub -h 127.0.0.1 -p 1883 -t '#' -v
```

When everything is working correctly, MQTT packets are forwarded and once the gateway reports status up, the system is fully operational.

---

## Importing Device Profiles

```bash
# Clone to a convenient location (does not need to be under /etc)
git clone https://github.com/chirpstack/chirpstack-device-profiles.git /opt/chirpstack/device-profiles

# Import all profiles (also updates existing ones)
chirpstack -c /etc/chirpstack import-device-profiles -d /opt/chirpstack/device-profiles
```
Now just start connecting sensors!

**_Thank you for reading!_**

---

## Tips

1. Generate the secret for the `"you must change this"` field: `openssl rand -base64 32`
2. MQTT integration example (`chirpstack.toml`):

```toml
[integration]
enabled = ["mqtt"]

[integration.mqtt]
server = "tcp://127.0.0.1:1883/"
json = true
username = ""
password = ""
qos = 0
clean_session = false
event_topic = "application/{{application_id}}/device/{{dev_eui}}/event/{{event}}"
command_topic = "application/{{application_id}}/device/{{dev_eui}}/command/{{command}}"
```


## Fusion360 drawing for 3D printable case
- Check https://a360.co/4tKQ2nZ
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
