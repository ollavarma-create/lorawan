Tässä perustasoinen ohje simppeliin asennukseen. Huomaa, että tällä konfiguraatiolla saat peruasiat toimimaan. Voit myös käyttää [sx1302\_hal](https://github.com/Lora-net/sx1302_hal) mukaista lähtökohtaa SeedStudio wm1302 :lleja määritellä UDP-pohjaisen kommunikaation, mutta omien kokemusteni mukaan etenkin Raspberry 5PI kanssa sx1302\_hal ei toimi kuin kerran oikein, eli jostain syystä toisella käynnistyskerralla (soft boot) SPI-yhteys menetetään tai se häiriintyy. Lisäksi oletus pinni 17 resetille ei toimi laisinkaan, mutta 23 tai 13 toimivat. Tästä syystä alla oleva Chirpstackin [concentratord](https://github.com/chirpstack/chirpstack-concentratord) on parempi vaihtoehto.

Projektin sivut ohjeineen ovat [tässä linkissä](https://www.chirpstack.io/).

Helpointa asennus on käyttää valmista OpenWRT-pohjaista asennusta suoraan SD-kortille. Siinä on kaikki valmiina. Minä haluan kuitenkin Grafanan ja erilaisia tietokantoja, kuten InfluxDB samaan laitteeseen ja siksi OpenWRT ei tule kyseeseen. Näin ollen käyttöjärjestelmäksi valikoituu Raspberry Pi OS Lite 64 bit.

## Raspberry asentaminen

Lataa Pi Imager ja asenna Other - Raspberry Pi OS Lite 64 bit

Imagerilla SD-kortille käyttis

Kytke näppis, ethernet-kaapli verkkoosi ja näyttö. Seuraavassa on listamuotoisesti mitä asetuksia kannattaa tässä vaiheessa tehdä:

1.  Keyboard - Other -Finnish - Finnish
2.  New username: käyttäjänimi ja salasana (muista nämä!)
3.  Login edellä tekemälläsi tunnuksella
4.  sudo apt update & sudo apt upgrade
5.  sudo raspi-config ja sieltä seuraavia asioita:
6.  Valikko 1: System - Hostname: jokin erottuva nimi laitteellesi
7.  Valikko 3: I1: SSH - Enable (pääset etänä SSH:lla laitteellesi)
8.  Valikko 3: I4: SPI Enable
9.  Valikko 3: I5: I2C Enable
10.  Valikko 3: I6: No login shell, Yes serial port interface
11.  Valikko 5: L1: fi\_FI UTF-8
12.  Valikko 5: L2: Helsinki
13.  Valikko 6: A1: Expand Filesystem
14.  Reboot
15.  Katso tässä vaiheessa minkä IP-osoitteen Raspberry sai ja voit käyttää osoitetta ssh:lla etänkoneeltasi malliin ssh \[käyttäjätunnus edeltä\]@laitteesi ip-osoite

Jos haluat asettaa tässä vaiheessa kiinteän ip-osoitteen ym. voit käyttää nmtui- työkalua. Poista ipv6 sillä kerneli ei osaa jostain syystä käsitellä ipv4 pohjaista dns-palvelinta samaan aikaan ja toimii epäluotettavasti.

1.  nmcli connection show
2.  sudo nmcli connection modify "Wired connection 1" ipv6.method "disabled"
3.  sudo nmcli connection up "Wired connection 1"

Tehdään lorawan-käyttäjä, jolloin ei tarvitse ajaa roottina eri sovelluksia.

1.  sudo adduser lorawan
2.  salasana (muista tämä!), jokin kiva nimi ja enteriä muutaman kerran
3.  sudo usermod -a -G spi,i2c,gpio,dialout,plugdev lorawan
4.  sudo nano /etc/udev/rules.d/99-spi.rules ja tiedoston sisälle rivi: SUBSYSTEM=="spidev", GROUP="spi", MODE="0660" -> Tallenna (Ctrl+O → Enter → Ctrl+X)
5.  sudo udevadm control --reload-rules && sudo udevadm trigger
6.  sudo -u lorawan ls /dev/spidev\* # pitäisi näyttää laitteet ilman "Permission denied"
7.  sudo -u lorawan ls /dev/i2c\* # pitäisi näyttää i2c-1
8.  sudo -u lorawan ls /dev/gpiochip\* # GPIO-chipit
9.  sudo adduser lorawan sudo
10.  sudo -u lorawan -s # tai su - lorawan

Nyt olet superuserina käyttäjänimellä lorawan. Asenna git ja muut peruspaketit lorawan-käyttäjälle.

1.  sudo apt install git build-essential -y
2.  sudo apt install git build-essential libgpiod-dev -y

___

## Chirpstack Dockerista

Minä olen törmännyt Dockerin kanssa liian usein ongelmiin silloin kun pitää saada kontit puhelemaan keskenään, eli jos haluat oikaista, käytä Dockeria:

```
# Docker + Compose asennus (jos ei jo ole)
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh
sudo usermod -aG docker $USER
# Uudelleenkirjautuminen tai newgrp docker
sudo apt install docker-compose-plugin -y   # Compose v2
```

Seuraavassa perinteisempi asennustapa.

## Chirpstack SqLite-tietokannalla

Seuraavana asennetaan [Chirpstack](https://www.chirpstack.io/docs/chirpstack/downloads.html) käyttäen SqLite-tietokantaa. Vaihtoehtona etenkin suurempiin toteutuksiin on PostGresSQL, mutta uusin versio 17 voi muodostua ongelmaksi asennusvaiheessa ainakin omien testieni perusteella. Jostain syystä Chirpstack ei osaa luoda tarvittavia tauluja uuteen tietokantaan. Lataa uusin versio!

1.  wget [https://artifacts.chirpstack.io/downloads/chirpstack/chirpstack\_4.16.2\_sqlite\_linux\_arm64.tar.gz](https://artifacts.chirpstack.io/downloads/chirpstack/chirpstack_4.16.2_sqlite_linux_arm64.tar.gz)
2.  tar xvfz chirpstack\_4.16.2\_sqlite\_linux\_arm64.tar.gz chirpstack
3.  sudo cp chirpstack /usr/bin/
4.  wget [https://raw.githubusercontent.com/chirpstack/chirpstack/master/chirpstack/configuration/chirpstack.toml](https://raw.githubusercontent.com/chirpstack/chirpstack/master/chirpstack/configuration/chirpstack.toml) \-O /tmp/chirpstack.toml
5.  wget [https://github.com/chirpstack/chirpstack-concentratord/blob/master/chirpstack-concentratord-sx1302/config/channels\_eu868.toml](https://github.com/chirpstack/chirpstack-concentratord/blob/master/chirpstack-concentratord-sx1302/config/channels_eu868.toml) \-O /tmp/channels\_eu868.toml
6.  sudo cp /tmp/chirpstack.toml /etc/chirpstack/chirpstack.toml
7.  sudo cp /tmp/channels\_eu868.toml /etc/chirpstack/channels\_eu868.toml
8.  sudo nano /etc/chirpstack/chirpstack.toml

Vaihtoehtoisesti voit laittaa channels\_eu868.toml sisällön chirpstack.toml-tiedoston loppuun myös.

Huom! Jos haluat käyttää muita alueita kuin eu868, kopioi kyseisten alueiden kanavattiedostot ja tee asetukset sen mukaisesti.

Tiedostossa \[postgresql\]-osion aktiiviset rivit kokonaan #-merkillä. Lisäksi tarkista:

1.  Varmista Redis: \[redis\] servers = \["[localhost:6379](http://localhost:6379/)"\]
2.  Luo tietokannalle hakemisto sudo mkdir -p /var/lib/chirpstack
3.  sudo chown lorawan:lorawan /var/lib/chirpstack

Lisää tiedoston loppuun:

```
[sqlite]
# Sqlite DB path.
# Make sure the path exists and that the Chirpstack process has read-write access.
# If the database file does not exist, it will be created the first time ChirpStack starts.
path = "sqlite:///var/lib/chirpstack/chirpstack.db"
```

Pidä tässä vaiheessa integration ainostaan mqtt:lle

```
[integration]
enabled = ["mqtt"]

[integration.mqtt]
server = "tcp://127.0.0.1:1883"
json = true

[gateway.backend.mqtt]
topic_prefix = "eu868"
```

Testaa toiminta: chirpstack --config /etc/chirpstack

```
chirpstack --config /etc/chirpstack
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

Sitten asennetaan Redis:

```
sudo apt update
sudo apt install redis-server
sudo systemctl start redis-server
sudo systemctl enable redis-server
sudo systemctl status redis-server
```

Lisää nano /etc/chirpstack/region\_eu868.toml tiedostoon:

```
# This file contains an example EU868 configuration.
[[regions]]

  # Name is an user-defined identifier for this region.
  name="eu868"

  # Common-name refers to the common-name of this region as defined by
  # the LoRa Alliance.
  common_name="EU868"


  # Gateway configuration.
  [regions.gateway]

    # Force gateways as private.
    #
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
        #
        # 0: at most once
        # 1: at least once
        # 2: exactly once
        #
        # Note: an increase of this value will decrease the performance.
        # For more information: https://www.hivemq.com/blog/mqtt-essentials-part-6-mqtt-quality-of-service-levels
        qos=0

        # Clean session
        #
        # Set the "clean session" flag in the connect message when this client
        # connects to an MQTT broker. By setting this flag you are indicating
        # that no messages saved by the broker for this client should be delivered.
        clean_session=true

        # Client ID
        #
        # Set the client id to be used by this client when connecting to the MQTT
        # broker. A client id must be no longer than 23 characters. When left blank,
        # a random id will be generated. This requires clean_session=true.
        client_id=""

        # CA certificate file (optional)
        #
        # Use this when setting up a secure connection (when server uses ssl://...)
        # but the certificate used by the server is not trusted by any CA certificate
        # on the server (e.g. when self generated).
        ca_cert=""

        # TLS certificate file (optional)
        tls_cert=""

        # TLS key file (optional)
        tls_key=""


    # Gateway channel configuration.
    #
    # Note: this configuration is only used in case the gateway is using the
    # ChirpStack Concentratord daemon. In any other case, this configuration 
    # is ignored.
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


  # Region specific network configuration.
  [regions.network]
    
    # Installation margin (dB) used by the ADR engine.
    #
    # A higher number means that the network-server will keep more margin,
    # resulting in a lower data-rate but decreasing the chance that the
    # device gets disconnected because it is unable to reach one of the
    # surrounded gateways.
    installation_margin=10

    # RX window (Class-A).
    #
    # Set this to:
    # 0: RX1 / RX2
    # 1: RX1 only
    # 2: RX2 only
    rx_window=0

    # RX1 delay (1 - 15 seconds).
    rx1_delay=1

    # RX1 data-rate offset
    rx1_dr_offset=0

    # RX2 data-rate
    rx2_dr=0

    # RX2 frequency (Hz)
    rx2_frequency=869525000

    # Prefer RX2 on RX1 data-rate less than.
    #
    # Prefer RX2 over RX1 based on the RX1 data-rate. When the RX1 data-rate
    # is smaller than the configured value, then the Network Server will
    # first try to schedule the downlink for RX2, failing that (e.g. the gateway
    # has already a payload scheduled at the RX2 timing) it will try RX1.
    rx2_prefer_on_rx1_dr_lt=0

    # Prefer RX2 on link budget.
    #
    # When the link-budget is better for RX2 than for RX1, the Network Server will first
    # try to schedule the downlink in RX2, failing that it will try RX1.
    rx2_prefer_on_link_budget=false

    # Downlink TX Power (dBm)
    #
    # When set to -1, the downlink TX Power from the configured band will
    # be used.
    #
    # Please consult the LoRaWAN Regional Parameters and local regulations
    # for valid and legal options. Note that the configured TX Power must be
    # supported by your gateway(s).
    downlink_tx_power=-1

    # ADR is disabled.
    adr_disabled=false

    # Minimum data-rate.
    min_dr=0

    # Maximum data-rate.
    max_dr=5


    # Rejoin-request configuration (LoRaWAN 1.1)
    [regions.network.rejoin_request]

      # Request devices to periodically send rejoin-requests.
      enabled=false

      # The device must send a rejoin-request type 0 at least every 2^(max_count_n + 4)
      # uplink messages. Valid values are 0 to 15.
      max_count_n=0

      # The device must send a rejoin-request type 0 at least every 2^(max_time_n + 10)
      # seconds. Valid values are 0 to 15.
      #
      # 0  = roughly 17 minutes
      # 15 = about 1 year
      max_time_n=0
    

    # Class-B configuration.
    [regions.network.class_b]

      # Ping-slot data-rate. 
      ping_slot_dr=0

      # Ping-slot frequency (Hz)
      #
      # set this to 0 to use the default frequency plan for the configured region
      # (which could be frequency hopping).
      ping_slot_frequency=0


    # Below is the common set of extra channels. Please make sure that these
    # channels are also supported by the gateways.
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

Seuraavaksi mene webbiselaimella Raspberrysi osoitteeseen ja lisää :8080 osoitteen perään, esimerkiksi http://192.168.x.y:8080. Oletus on admin/admin.

**Vaihda Adminin salasana heti kohdasta Users - Admin - Change password.**

Tehdään tästä toiminnan tarkastuksen jälkeen palvelu:

1.  sudo nano /etc/systemd/system/chirpstack.service

```
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

1.  sudo systemctl daemon-reload
2.  sudo systemctl enable --now chirpstack
3.  sudo systemctl status chirpstack

Tarkista, että palvelu toimii:

```
sudo systemctl status chirpstack
chirpstack.service - ChirpStack LoRaWAN Network Server
     Loaded: loaded (/etc/systemd/system/chirpstack.service; enabled; preset: enabled)
     Active: active (running) since Sat 2026-02-07 12:58:56 EET; 2s ago
 Invocation: 282560bb9d4d4573836b5febd5ce6285
   Main PID: 3914 (chirpstack)
      Tasks: 10 (limit: 9577)
        CPU: 44ms
     CGroup: /system.slice/chirpstack.service
             └─3914 /usr/bin/chirpstack --config /etc/chirpstack
....
```

___

## MQTT-brokerin asennus

Seuraavaksi asennetaan Mosquitto mqtt-brokeri. Tarvitset tämän siksi, että data kulkee Concentratord → MQTT Forwarder → MQTT broker → ChirpStack. Minä käytän MQTT:tä esimerkiksi WiFi-pohjaisten sensoreideni liikenteeseen ja sitä kautta InfluxDB ja Grafanaan. MQTT on de facto IoT-pohjaisissa järjestelmissä. Tässä ohjeessa brokeri on ilman salasanaa tai TLS-suojausta, mutta tuotannossa kannattaa ne ottaa käyttöön.

1.  sudo apt install mosquitto mosquitto-clients -y
2.  sudo nano /etc/mosquitto/conf.d/local.conf

```
listener 1883
allow_anonymous true
```

1.  sudo systemctl restart mosquitto
2.  sudo systemctl status mosquitto
3.  sudo nano /etc/chirpstack/chirpstack.tomlsudo systemctl enable --now mosquitto
4.  sudo nano /etc/chirpstack/chirpstack.toml
5.  chirpstack --config /etc/chirpstack

Katso näkyykö aloituksessa yhteys mqtt-brokeriin

2026-02-07T10:11:29.452233Z INFO chirpstack::integration::mqtt: Connecting to MQTT broker server\_uri=tcp://localhost:1883/ client\_id=a118dd346c97cfbf clean\_session=false

___

## ChirpStack Concentratord WM1302:lle

Lataa uusin [concentratord](https://github.com/chirpstack/chirpstack-concentratord) arm64:lle:

1.  wget [https://artifacts.chirpstack.io/downloads/chirpstack-concentratord/chirpstack-concentratord-sx1302\_4.6.0\_linux\_arm64.tar.gz](https://artifacts.chirpstack.io/downloads/chirpstack-concentratord/chirpstack-concentratord-sx1302_4.6.0_linux_arm64.tar.gz)
2.  tar xvfz chirpstack-concentratord-sx1302\_4.6.0\_linux\_arm64.tar.gz
3.  sudo cp chirpstack-concentratord-sx1302 /usr/bin/
4.  sudo mkdir -p /etc/chirpstack-concentratord
5.  sudo nano /etc/chirpstack-concentratord/concentratord.toml

Lisää minimitiedot:

```
# Concentratord configuration for Seeed WM1302 Pi HAT (SPI)

[gateway]
  antenna_gain = 1
  lorawan_public = false
  region = "EU868"
  model = "seeed_wm1302"
  gateway_id = "sun id"
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

Huom! Asetus lorawan\_public = false tekee gatwaysta aidosti privaatin, eikä kuuntele julkisia headereita (0x34), mutta saattaa aiheuttaa ongelmia joidenkin laitteiden kanssa.

### Tärkeä huomio laitteiden (end-devices) puolelta

-   **OTAA-laitteet**: Useimmissa LoRaWAN-stackeissa (esim. LMIC, Arduino-LMIC, STM32WL, Zephyr, mbed-os) sync word on automaattisesti **julkinen (0x34)** oletuksena. Jos muutat gatewayn false:ksi, laitteiden pitää myös käyttää **private sync wordiä (0x12)**. LMIC:ssä: aseta LMIC\_setDrTxpow() tai custom initissä LMIC.radio.syncWord = 0x12; Jos laite ei tue custom sync wordiä, se ei toimi false:lla → testaa ensin yhdellä laitteella.
-   **ABP-laitteet**: Sama vaatimus pätee – molempien pitää olla samalla sync wordillä.
-   Jos laitteesi eivät tue private sync wordiä (harvinaista nykyään), jätä asetus true:ksi ja hyväksy pienempi erottuvuus.

1.  Testaa toiminta: chirpstack-concentratord-sx1302 --config /etc/chirpstack-concentratord/concentratord.toml

```
sudo chirpstack-concentratord-sx1302 --config /etc/chirpstack-concentratord/concentratord.toml
2026-02-07T10:38:03.720Z INFO  [libconcentratord::reset] Configuring reset pin, dev: /dev/gpiochip0, pin: 17
2026-02-07T10:38:03.721Z INFO  [libconcentratord::reset] Configuring sx1302 power enable pin, dev: /dev/gpiochip0, pin: 18
2026-02-07T10:38:03.721Z INFO  [libconcentratord::reset] Configuring sx1261 reset pin, dev: /dev/gpiochip0, pin: 5
2026-02-07T10:38:03.721Z INFO  [chirpstack_concentratord_sx1302::cmd::root] Starting Concentratord SX1302 (version: 4.6.0, docs: https://www.chirpstack.io/docs/chirpstack-concentratord/)
2026-02-07T10:38:03.721Z INFO  [libconcentratord::reset] Enabling concentrator power
2026-02-07T10:38:03.821Z INFO  [libconcentratord::reset] Triggering sx130x reset
2026-02-07T10:38:04.021Z INFO  [libconcentratord::reset] Triggering sx1261 reset
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Setting i2c device path, path: /dev/i2c-1
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Setting i2c temperature sensor address, address: 57
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Setting board configuration, lorawan_public: true, clock_source: 0, com_type: Spi, com_path: /dev/spidev0.0
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Setting up fine timestamp, enable: false
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] LBT is disabled
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Setting up concentrator channels
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring radio, radio: 0, enabled: false, center_freq: 0, type: SX1250
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring radio, radio: 1, enabled: false, center_freq: 0, type: SX1250
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Setting up concentrator channels
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring multi-SF LoRa channel, channel: 0, enabled: false, freq: 0, rf_chain: 0, if_freq: 0
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring multi-SF LoRa channel, channel: 1, enabled: false, freq: 0, rf_chain: 0, if_freq: 0
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring multi-SF LoRa channel, channel: 2, enabled: false, freq: 0, rf_chain: 0, if_freq: 0
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring multi-SF LoRa channel, channel: 3, enabled: false, freq: 0, rf_chain: 0, if_freq: 0
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring multi-SF LoRa channel, channel: 4, enabled: false, freq: 0, rf_chain: 0, if_freq: 0
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring multi-SF LoRa channel, channel: 5, enabled: false, freq: 0, rf_chain: 0, if_freq: 0
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring multi-SF LoRa channel, channel: 6, enabled: false, freq: 0, rf_chain: 0, if_freq: 0
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring multi-SF LoRa channel, channel: 7, enabled: false, freq: 0, rf_chain: 0, if_freq: 0
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring Std LoRa channel, enabled: false, freq: 0, rf_chain: 0, if_freq: 0
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring FSK channel, enabled: false, freq: 0, rf_chain: 0, if_freq: 0
2026-02-07T10:38:04.221Z INFO  [chirpstack_concentratord_sx1302::concentrator] Starting the concentrator
Opening SPI communication interface
Note: chip version is 0x10 (v1.0)
INFO: using legacy timestamp
```

Tehdään tästä palvelu tarkistuksen jälkeen: sudo nano /etc/systemd/system/chirpstack-concentratord.service

```
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

1.  sudo systemctl daemon-reload
2.  sudo systemctl enable --now chirpstack-concentratord
3.  sudo journalctl -u chirpstack-concentratord -f

```
sudo journalctl -u chirpstack-concentratord -f
helmi 07 12:52:50 pi-loragw chirpstack-concentratord-sx1302[3501]: 2026-02-07T10:52:50.760Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring multi-SF LoRa channel, channel: 6, enabled: true, freq: 867700000, rf_chain: 0, if_freq: 200000
helmi 07 12:52:50 pi-loragw chirpstack-concentratord-sx1302[3501]: 2026-02-07T10:52:50.760Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring multi-SF LoRa channel, channel: 7, enabled: true, freq: 867900000, rf_chain: 0, if_freq: 400000
helmi 07 12:52:50 pi-loragw chirpstack-concentratord-sx1302[3501]: 2026-02-07T10:52:50.760Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring Std LoRa channel, enabled: false, freq: 0, rf_chain: 0, if_freq: 0
helmi 07 12:52:50 pi-loragw chirpstack-concentratord-sx1302[3501]: 2026-02-07T10:52:50.760Z INFO  [chirpstack_concentratord_sx1302::concentrator] Configuring FSK channel, enabled: false, freq: 0, rf_chain: 0, if_freq: 0
helmi 07 12:52:50 pi-loragw chirpstack-concentratord-sx1302[3501]: 2026-02-07T10:52:50.760Z INFO  [chirpstack_concentratord_sx1302::concentrator] Starting the concentrator
helmi 07 12:52:50 pi-loragw chirpstack-concentratord-sx1302[3501]: Opening SPI communication interface
helmi 07 12:52:53 pi-loragw chirpstack-concentratord-sx1302[3501]: 2026-02-07T10:52:53.123Z INFO  [chirpstack_concentratord_sx1302::cmd::root] Gateway ID retrieved, gateway_id: "0016c001f11a4b01"
helmi 07 12:52:53 pi-loragw chirpstack-concentratord-sx1302[3501]: 2026-02-07T10:52:53.123Z INFO  [libconcentratord::jitqueue] Initializing JIT queue, capacity: 32
helmi 07 12:52:53 pi-loragw chirpstack-concentratord-sx1302[3501]: 2026-02-07T10:52:53.123Z INFO  [libconcentratord::events] Creating socket for publishing events, bind: ipc:///tmp/concentratord_event
helmi 07 12:52:53 pi-loragw chirpstack-concentratord-sx1302[3501]: 2026-02-07T10:52:53.123Z INFO  [libconcentratord::commands] Creating socket for receiving commands, bind: ipc:///tmp/concentratord_command
helmi 07 12:53:23 pi-loragw chirpstack-concentratord-sx1302[3501]: 2026-02-07T10:53:23.124Z INFO  [libconcentratord::events] Publishing stats event, rx_received: 0, rx_received_ok: 0, tx_received: 0, tx_emitted: 0
helmi 07 12:53:53 pi-loragw chirpstack-concentratord-sx1302[3501]: 2026-02-07T10:53:53.126Z INFO  [libconcentratord::events] Publishing stats event, rx_received: 0, rx_received_ok: 0, tx_received: 0, tx_emitted: 0 
```

Etsi edellä olevasta Gateway ID: 00.... tarvitsemme sitä Concentratordiin (web liittymässä) ja MQTT Forwardiin.
___

### ChirpStack Bridge (malliksi)

Voit myös käyttää puhtaasi UDP-pohjaista siltaa. Seuraavassa malliconfiguraatiota.

```
wget https://artifacts.chirpstack.io/downloads/chirpstack-gateway-bridge/chirpstack-gateway-bridge_4.1.0_linux_arm64.tar.gz
tar xvfz chirpstack-gateway-bridge_4.1.0_linux_arm64.tar.gz
sudo cp chirpstack-gateway-bridge /usr/bin/
sudo chmod +x /usr/bin/chirpstack-gateway-bridge
```

1.  sudo nano etc/chirpstack-gateway-bridge/chirpstack-gateway-bridge.toml

```
[integration.mqtt]
servers = ["tcp://127.0.0.1:1883"]
topic_prefix = "eu868"

[backend]
type = "semtech_udp"

[backend.semtech_udp]
bind = "0.0.0.0:1700"
```

1.  sudo nano /etc/systemd/system/chirpstack-gateway-bridge.service

```
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

käynnistä ja testaa

```
sudo systemctl daemon-reload
sudo systemctl enable chirpstack-gateway-bridge
sudo systemctl start chirpstack-gateway-bridge
sudo systemctl status chirpstack-gateway-bridge
```

___

```
INFO[0000] starting ChirpStack Gateway Bridge            docs="https://www.chirpstack.io/gateway-bridge/" version=4.1.0
INFO[0000] backend/semtechudp: starting gateway udp listener  addr="0.0.0.0:1700"
WARN[0000] [store]    memorystore wiped                  module=mqtt
INFO[0000] integration/mqtt: connected to mqtt broker
```

___

## ChirpStack MQTT Forwarder

Huoma, että ChirpStack Bridge versiosta 4.1.0 alkaen ei suoraa tue backendiä, eli älä käytä tähän tarkoitukseen sitä. Toimintakaavion näet [tästä](https://deepwiki.com/chirpstack/chirpstack-docs/1-chirpstack-overview). Concentratord → MQTT Forwarder → MQTT broker → ChirpStack.

1.  sudo nano /etc/systemd/system/chirpstack-mqtt-forwarder.service

Säännöt pelkälle concentratord backendille:

```
# Logging settings.
[logging]

  # Log level.
  #
  # Valid options are:
  #   * TRACE
  #   * DEBUG
  #   * INFO
  #   * WARN
  #   * ERROR
  #   * OFF
  level="info"

  # Log to syslog.
  #
  # If set to true, log messages are being written to syslog instead of stdout.
  log_to_syslog=false


# MQTT settings.
[mqtt]

  # Topic prefix.
  #
  # ChirpStack MQTT Forwarder publishes to the following topics:
  #
  #  * [Prefix/]gateway/[Gateway ID]/event/[Event]
  #  * [Prefix/]gateway/[Gateway ID]/state/[State]
  #
  # And subscribes to the following topic:
  #
  #  * [Prefix/]gateway/[Gateway ID]/command/[Command]
  #
  # The topic prefix can be used to define the region of the gateway.
  # Note, there is no need to add a trailing '/' to the prefix. The trailing
  # '/' is automatically added to the prefix if it is configured.
  topic_prefix="eu868"

  # Use JSON encoding instead of Protobuf (binary).
  #
  # Note, only use this for debugging purposes.
  json=false

  # MQTT server (e.g. scheme://host:port where scheme is tcp, ssl, ws or wss)
  server="tcp://127.0.0.1:1883"

  # Connect with the given username (optional)
  username=""

  # Connect with the given password (optional)
  password=""

  # Quality of service level
  #
  # 0: at most once
  # 1: at least once
  # 2: exactly once
  #
  # Note: an increase of this value will decrease the performance.
  # For more information: https://www.hivemq.com/blog/mqtt-essentials-part-6-mqtt-quality-of-service-levels
  qos=0

  # Clean session
  #
  # Set the "clean session" flag in the connect message when this client
  # connects to an MQTT broker. By setting this flag you are indicating
  # that no messages saved by the broker for this client should be delivered.
  clean_session=false

  # Client ID
  #
  # Set the client id to be used by this client when connecting to the MQTT
  # broker. A client id must be no longer than 23 characters. If left blank,
  # a random id will be generated by ChirpStack.
  client_id=""

  # Keep alive interval.
  #
  # This defines the maximum time that that should pass without communication
  # between the client and server.
#  keep_alive_interval=""

  # CA certificate file (optional)
  #
  # Use this when setting up a secure connection (when server uses ssl://...)
  # but the certificate used by the server is not trusted by any CA certificate
  # on the server (e.g. when self generated).
  ca_cert=""

  # TLS certificate file (optional)
  tls_cert=""

  # TLS key file (optional)
  tls_key=""

  # Reconnect interval.
  #
  # This defines the reconnection interval to the MQTT broker in case of
  # network issues.
#  reconnect_interval=""


# Backend configuration.
[backend]

  # Enabled backend.
  #
  # Set this to the backend that must be used by the ChirpStack MQTT Forwarder.
  # Valid options are:
  #   * concentratord
  #   * semtech_udp
  enabled="concentratord"

  # Filters.
  [backend.filters]

    # Forward CRC ok.
    forward_crc_ok=true

    # Forward CRC invalid.
    forward_crc_invalid=false

    # Forward CRC missing.
    forward_crc_missing=false

    # DevAddr prefix filters.
    #
    # Example configuration:
    # dev_addr_prefixes=["0000ff00/24"]
    #
    # The above filter means that the 24MSB of 0000ff00 will be used to
    # filter DevAddrs. Uplinks with DevAddrs that do not match any of the
    # configured filters will not be forwarded. Leaving this option empty
    # disables filtering on DevAddr.
    dev_addr_prefixes=[
    ]

    # JoinEUI prefix filters.
    #
    # Example configuration:
    # join_eui_prefixes=["0000ff0000000000/24"]
    #
    # The above filter means that the 24MSB of 0000ff0000000000 will be used
    # to filter JoinEUIs. Uplinks with JoinEUIs that do not match any of the
    # configured filters will not be forwarded. Leaving this option empty
    # disables filtering on JoinEUI.
    join_eui_prefixes=[
    ]


  # ChirpStack Concentratord backend configuration.
  [backend.concentratord]

    # Event API URL.
    event_url="ipc:///tmp/concentratord_event"

    # Command API URL.
    command_url="ipc:///tmp/concentratord_command"


  # Semtech UDP backend configuration.
  [backend.semtech_udp]

    # ip:port to bind the UDP listener to.
    #
    # Example: 0.0.0.0:1700 to listen on port 1700 for all network interfaces.
    # This is the listener to which the packet-forwarder forwards its data
    # so make sure the 'serv_port_up' and 'serv_port_down' from your
    # packet-forwarder matches this port.
    bind="0.0.0.0:1700"

    # Time fallback.
    #
    # In case the UDP packet-forwarder does not set the 'time' field, then the
    # server-time will be used as fallback if this option is enabled.
    time_fallback_enabled=false


# Gateway metadata configuration.
[metadata]

  # Static key / value metadata.
  [metadata.static]
      
    # Example:
    # serial_number="1234"


  # Commands returning metadata.
  [metadata.commands]

    # Example:
    # datetime=["date", "-R"]


# Executable commands.
[commands]

  # Example:
  # reboot=["/usr/bin/reboot"]


# Callback commands.
#
# These are commands that are triggered by certain events (e.g. MQTT connected
# or error). These commands are intended to e.g. trigger a LED of a gateway.
# Commands are configured as an array, where the first item is the path to the
# command, and the (optional) remaining elements are the arguments. An empty
# array disables the callback.
[callbacks]

  # On MQTT connected.
  on_mqtt_connected=[]

  # On MQTT connection error.
  on_mqtt_connection_error=[][concentratord]
event_url = "ipc:///tmp/concentratord_event"
command_url = "ipc:///tmp/concentratord_command"

[mqtt]
server = "tcp://localhost:1883"
topic_prefix = "eu868"

[mqtt]
gateway_id = "tähän gw id:si"
```

Testaa toiminta: chirpstack-mqtt-forwarder --config /etc/chirpstack-mqtt-forwarder/chirpstack-mqtt-forwarder.toml

```
chirpstack-mqtt-forwarder --config /etc/chirpstack-mqtt-forwarder/chirpstack-mqtt-forwarder.toml
2026-02-07T12:56:43.522Z INFO  [chirpstack_mqtt_forwarder] Starting ChirpStack MQTT Forwarder (version: 4.5.1, docs: https://www.chirpstack.io/)
2026-02-07T12:56:43.522Z INFO  [chirpstack_mqtt_forwarder::backend] Setting up ChirpStack Concentratord backend
2026-02-07T12:56:43.522Z INFO  [chirpstack_mqtt_forwarder::backend::concentratord] Setting up ChirpStack Concentratord backend
2026-02-07T12:56:43.522Z INFO  [chirpstack_mqtt_forwarder::backend::concentratord] Connecting to Concentratord event API, event_url: ipc:///tmp/concentratord_event
2026-02-07T12:56:43.522Z INFO  [chirpstack_mqtt_forwarder::backend::concentratord] Connecting to Concentratord command API, command_url: ipc:///tmp/concentratord_command
2026-02-07T12:56:43.522Z INFO  [chirpstack_mqtt_forwarder::backend::concentratord] Reading gateway id
2026-02-07T12:56:43.522Z INFO  [chirpstack_mqtt_forwarder::backend::concentratord] Received gateway id, gateway_id: 0016c001f11a4b01
2026-02-07T12:56:43.522Z INFO  [chirpstack_mqtt_forwarder::backend] Received Gateway ID from backend, gateway_id: 0016c001f11a4b01
2026-02-07T12:56:43.526Z INFO  [chirpstack_mqtt_forwarder::mqtt] Starting MQTT event loop
2026-02-07T12:56:43.527Z INFO  [chirpstack_mqtt_forwarder::mqtt] Subscribing to command topic, topic: eu868/gateway/0016c001f11a4b01/command/+
2026-02-07T12:56:43.527Z INFO  [chirpstack_mqtt_forwarder::mqtt] Sending conn state, topic: eu868/gateway/0016c001f11a4b01/state/conn
2026-02-07T12:56:53.428Z INFO  [chirpstack_mqtt_forwarder::backend::concentratord] received gateway stats
2026-02-07T12:56:53.428Z INFO  [chirpstack_mqtt_forwarder::mqtt] Sending gateway stats event, topic: eu868/gateway/0016c001f11a4b01/event/stats
2026-02-07T12:57:23.429Z INFO  [chirpstack_mqtt_forwarder::backend::concentratord] received gateway stats
2026-02-07T12:57:23.429Z INFO  [chirpstack_mqtt_forwarder::mqtt] Sending gateway stats event, topic: eu868/gateway/0016c001f11a4b01/event/stats 
```

Tee service: sudo nano /etc/systemd/system/chirpstack-mqtt-forwarder.service

```
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

1.  sudo systemctl daemon-reload
2.  sudo systemctl enable chirpstack-mqtt-forwarder
3.  sudo systemctl start chirpstack-mqtt-forwarder

Testaa:

1.  sudo systemctl status chirpstack-mqtt-forwarder
2.  mosquitto\_sub -h 127.0.0.1 -p 1883 -t '#' -v

Mikäli kaikki toimii oikein, mqtt-paketteja forwardoidaan oikein ja kun gatewaylta tulee status up, hommat alkavat pelittämään.

```
mosquitto_sub -h 127.0.0.1 -p 1883 -t '#' -v
eu868/gateway/0016c001f11a4b01/state/conn �0016c001f11a4b01
eu868/gateway/0016c001f11a4b01/event/stats 
                                          ΅����ӥR
mqtt_forwarder_version4.5.1R

hal_versionVersion: 2.1.0;R
concentratord_version4.6.0R
config_versionR
concentrator_temp45.9375R
model
     seeed_wm1302�0016c001f11a4b0�� 
```

## Device profiilien tuonti

```
# Kloonaa repoon sopivaan paikkaan (ei tarvitse olla /etc alle)
git clone https://github.com/chirpstack/chirpstack-device-profiles.git /opt/chirpstack/device-profiles

# Tuo profiilit kerralla (tai päivittää olemassa olevat)
chirpstack -c /etc/chirpstack import-device-profiles -d /opt/chirpstack/device-profiles
```

Sitten vain sensoreita liittämään!

**_Kiitos lukemisesta!_**

___

Vinkkejä:

1.  Secret luominen kohtaan "you must change this" -> openssl rand -base64 32
2.  mqtt integraatioesimerkki (chirpstack.toml):

```
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