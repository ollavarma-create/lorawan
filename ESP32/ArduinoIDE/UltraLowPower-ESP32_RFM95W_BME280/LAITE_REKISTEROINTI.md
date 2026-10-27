# LoRaWAN-laitteen rekisterointi – ohje

Tama ohje kattaa uuden ESP32+RFM95W+BME280 -sensorinoden rekisteroinnin
Chirpstack v4 -verkonhallintaan kayttaen automaattista devEUI-generointia.

---

## Yleiskatsaus

```
[ESP32-node] --> (LoRa RF) --> [RPi5 + WM1302 gateway] --> [Chirpstack v4]
```

Jokaisella laitteella on uniikki **devEUI**, joka generoidaan automaattisesti
ESP32:n tehdasohjelmoitusta MAC-osoitteesta. JoinEUI ja AppKey ovat samat
kaikilla laitteilla.

---

## Vaihe 1 – Arduino-koodi: esivalmistelut

Avaa `ESP32_RFM95W_BME280.ino` Arduino IDE:ssa.

### 1a. Valitse boardi

Tiedoston alussa, muuta yksi rivi aktiiviseksi:

```cpp
// Kommentoi toinen rivi
#define BOARD_SUPERMINI
// #define BOARD_DEVBOARD
```

tai DevBoardille:

```cpp
// #define BOARD_SUPERMINI
#define BOARD_DEVBOARD
```

### 1b. Kytke DEBUG paalle

```cpp
#define DEBUG true
```

### 1c. Tarkista joinEUI ja appKey

Nama loytyy Chirpstackista: **Applications --> [sovellus] --> [laite] --> OTAA keys**

```cpp
uint64_t joinEUI = 0xXXXXXXXXXXXXXXXXULL;  // Chirpstack: Join EUI

uint8_t appKey[16] = {
  0xXX, 0xXX, ...  // Chirpstack: Application key (16 tavua)
};
```

> **Huom:** devEUI:ta EI tarvitse muuttaa koodiin – se generoidaan automaattisesti.

### 1d. Kaanna ja flashaa

Arduino IDE: **Upload** (Ctrl+U)

---

## Vaihe 2 – Lue devEUI serial monitorista

Avaa Arduino IDE: **Tools --> Serial Monitor**, nopeus **115200 baud**.

Nollaa laite tai paina reset. Serial monitoriin ilmestyy:

```
=== ESP32 + RFM95W + BME280 + LoRaWAN OTAA (ULP) ===

devEUI (rekisteroi Chirpstackiin): 3C:DC:75:FF:FE:CE:0C:98
Board: DevBoard
Alustetaan RFM95...
Radio OK
Alustetaan BME280...
Aloitetaan OTAA activation...
Yritys 1 / 5
Activation epaonnistui, koodi: -1116
```

> **Activation epaonnistuu tassa vaiheessa – se on normaalia.**
> Laite ei ole viela rekisteroity Chirpstackiin.

Kopioi devEUI talteen, esim. `3C:DC:75:FF:FE:CE:0C:98`.

---

## Vaihe 3 – Rekisteroi laite Chirpstackiin

### 3a. Avaa sovellus

Chirpstack web-UI --> **Applications** --> valitse sovelluksesi (esim. *BME280 Sensorit*)

### 3b. Lisaa laite

Klikkaa **Add device**.

| Kentta | Arvo |
|---|---|
| **Name** | Esim. `ESP32-SuperMini-Ulko` |
| **DevEUI** | Kopioi serial monitorista, **ilman kaksoispisteet** esim. `3CDC75FFFECE0C98` |
| **Device profile** | Valitse olemassa oleva, esim. `LoraWan_BME280_ULP` |
| **Skip frame-counter checks** | Ei (jata oletukselle) |

Klikkaa **Submit**.

### 3c. Aseta OTAA-avaimet

Laitelomakkeella valitse valilehti **OTAA keys**.

| Kentta | Arvo |
|---|---|
| **Application key** | Sama kuin koodissa, 32 hex-merkkin jono |

Klikkaa **Submit**.

> **JoinEUI** taytetaan automaattisesti device profiilin mukaan tai voit
> tarkistaa etta se vastaa koodin `joinEUI`-arvoa.

---

## Vaihe 4 – Tarkista join Chirpstackissa

### LoRaWAN Frames

Chirpstack --> **Applications** --> laite --> **LoRaWAN Frames**

Kun laite yrittaa joinia, naet:

| Tyyppi | Selite |
|---|---|
| `JoinRequest` | Laite lahettaa join-pyynnon |
| `JoinAccept` | Chirpstack hyvaksyi joinin |

Jos `JoinRequest` nakyy mutta `JoinAccept` puuttuu, tarkista:
- AppKey on sama koodissa ja Chirpstackissa
- Device profile on CLASS_A
- Gateway on online (Chirpstack --> **Gateways**)

### Events

Chirpstack --> **Applications** --> laite --> **Events**

Onnistuneen joinin jalkeen naet:

```
join  – Device joined (new session)
up    – Uplink received, fPort=1, data decoded
```

Decoded payload nayttaa:
```json
{
  "temp": 20,
  "hum": 28,
  "pres": 1016
}
```

---

## Vaihe 5 – Tuotantokayyttoon

Kun join on onnistunut ja data nakyy Chirpstackissa:

### 5a. Kytke DEBUG pois

```cpp
#define DEBUG false
```

### 5b. Flashaa uudelleen

Arduino IDE: **Upload** (Ctrl+U)

Laite siirtyy nyt ultra low power -tilaan:
- Heraaa 10 minuutin valein
- Lukee BME280 forced modessa (~10 ms)
- Lahettaa 3-tavu payload LoRaWAN DR5 (SF7)
- Nukkuu takaisin deep sleepiin

---

## Vaihe 6 – Uusi laite (toisto)

Jokainen uusi laite: toista vaiheet 1–5.

- **Vaihda vain** `#define BOARD_SUPERMINI` / `#define BOARD_DEVBOARD`
- JoinEUI ja AppKey pysyvat **samoina**
- DevEUI generoidaan automaattisesti – lue aina serial monitorista

---

## Vianmaaritys

| Ongelma | Syy | Ratkaisu |
|---|---|---|
| `Activation epaonnistui, koodi: -1116` | Laite ei rekisteroity | Rekisteroi Chirpstackiin (vaihe 3) |
| `JoinRequest` nakyy, ei `JoinAccept` | Vaaraa AppKey | Tarkista avain Chirpstackista |
| `Radio alustus epaonnistui` | SPI-pinnit vaarassa | Tarkista `#define BOARD_` valinta |
| `BME280 EI LOYDY` | I2C-osoite tai pinnit | Tarkista johdotus, kokeile 0x76/0x77 |
| DR nakyy SF9 eika SF7 | Chirpstack muistaa vanhan DR:n | Chirpstack: laite --> **Flush OTAA session** |
| `RSSI=0 dBm` serial monitorissa | RadioLib-rajoitus confirmed uplinkissa | Ei haittaa – oikea RSSI nakyy Chirpstack Framesissa |

---

## Payload-formaatti (3 tavua)

```
Tavu 0: lampotila + 40   (0-125  ->  -40...+85 degC)
Tavu 1: kosteus           (0-100  ->  0...100 %)
Tavu 2: paine - 900       (0-160  ->  900...1060 hPa)
```

### Chirpstack decoder (JavaScript)

```javascript
function decodeUplink(input) {
  if (input.bytes.length < 3) {
    return { data: {}, errors: ['Payload liian lyhyt'] };
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

## Laitteisto

| Komponentti | Malli | Huomio |
|---|---|---|
| Mikrokontrolleri | ESP32-C3 SuperMini tai DevBoard-M | Valitaan `#define`-rivillä |
| LoRa-radio | RFM95W (SX1276) | EU868, SF7-SF12 |
| Ympäristosenori | BME280 | I2C, osoite 0x76 tai 0x77 |
| Gateway | Raspberry Pi 5 + RAK WM1302 | EU868 |
| Verkonhallinta | Chirpstack v4 (Docker) | CLASS_A, OTAA |
