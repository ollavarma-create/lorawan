# OTA-päivitettävä ESP32 LoRaWAN-sensori – käyttöohje

Tämä ohje kattaa kaiken ensimmäisestä USB-flashauksesta OTA-päivityksen suorittamiseen.
Laite on ESP32-WROOM-32D + RFM95W + BME280, ohjelmisto perustuu ArduinoOTA-kirjastoon
ja Chirpstack v4 -verkonhallintaan.

---

## Sisältö

1. [Ensimmäinen flashaus USB:llä](#1-ensimmäinen-flashaus-usbllä)
2. [Arduino IDE – pakolliset asetukset](#2-arduino-ide--pakolliset-asetukset)
3. [Laitteen rekisteröinti Chirpstackiin](#3-laitteen-rekisteröinti-chirpstackiin)
4. [Downlink-komennot – pikaopas](#4-downlink-komennot--pikaopas)
5. [OTA-päivitys vaihe vaiheelta](#5-ota-päivitys-vaihe-vaiheelta)
6. [espota.py – päivitys suoraan IP-osoitteella](#6-espotapy--päivitys-suoraan-ip-osoitteella)
7. [Tyypilliset virheet ja ratkaisut](#7-tyypilliset-virheet-ja-ratkaisut)
8. [Downlink-komentojen HEX-referenssi](#8-downlink-komentojen-hex-referenssi)

---

## 1. Ensimmäinen flashaus USB:llä

OTA vaatii että laite on flashattu USB:llä vähintään kerran oikealla
partitiotaululla. Myöhemmät päivitykset onnistuvat langattomasti.

### Vaiheet

1. Kytke ESP32 USB-kaapelilla kehityskoneeseen
2. Avaa sketch Arduino IDE:ssä
3. Muokkaa `settings.h` ennen käännöstä:
   - `DEFAULT_WIFI_SSID` ja `DEFAULT_WIFI_PASS` → oman verkon tiedot
   - `DEFAULT_JOIN_EUI` ja `DEFAULT_APP_KEY` → Chirpstackin arvot
4. Aseta partitiotaulu (katso luku 2)
5. Valitse oikea portti: **Tools → Port → /dev/ttyUSB0** (tai vastaava)
6. Paina **Upload** (Ctrl+U)

### settings.h – AppKey-muoto

Chirpstackin 32-merkkinen hex-jono muunnetaan C-taulukkoon pareittain:

```
Chirpstack:  830c0fd67327528c293e05a931c088ab

settings.h:  { 0x83, 0x0c, 0x0f, 0xd6, 0x73, 0x27, 0x52, 0x8c,
               0x29, 0x3e, 0x05, 0xa9, 0x31, 0xc0, 0x88, 0xab }
```

---

## 2. Arduino IDE – pakolliset asetukset

### Partitiotaulu (kriittinen OTA:lle)

Oletuspartitiotaulu jakaa flashin tasan kahtia (~630 kB per osio).
Sketchi on yli 1 MB, joten se ei mahdu OTA-partitioon oletusasetuksilla.

**Tools → Partition Scheme → `Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)`**

| Asetus | Arvo |
|---|---|
| Board | ESP32-WROOM-DA Module |
| Partition Scheme | **Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)** |
| Upload Speed | 921600 |
| Flash Size | 4MB (32Mb) |

> **Huom:** Jos partitiotaulu on väärä, OTA-päivitys epäonnistuu hiljaisesti
> tai laite käynnistyy vanhan firmwaren kanssa päivityksen jälkeen.

### Akku-ADC pinnit WROOM-32D:llä

GPIO1 = TXD0 (sarjaportti) – ei sovi ADC:ksi.
WiFi estää ADC2-pinnit (GPIO 0,2,4,12–15,25–27) OTA-tilan aikana.

Käytä `settings.h`:ssa:
```cpp
#define BATT_GND_GPIO   32    // GPIO-ohjattu GND
#define BATT_ADC_GPIO   34    // ADC1, ei konfliktia WiFin kanssa
#define BATT_ADC_MAX  4095
```

---

## 3. Laitteen rekisteröinti Chirpstackiin

### 3a. Lue devEUI serial monitorista

Avaa **Tools → Serial Monitor**, nopeus **115200 baud**.
Nollaa laite – serial monitoriin ilmestyy:

```
=== LoRaWAN OTAA + OTA | FW 1.1.0 ===
devEUI (rekisteroi Chirpstackiin): 9C:9C:1F:FF:FE:C5:D4:A8
```

Kopioi devEUI talteen.

### 3b. Lisää laite Chirpstackiin

**Applications → [sovelluksesi] → Add device**

| Kenttä | Arvo |
|---|---|
| Name | Esim. `TestiChip` |
| DevEUI | devEUI ilman kaksoispisteet, esim. `9C9C1FFFFEC5D4A8` |
| Device profile | Oma profiili, esim. `OTA_ESP32_ULP_BME280` |

### 3c. Aseta OTAA-avaimet

**Laite → OTAA keys → Application key** = sama 32-merkkinen hex kuin `settings.h`:ssa.

### 3d. Lisää Codec

**Device Profiles → [profiili] → Codec → Custom JavaScript**

Liitä `chirpstack_decoder_v2.js` sisältö kenttään ja tallenna.

### 3e. Measurements-välilehti

Kun laite on lähettänyt ensimmäisen uplinkin, siirry
**Device Profiles → [profiili] → Measurements**.

Aseta kaikille kentille tyyppi **Gauge** ja nimet:

| Key | Kind | Measurement name |
|---|---|---|
| `temp` | Gauge | Lämpötila |
| `hum` | Gauge | Kosteus |
| `pres` | Gauge | Ilmanpaine |
| `batt_pct` | Gauge | Akku % |
| `batt_v` | Gauge | Akku V |

### 3f. Location-tagi

**Laite → Configuration → Tags → Lisää:**

| Key | Value |
|---|---|
| `location` | Esim. `tyohuone` |

Tagi näkyy InfluxDB:ssä automaattisesti Telegraf-integraation kautta.

---

## 4. Downlink-komennot – pikaopas

Komennot lähetetään: **Applications → laite → Queue → Enqueue**

- **fPort:** `1`
- **Confirmed:** ei (pois päältä)
- **Muoto:** HEX-välilehti

### Kaikki komennot

| Toiminto | HEX | Selite |
|---|---|---|
| Muuta uni-aika 60 s | `01003C` | [0x01, 0x00, 0x3C] |
| Muuta uni-aika 5 min | `010132` | [0x01, 0x01, 0x32] = 306 s |
| Muuta uni-aika 10 min | `010258` | [0x01, 0x02, 0x58] = 600 s |
| Käynnistä OTA (5 min) | `0205` | [0x02, 0x05] |
| Käynnistä OTA (10 min) | `020A` | [0x02, 0x0A] |
| Tyhjennä pin-välimuisti | `03` | pakottaa uuden pin-proobauksen |

### Uni-ajan laskeminen

Uni-aika lähetetään 16-bittisena big-endian lukuna (tavut 1 ja 2):

```
sekuntia → hex:
 60 s = 0x003C → HEX: 01 00 3C
300 s = 0x012C → HEX: 01 01 2C
600 s = 0x0258 → HEX: 01 02 58
```

---

## 5. OTA-päivitys vaihe vaiheelta

### Vaihe 1 – Exporttaa binary valmiiksi

Arduino IDE: **Sketch → Export Compiled Binary**

Tiedosto tallentuu sketch-kansioon:
```
build/esp32.esp32.esp32da/NimiSketchi.ino.bin
```

Löydät sen myös:
```bash
find ~/Arduino -name "*.ino.bin" 2>/dev/null
```

> Käytä aina `.ino.bin` – ei `.merged.bin` (se sisältää bootloaderin
> eikä sovi OTA-päivitykseen).

### Vaihe 2 – Laita jonoon uni-ajan lyhennys (valinnainen)

Jos laite nukkuu 10 minuuttia, voit lyhentää odotusajan:

**Queue → Enqueue → HEX: `01003C`** (60 sekuntia)

### Vaihe 3 – Lähetä OTA-komento

**Queue → Enqueue → HEX: `0205`**

Laite käsittelee komennot peräkkäin samassa uplinkissa –
molemmat voi laittaa jonoon kerralla.

> **Tärkeää:** Jos jonossa on jo väärä komento, paina ensin **Flush queue**.

### Vaihe 4 – Odota heräämistä

Laite vastaanottaa downlinkin seuraavan uplinkin RX1-ikkunassa (1 s uplinkin jälkeen).
Serial monitoriin ilmestyy vahvistus:

```
[dl] 2 tavua: 02 05
[dl] OTA-tila asetettu: 5 min
```

### Vaihe 5 – Seuraava herääminen käynnistää OTA-tilan

```
[ota] OTA-tila aktiivinen – haetaan päivitystä
[OTA] Isäntänimi: lorawan-c5d4a8 (FW: 1.1.0)
[OTA] Yhdistetty: 192.168.64.207
[OTA] Palvelin käynnissä – avaa Arduino IDE → Portit
[OTA] Odotetaan enintään 300 sekuntia...
```

### Vaihe 6 – Lataa firmware

**Vaihtoehto A – Arduino IDE (jos mDNS toimii):**

Tools → Port → `lorawan-c5d4a8 (ESP32)` → Upload

**Vaihtoehto B – espota.py suoraan IP:llä (suositeltu Linuxilla):**

Katso luku 6.

### Vaihe 7 – Onnistuneen päivityksen merkki

```
[OTA] 10%
[OTA] 20%
...
[OTA] 100%
[OTA] Päivitys valmis – käynnistetään uudelleen
```

Laite käynnistyy uudelleen automaattisesti. Ensimmäinen join OTA:n jälkeen
epäonnistuu koodilla `-1116` – tämä on normaalia koska OTA-reset tyhjentää
RTC-muistin. Laite joinaa uudelleen seuraavalla heräämisellä.

### Vaihe 8 – Palauta uni-aika

Jos lyhensit uni-ajan testiä varten:

**Queue → Enqueue → HEX: `010258`** (600 s = 10 min)

---

## 6. espota.py – päivitys suoraan IP-osoitteella

Linux-ympäristössä Arduino IDE ei välttämättä löydä ESP32:ta mDNS:n kautta.
`espota.py` ohittaa mDNS:n ja yhdistää suoraan IP-osoitteeseen.

### Etsi espota.py

```bash
find ~/.arduino15 -name "espota.py" 2>/dev/null
```

Käytä uusinta versiota, esim.:
```
/home/käyttäjä/.arduino15/packages/esp32/hardware/esp32/3.3.7/tools/espota.py
```

### Suorita päivitys

```bash
python3 ~/.arduino15/packages/esp32/hardware/esp32/3.3.7/tools/espota.py \
  -i 192.168.64.207 \
  -p 3232 \
  -a 192.168.64.21 \
  -f ~/Arduino/Projekti/build/esp32.esp32.esp32da/Sketchi.ino.bin
```

| Parametri | Selite |
|---|---|
| `-i` | ESP32:n IP-osoite (näkyy serialissa OTA-tilassa) |
| `-p` | OTA-portti, aina `3232` |
| `-a` | Kehityskoneen IP-osoite (ESP32 yhdistää takaisin tähän) |
| `-f` | Polku `.ino.bin`-tiedostoon |

### Etsi koneen IP

```bash
ip addr | grep inet | grep -v 127
```

### Etsi .bin-tiedosto

```bash
find ~/Arduino -name "*.ino.bin" 2>/dev/null
```

---

## 7. Tyypilliset virheet ja ratkaisut

### OTA-tila ei käynnisty

| Oire | Syy | Ratkaisu |
|---|---|---|
| Ei `[dl] OTA-tila asetettu` | Väärä payload jonossa | Tarkista HEX: `0205`, ei `AgU=` tai muuta |
| Komento häviää jonosta | Laite oli jo herännyt ennen jonoon laittoa | Laita komento uudelleen |
| Jonossa oli väärä komento ensin | Vanha viesti | **Flush queue** → laita oikea komento |

### espota.py epäonnistuu

| Virhe | Syy | Ratkaisu |
|---|---|---|
| `No response from device` | Palomuuri estää paluuyhteyden | `sudo ufw disable` tai `sudo ufw allow from 192.168.x.0/24` |
| `No response from device` | `-a` puuttuu tai väärä IP | Lisää `-a <koneen IP>` |
| `Connection refused` | ESP32 ei ole OTA-tilassa | Laita OTA-komento jonoon ja odota heräämistä |
| `Yhteysvirhe` serialissa | Palomuuri tai väärä IP | Sama kuin yllä |

### Palomuuri (Ubuntu/Debian)

```bash
# Tarkista tila
sudo ufw status

# Avaa väliaikaisesti koko aliverkko
sudo ufw allow from 192.168.64.0/24

# Tai sammuta testin ajaksi
sudo ufw disable

# Palauta käyttöön testin jälkeen
sudo ufw enable
```

### mDNS ei toimi (laite ei näy Arduino IDE:ssä)

Arduino IDE 2.x Linuxilla käyttää omaa mDNS-toteutusta joka on tunnetusti epäluotettava.

```bash
# Tarkista avahi
sudo systemctl status avahi-daemon

# Asenna jos puuttuu
sudo apt install avahi-daemon
sudo systemctl enable --now avahi-daemon

# Tarkista näkyykö ESP32
avahi-browse _arduino._tcp
```

Jos avahi on käynnissä mutta IDE ei silti näytä laitetta, käytä `espota.py`:tä
suoraan IP-osoitteella (luku 6) – se on luotettavampi vaihtoehto Linuxilla.

### Join epäonnistuu OTA:n jälkeen (-1116)

OTA-reset (`SW_CPU_RESET`) tyhjentää RTC-muistin, joten `isJoined = false`.
Laite yrittää uutta OTAA:ta – join onnistuu seuraavalla heräämisellä automaattisesti.
Ei vaadi toimenpiteitä.

### Watchdog-reset (TG1WDT_SYS_RESET) käynnistyksen aikana

Pin-proobaus kestää liian kauan ja WDT laukeaa.

Ratkaisu: lisää `BOARD_PINS_KNOWN` -define `settings.h`:iin ja kovakoodaa pinnit
(katso `settings.h` kommentit). Proobaus tehdään vain kerran ja tallennetaan
NVS:ään – seuraavilla käynnistyksillä se ohitetaan.

### Sketchi ei mahdu OTA-partitioon

```
Sketch uses 1098870 bytes (83%) of program storage space.
```

Tämä on normaali koko. OTA vaatii oikean partitiotaulun:

**Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)**

---

## 8. Downlink-komentojen HEX-referenssi

### Komento 0x01 – uni-ajan muutos

```
[0x01, korkea_tavu, matala_tavu]
sekunnit = korkea_tavu × 256 + matala_tavu
```

| Uni-aika | Sekuntia | HEX |
|---|---|---|
| 1 minuutti | 60 | `01003C` |
| 5 minuuttia | 300 | `01012C` |
| 10 minuuttia | 600 | `010258` |
| 15 minuuttia | 900 | `010384` |
| 30 minuuttia | 1800 | `010708` |
| 1 tunti | 3600 | `010E10` |

### Komento 0x02 – OTA-tila

```
[0x02]         → 5 minuutin timeout (oletus)
[0x02, 0x05]   → 5 minuuttia
[0x02, 0x0A]   → 10 minuuttia
[0x02, 0x1E]   → 30 minuuttia
```

ESP32 yhdistää WiFiin ja odottaa `espota.py`- tai Arduino IDE -päivitystä.
Timeout-ajan jälkeen laite jatkaa normaalia toimintaa.

### Komento 0x03 – pin-välimuistin tyhjennys

```
[0x03]
HEX: 03
```

Pakottaa I2C- ja SPI-pinnien uudelleentunnistuksen seuraavalla heräämisellä.
Hyödyllinen jos komponentteja on vaihdettu tai pinnit muuttuneet.

---

## Payload-formaatti (4 tavua)

```
Tavu 0: lämpötila + 40   (0–125  → -40...+85 °C)
Tavu 1: kosteus           (0–100 %)
Tavu 2: paine – 900       (0–160 → 900–1060 hPa)
Tavu 3: akku %            (0–100 %)
```

Esimerkki: `3D 1C 7F 00`
- `0x3D` = 61 → 61 - 40 = **21 °C**
- `0x1C` = 28 → **28 %**
- `0x7F` = 127 → 127 + 900 = **1027 hPa**
- `0x00` = 0 → **0 % akku** (ei kytketty)

---

*Firmware: v1.1.0 | Chirpstack: v4 | ArduinoOTA | ESP32 Arduino Core 3.3.7*
