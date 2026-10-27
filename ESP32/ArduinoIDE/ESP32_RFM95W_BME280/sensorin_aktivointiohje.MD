# Sensorin Aktivointi Ohjeet

Tämä dokumentti tarjoaa kattavat ohjeet LoRaWAN-sensorin (esimerkiksi BME280-pohjaisen sensorin) aktivointiin ChirpStack-ympäristössä. Ohjeet keskittyvät Arduino-koodin muokkaamiseen tiedostossa `LoraWanBME280_OTAA_ULP.ino`, tarvittavien avainten ja tunnisteiden generointiin ChirpStackissa sekä normaalin toiminnan tarkkailuun. Dokumentti on suunniteltu auttamaan käyttäjiä välttämään yleisiä virheitä, tarjoamaan kontekstia LoRaWAN-protokollalle ja käsittelemään mahdollisia edge caseja, kuten verkkoongelmia tai avainten hallintaa.

## Taustaa ja Konteksti
LoRaWAN (Long Range Wide Area Network) on langaton protokolla, joka mahdollistaa matalatehoisten laitteiden (kuten sensorien) yhteydenpidon pitkien etäisyyksien yli. OTAA (Over-The-Air Activation) on turvallinen tapa aktivoida laitteita, jossa laite liittyy verkkoon dynaamisesti ilman etukäteen määriteltyjä avaimia verkossa. Tässä prosessissa käytetään seuraavia keskeisiä elementtejä:
- **DevEUI (Device EUI)**: Yksilöllinen 64-bittinen tunniste laitteelle, joka generoidaan ChirpStackissa. Se varmistaa, että jokainen sensori on uniikki verkossa.
- **JoinEUI (entinen AppEUI)**: 64-bittinen tunniste, joka yhdistää laitteen tiettyyn sovellukseen verkossa. Se on usein sama kaikille saman sovelluksen laitteille.
- **AppKey (Application Key)**: 128-bittinen salainen avain (uint8_t[16]), joka käytetään OTAA-prosessissa salaukseen ja autentikointiin. Se generoidaan sovelluskohtaisesti.

Muuttamalla näitä arvoja koodissa varmistat, että sensori voi liittyä ChirpStack-verkkoon turvallisesti. Jos arvoja ei muuteta oikein, sensori ei pysty liittymään verkkoon, mikä voi johtaa virheisiin kuten "Join failed" -viesteihin. Edge caseina huomioi, että jos verkko on ruuhkainen tai gateway ei ole saatavilla, aktivointi voi epäonnistua väliaikaisesti – tällöin kannattaa tarkistaa gatewayn tila ChirpStackin käyttöliittymästä.

## Vaihe 1: Arduino-Koodin Muokkaus
Avaa Arduino IDE:ssä tiedosto `LoraWanBME280_OTAA_ULP.ino`. Etsi seuraavat muuttujat ja korvaa ne ChirpStackista saatavilla arvoilla. Muuttujat määritellään yleensä koodin alussa, esimerkiksi `#define`-direktiivien tai muuttujien alustuksen yhteydessä.

### Muutettavat Kohdat:
- **uint64_t joinEUI**: Aseta tähän JoinEUI-arvo, joka löytyy ChirpStackista (katso Vaihe 2). Esimerkkiarvo: `0x0000000000000001` (käytä heksadesimaalimuotoa).
- **uint64_t devEUI**: Aseta tähän yksilöllinen DevEUI, joka generoidaan jokaiselle sensorille erikseen. Esimerkkiarvo: `0xAABBCCDDEEFF0011`.
- **uint8_t appKey[16]**: Tämä on taulukko 16 tavusta (128 bittiä). Kopioi generoitu AppKey tähän muotoon, esimerkiksi: `{0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C}`.

#### Esimerkki Koodinpätkä Muutoksesta:
Alkuperäinen (oletusarvoilla):
```cpp
uint64_t joinEUI = 0x0000000000000000;
uint64_t devEUI = 0x0000000000000000;
uint8_t appKey[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
```

Muutettu (esimerkkiarvoilla):
```cpp
uint64_t joinEUI = 0x0000000000000001;  // JoinEUI ChirpStackista
uint64_t devEUI = 0xAABBCCDDEEFF0011;    // Yksilöllinen DevEUI
uint8_t appKey[16] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C};  // Generoitu AppKey
```

**Huomioita ja Edge Caseja:**
- Varmista, että arvot ovat heksadesimaalimuodossa ja oikein muotoiltu – virheellinen muoto voi aiheuttaa käännösvirheitä Arduino IDE:ssä.
- Jos käytät useita sensoreita, kopioi jokaiselle oma DevEUI ja käytä samaa JoinEUI:ta ja AppKey:tä saman sovelluksen sisällä, ellei sovellus vaadi erillisiä avaimia.
- Testaa koodi lataamalla se sensorille (esim. ESP32- tai Arduino-pohjaiselle levylle) ja seuraamalla sarjaporttia debug-viestejä varten. Jos liittyminen epäonnistuu, tarkista lokit: yleisiä syitä ovat väärät avaimet tai heikko signaali.
- Muista tarkistaa myös pinnit, sillä esimerkiksi SuperMini kehityslaudan pinnit ovat hieman eri kuin normaalin DebBoardin!

## Vaihe 2: Avainten ja Tunnisteiden Generointi ChirpStackissa
Kirjaudu ChirpStackin käyttöliittymään (yleensä osoitteessa `https://your-chirpstack-server:8080`). Seuraa alla olevaa polkua generoidaksesi tarvittavat arvot. Tämä prosessi varmistaa, että sensori rekisteröidään oikein verkkoon.

### Polku DevEUI:n Generointiin:
1. Siirry **Tenants**-välilehdelle (vuokralaiset, jos käyttöliittymä on suomeksi).
2. Valitse **ChirpStack** (tai vastaava tenant-nimi).
3. Siirry **Applications**-välilehdelle (sovellukset).
4. Valitse **BME280 Sensorit** -sovellus (tai luo uusi, jos sellaista ei ole).
5. Siirry **Devices**-välilehdelle (laitteet).
   - Klikkaa "Add Device" luodaksesi uuden laitteen.
   - Generoi yksilöllinen **DevEUI** automaattisesti tai syötä oma (suositellaan automaattista generointia uniikkiuden varmistamiseksi).
   - **JoinEUI** löytyy sovelluksen asetuksista tai Devices-sivulta – se on usein sama kaikille saman sovelluksen laitteille.

### Application Keyn Generointi:
- Palaa **Applications**-välilehdelle.
- Valitse **BME280 Sensorit**.
- Etsi "Application Key" -osio (tai vastaava, kuten "Root Keys" OTAA:lle).
- Generoi uusi 128-bittinen avain (yleensä heksadesimaalimuodossa) ja kopioi se. Jos avainta ei generoida automaattisesti, käytä työkalua kuten OpenSSL:ää komennolla `openssl rand -hex 16`.

**Huomioita ja Edge Caseja:**
- Jos ChirpStack on konfiguroitu monivuokralaisena (multi-tenant), varmista oikea tenant-valinta – väärä tenant voi johtaa siihen, että avaimet eivät matchaa.
- Turvallisuus: Älä jaa AppKey:tä julkisesti, sillä se mahdollistaa laitteen spoofingin. Jos avain vuotaa, generoi uusi ja päivitä koodi.
- Jos käytät ChirpStack v4:ää tai uudempaa, prosessi voi hieman vaihdella – tarkista dokumentaatio osoitteesta https://www.chirpstack.io/docs/.

## Vaihe 3: Normaalin Toiminnan Tarkkailu
Kun sensori on aktivoitu ja koodi ladattu, seuraa toimintaa ChirpStackin käyttöliittymästä laitteen sivulla. Siirry **Devices**-välilehdelle, valitse sensori ja avaa **LoRaWAN Frames** -välilehti (kehykset). Tässä näet reaaliaikaiset paketit, jotka auttavat debuggauksessa.

### Odotettu Sekvenssi:
1. **Join Request**: Sensori lähettää liittymispyynnön. Näkyy DevEUI ja JoinEUI. Esimerkki: "JoinRequest: DevEUI=0xAABBCCDDEEFF0011, JoinEUI=0x0000000000000001".
2. **Join Accept**: Verkko hyväksyy liittymisen. Näkyy DevEUI, gatewayn ID (esim. gateway, joka vastaanottaa signaalin). Tämä vahvistaa onnistuneen autentikoinnin.
3. **Confirmed Data Up**: Sensori lähettää dataa (esim. BME280:n lämpötila, kosteus, paine). Tämä on vahvistettu (confirmed), joten verkko lähettää kuittauksen.
4. **Unconfirmed Data Down**: Tässä vaiheessa tämä on varattu tulevaisuudelle. Sitä voidaan käyttää etäkonfiguraatioon, kuten arvojen asettamiseen tai sensorin resetointiin. Jos et käytä downlinkiä, tämä voi olla tyhjä aluksi.

#### Esimerkki Frame-Lokista:
- JoinRequest: [Timestamp] DevEUI: 0xAABBCCDDEEFF0011, JoinEUI: 0x0000000000000001
- JoinAccept: [Timestamp] DevAddr: 0x26012345, Gateway ID: eui-1234567890abcdef
- Confirmed Data Up: [Timestamp] Payload: [Sensor data in hex], FPort: 1
- Unconfirmed Data Down: [Timestamp] (Tyhjä tai tuleva komento)

**Huomioita ja Edge Caseja:**
- Jos näet vain JoinRequesteja ilman Acceptia, tarkista: (1) Ovatko avaimet oikein? (2) Onko gateway online? (3) Onko signaali riittävä (RSSI/SNR-arvot frameissa)?
- Downlink-ominaisuudet: Tulevaisuudessa voit konfiguroida ChirpStackin lähettämään komentoja, kuten "reset" tai asetusten muutoksia. Tämä vaatii koodin muokkausta sensorin puolella vastaanottamaan downlink-paketteja.
- Nuanssit: LoRaWAN käyttää ABP:tä (Activation by Personalization) vaihtoehtona OTAA:lle, mutta OTAA on turvallisempi dynaamisille verkoille. Jos siirryt ABP:hen, tarvitset DevAddr ja session-avaimet.
- Implicaatiot: Tämä setup mahdollistaa skaalautuvan sensoriverkon, mutta huomioi energiankulutus – ULP (Ultra Low Power) -tila auttaa pidentämään akun kestoa.

## Lisäresurssit ja Vianetsintä
- **Dokumentaatio**: ChirpStack docs (https://www.chirpstack.io/), LoRa Alliance spec (https://lora-alliance.org/resource_hub/lorawan-specification-v1-0-4/).
- **Esimerkit**: Jos kohtaat virheitä, kokeile esimerkkikoodia GitHubista (esim. The Things Networkin repoista mukautettuna ChirpStackille).
- **Yleiset Ongelmat**: Väärä endianness avaimissa (big vs. little endian) – varmista koodin matching ChirpStackin kanssa.
- **Turvallisuusimplikaatiot**: Käytä HTTPS:ää ChirpStackiin ja pidä avaimet salassa estääksesi hyökkäyksiä.
- **Toisinaan on helpointa deletoida laite Chirpstackista** ja tehdä se uusiksi. Muista ottaa generoimasi DevID ym. tiedot ylös per laite!

Tämä ohjeistus kattaa prosessin kattavasti, mutta jos tarvitset mukautuksia (esim. useille sensoreille), ota yhteyttä lisäohjeisiin.
