## Projektin tavoite

Tehdä toimiva LoRaWAN-sensori (BME280 + ESP32-C3 + RFM95W), joka liittyy OTAA:lla ChirpStack-verkkoon ja lähettää lämpötila-, kosteus- ja painemittauksia.

**Aloituspiste aamulla**

-   Laite lähetti Join Requestin → näkyi gatewaylla ja ChirpStackissa
-   JoinAccept lähetettiin (joskus jopa useita kertoja) → mutta laite sai silti virheen **\-1116** (JoinAccept not received)
-   Serial-lokissa näkyi vain RX2-ikkunan avautuminen ja sulkeutuminen → RX1-ikkunaa ei näkynyt ollenkaan

**Päivän aikana selvitetty ongelmat (ja kiroukset)**

1. **DUPONT-liittimet – kirosana numero 1**

Kaikki alkoi siitä, että MISO/MOSI/SCK/CS/DIO0/DIO1 -kytkennät pätkivät satunnaisesti.

-   Useita kertoja homma näytti toimivan → sitten yhtäkkiä ei enää.
-   Lopulta selvisi, että lähes kaikki ongelmat johtuivat **löysistä, hapettuneista tai huonosti istuvista DUPONT-liittimistä**.
-   Kirous näille liittimille – ne ovat halpoja ja käteviä, mutta luotettavuus on nollaluokkaa prototyyppien ulkopuolella.

→ Ratkaisu: juotetaan tai käytetään laadukkaampia liittimiä / PCB:tä jatkossa.

2. **SPI-pinnit olivat pielessä**

Aluksi käytettiin vakiopinnejä (MISO=2/7, MOSI=7/2 – sekaisin).

Lopullinen toimiva konfiguraatio ESP32-C3:lla:

-   SCK = GPIO6
-   MOSI = GPIO2
-   MISO = GPIO9
-   CS = GPIO5
-   DIO0 = GPIO4
-   RST = -1 (ei käytössä)
-   DIO1 = -1 (ei käytössä)

→ Pakotettiin SPI.begin(SCK\_PIN, MISO\_PIN, MOSI\_PIN, CS\_PIN) – ilman tätä ei toiminut.

3. **Taajuus ja RX1/RX2 -ongelma**

JoinAccept lähetettiin usein **RX1**\-ikkunaan (esim. 868.3 MHz, SF7), mutta laite kuunteli vain **RX2**\-ikkunaa (869.525 MHz, SF12).

-   Ratkaisu: pakotettiin aloitustaajuus **868.3 MHz**:iin (sama kuin Accept) + datarate DR5 (SF7) ennen joinia.
-   RX1-ikkuna osui vihdoin kohdalleen → Accept kuultiin.

4. **DevNonce "already used" -virhe**

Kun joinia yritettiin liian monta kertaa saman istunnon aikana, DevNonce toistui → ChirpStack hylkäsi pyynnön.

-   Ratkaisu: poistettiin laite ChirpStackista kokonaan ja lisättiin uudelleen puhtaana → DevNonce-historia tyhjeni → join meni läpi.

5. **Lopputulos tänään**

-   Join onnistui → "JOIN ONNISTUI!" Serialissa
-   Ensimmäinen uplink lähti: **"T:20.5C H:23.3% P:992hPa"** porttiin 1
-   Verkko vastasi downlinkillä (LinkADRReq + kanavamuutokset)
-   DevAddr aktivoitui (näkyy ChirpStackissa)
-   Mittaukset näkyvät nyt verkossa

## Mitä opin / mitä suosittelisin muille

-   **Älä koskaan luota DUPONT-liittimiin tuotantotasolla** – juota tai käytä laadukkaita liittimiä (esim. JST, Hirose, Samtec)
-   ESP32-C3:n SPI-pinnit eivät ole intuitiivisia – dokumentoi aina toimiva konfiguraatio
-   Taajuus on kriittinen RX1-ikkunassa – pakota se samaan kanavaan kuin Accept näkyy gatewaylla
-   ChirpStackissa kannattaa aina aloittaa puhtaalla laitteella (poista ja lisää uudelleen) jos join jumittaa
-   RadioLib 7.5.0.0 toimii hyvin, mutta debug-tulosteet ovat kullanarvoisia

Nyt kun perusasiat pelaavat, seuraavaksi voisi lisätä:

-   confirmed uplink (varmistus viestien perillemenosta)
-   parempi payload (binäärinen, Cayenne LPP)
-   deep sleep + RTC-herätys (virransäästö)
-   downlink-komennot (esim. konfiguroi mittausväliä)

Jos joku muu painii samanlaisessa ESP32-C3 + RFM95W + ChirpStack -projektissa, kysykää vaan – tästä on nyt kokemusta vaikka muille jakaa 😄

___

Laitan kuvat tähän putkeen kun tämä paska ei anna kirjoittaa ja helvetttti hävittää pitkät tekstit saatanaaa!

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQFqBd327ha60g/article-inline_image-shrink_1500_2232/B4DZxMvsRBIUAU-/0/1770814097155?e=1772668800&v=beta&t=cwL27-4RkJ5RbHaPH5_R-MtfVvGLZ8MLsdtKZEqj7Tc)

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQE6cU6brzzkLQ/article-inline_image-shrink_1500_2232/B4DZxMvt6uGUAY-/0/1770814103894?e=1772668800&v=beta&t=gw8wO7CYqsjUb6ColJw30_oGhqMFSIPXAyydjYtVPVU)

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQEzuOW69QjgJw/article-inline_image-shrink_1000_1488/B4DZxMvv63JwAU-/0/1770814111919?e=1772668800&v=beta&t=BZeHlhj18hohusGPgyBvfQezRFpD240EME7UFS8xk1o)

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQEfJZRNQYLntw/article-inline_image-shrink_1000_1488/B4DZxMvxrGKYAY-/0/1770814119299?e=1772668800&v=beta&t=vg79esoOgLkCfRYBR33ynB3peHZbAJuGSnaSuh4kEcg)

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQH0TLaxnviBwg/article-inline_image-shrink_1500_2232/B4DZxMv0WgJ8AY-/0/1770814130189?e=1772668800&v=beta&t=o1sm9G8VgWSqIDvbejvwjFA83VpbTYwbD3uOXhP-iEY)

___

Sitten koodia jos tämä paskaeditori antaa sitä tänne laittaa:

```
#include <RadioLib.h>
#include <Wire.h>
#include <Adafruit_BME280.h>

// ───────────────────────────────────────────────
// OTAA-avaimet – ChirpStack-arvoilla
uint64_t joinEUI = vaiha;  // JoinEUI
uint64_t devEUI  = vaiha;  // Device EUI

uint8_t appKey[16] = {
vaiha};

uint8_t nwkKey[16];

// ───────────────────────────────────────────────
// RFM95W pinnit – toimiva konfiguraatio
#define CS_PIN    5
#define DIO0_PIN  4
#define RST_PIN   -1
#define DIO1_PIN  -1

// SPI-pinnit – toimiva yhdistelmä
#define SCK_PIN   6
#define MOSI_PIN  2
#define MISO_PIN  9

// ───────────────────────────────────────────────
// Radio ja LoRaWAN-node
SX1276 radio = new Module(CS_PIN, DIO0_PIN, RST_PIN, DIO1_PIN);
LoRaWANNode node(&radio, &EU868);

// BME280
Adafruit_BME280 bme;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  delay(2000);

  Serial.println(F("\n=== ESP32-C3 + RFM95W + BME280 + LoRaWAN OTAA ===\n"));

  // Kopioidaan AppKey → NwkKey
  memcpy(nwkKey, appKey, 16);

  // Näytetään avaimet debugiksi
  Serial.println(F("OTAA-avaimet:"));
  Serial.print(F("  JoinEUI: 0x")); Serial.println(joinEUI, HEX);
  Serial.print(F("  DevEUI:  0x")); Serial.println(devEUI, HEX);
  Serial.print(F("  AppKey:  "));
  for (int i = 0; i < 16; i++) Serial.printf("%02x", appKey[i]);
  Serial.println();

  // SPI custom pinneillä
  Serial.println(F("SPI.begin(SCK=6, MISO=9, MOSI=2, SS=5)"));
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
  SPI.setFrequency(1000000);
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);

  // Alustetaan radio
  Serial.println(F("Alustetaan RFM95..."));
  int16_t state = radio.begin();

  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("Radio alustus epäonnistui, koodi: "));
    Serial.println(state);
    while (true) delay(1000);
  }

  Serial.println(F("Radio OK"));
  Serial.print(F("Chip version: 0x"));
  Serial.println(radio.getChipVersion(), HEX);

  // Pakotetaan taajuus täsmälleen samaan kanavaan kuin viimeisin JoinAccept tuli
  radio.setFrequency(868.3);
  Serial.println(F("Taajuus pakotettu 868.3 MHz (sama kuin viimeisin JoinAccept)"));

  // Datarate sama kuin Accept (SF7 / DR5)
  node.setDatarate(5);
  Serial.println(F("Datarate asetettu DR5 (SF7)"));

  // OTAA-avaimet nodeen
  Serial.println(F("Asetetaan OTAA-avaimet..."));
  state = node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);

  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("beginOTAA epäonnistui, koodi: "));
    Serial.println(state);
    while (true) delay(1000);
  }

  Serial.println(F("OTAA-avaimet asetettu"));

  // ADR päälle
  node.setADR(true);
  Serial.println(F("ADR päällä"));

  // BME280 alustus
  Serial.println(F("\nAlustetaan BME280..."));
  Wire.begin(0, 1);  // SDA=GPIO0, SCL=GPIO1

  if (!bme.begin(0x76)) {
    if (!bme.begin(0x77)) {
      Serial.println(F("BME280 EI LÖYTYNYT!"));
    } else {
      Serial.println(F("BME280 löytyi 0x77"));
    }
  } else {
    Serial.println(F("BME280 löytyi 0x76"));
  }

  // OTAA join -yritykset
  Serial.println(F("\nAloitetaan OTAA join..."));
  int attempts = 0;
  const int MAX_ATTEMPTS = 10;

  while (!node.isActivated() && attempts < MAX_ATTEMPTS) {
    attempts++;
    Serial.printf("Yritys %d / %d\n", attempts, MAX_ATTEMPTS);

    // Varmistetaan taajuus ennen jokaista yritystä
    radio.setFrequency(868.3);
    Serial.println(F("   → Taajuus ennen join: 868.3 MHz"));

    state = node.activateOTAA();

    if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
      Serial.println(F("JOIN ONNISTUI!"));
      break;
    }

    Serial.print(F("Join epäonnistui, koodi: "));
    Serial.println(state);

    delay(15000 + attempts * 5000);  // kasvava viive
  }

  if (!node.isActivated()) {
    Serial.println(F("Kaikki join-yritykset epäonnistuivat."));
  }
}

void loop() {
  static unsigned long lastSend = 0;
  const unsigned long TX_INTERVAL = 60000UL;  // 60 s

  if (millis() - lastSend < TX_INTERVAL) {
    delay(100);
    return;
  }

  lastSend = millis();

  if (!node.isActivated()) {
    Serial.println(F("Ei vielä joinattu → ohitetaan lähetys"));
    return;
  }

  // BME280 mittaukset
  float temp = bme.readTemperature();
  float hum  = bme.readHumidity();
  float pres = bme.readPressure() / 100.0F;

  if (isnan(temp) || isnan(hum) || isnan(pres)) {
    Serial.println(F("BME280 lukuarvo virheellinen!"));
    return;
  }

  char payload[72];
  snprintf(payload, sizeof(payload),
           "T:%.1fC H:%.1f%% P:%.0fhPa",
           temp, hum, pres);

  uint8_t downlink[64];
  size_t downlinkLen = 0;

  Serial.print(F("Lähetetään: "));
  Serial.println(payload);

  // Lähetys (oikea sendReceive-kutsu)
  int16_t state = node.sendReceive(
    (uint8_t*)payload,
    strlen(payload),
    1,                // portti 1
    downlink,
    &downlinkLen,
    false             // unconfirmed uplink
  );

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Uplink onnistui"));
    if (downlinkLen > 0) {
      Serial.printf("Downlink (%d tavua): ", downlinkLen);
      for (size_t i = 0; i < downlinkLen; i++) {
        Serial.printf("%02X ", downlink[i]);
      }
      Serial.println();
    } else {
      Serial.println(F("Ei downlinkia"));
    }
  } else {
    Serial.print(F("Uplink epäonnistui, koodi: "));
    Serial.println(state);
  }
}
```