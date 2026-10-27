Projektina on siirtää vanhassa Raspberry PI3B:ssä pyörivät mqtt-brokeri, influxdb ja Grafana uuteen Raspberry PI5 ja lisätä LoraWan-sensoreita. Tässä ohjeessa on asennusohjeet ChirpsStack asennukseen, joka on oikeastaan ainoa järkevä Raspberry Pi5:lle pelkäksi gatewayksi. Tässä ohjeessa tehdään SD-kortille puhdas ChirpsStack asennus, mutta OpenWrt ei voi asentaa InfluxDB:tä eikä Grafanaa, jolloin asennus pitää tehdä siten, että käyttää Raspberryn virallista käyttistä ja ajaa ChirpStackia dockkerilla. Päivitän tätä ohjetta kun pääsen testaamaan lisää.

## Mikä ihmeen LoRaWan?

**LoRaWAN** on langaton, matalatehoinen laajaverkkoteknologia (LPWAN), joka on suunniteltu erityisesti **IoT-laitteille** (esim. sensorit, mittarit, seurantalaitteet). Se mahdollistaa pienten datamäärien lähettämisen erittäin pitkällä etäisyydellä (yleensä 2–15 km kaupunki- tai maaseutualueella) samalla kun laitteet kuluttavat vain vähän virtaa ja voivat toimia vuosia paristoilla.

LoRaWAN perustuu **LoRa**\-modulaatioon, mutta ne eivät ole sama asia.

### LoRa vs. LoRaWAN – lyhyt ero

**LoRa** (Long Range)

-   Fyysinen kerros (radioaaltojen modulaatio).
-   Se on vain tapa lähettää signaalia pitkälle matalalla teholla (Chirp Spread Spectrum -tekniikka).
-   Ei määrittele, miten laitteet kommunikoivat keskenään, verkkoa tai turvallisuutta.
-   Voi käyttää esimerkiksi suoraan pisteestä pisteeseen (P2P), mesh-verkossa tai täysin omassa protokollassa.

**LoRaWAN**

-   Verkkoprotokolla (MAC-kerros), joka toimii LoRa-modulaation päällä.
-   Määrittelee säännöt: miten laitteet liittyvät verkkoon (OTAA/ABP), milloin lähettävät, miten pakettien muoto on, turvallisuus (salaus), luokkatyypit (Class A, B, C) jne.
-   Tukee tähtitopologiaa: sensorit → gatewayt → verkko-serveri → sovellus (esim. ChirpStack, The Things Network).
-   Takaa laitteiden yhteensopivuuden eri valmistajilta.

Yksinkertaisesti: **LoRa** on radio, **LoRaWAN** on säännöt, miten radiota käytetään verkossa.

### Taajuusalueet ja luvitus Suomessa

Suomessa (kuten koko EU:ssa) LoRaWAN käyttää pääasiassa **EU868**\-taajuusaluetta, eli **863–870 MHz** (yleisimmin kutsutaan 868 MHz -kaistaksi). Tämä on **vapaa taajuusalue** (ISM-band), joten siihen ei tarvita erillistä radiolupaa, kun noudatetaan sääntöjä.

Tärkeimmät rajoitukset Suomessa / EU:ssa:

-   Lähetysteho: enintään **14 dBm** (25 mW) useimmilla kanavilla, joillain jopa **27 dBm** (500 mW) mutta erittäin rajoitetulla duty cyclellä.
-   **Duty cycle** \-rajoitus: laite saa lähettää vain tietyn prosenttiosuuden ajasta (yleensä 0,1–1 % riippuen alikaistasta) → estää verkon tukkeutumisen.
-   Pakolliset kanavat: laitteiden on tuettava vähintään taajuuksia **868.1, 868.3 ja 868.5 MHz**.
-   Muita yleisiä uplink-kanavia: 867.1–867.9 MHz jne.

Suomessa Traficom valvoo taajuuksien käyttöä, mutta normaalissa LoRaWAN-käytössä (noudattaen ETSI EN300.220 -standardia ja LoRaWAN Regional Parameters -määrittelyä) **ei tarvita yksittäistä lupaa** – se on luvaton (license-free) SRD-kaista.

### Yhteenveto

1.  LoRa = pitkän matkan radioaalto.
2.  LoRaWAN = standardoitu verkko LoRa:n päälle (turvallisuus, skaalautuvuus, yhteensopivuus).
3.  Suomessa käytetään 863–870 MHz -aluetta, luvaton kun pysytään säännöissä – sopii erinomaisesti omiin IoT-projekteihin kuten sensoreihin!

___

## LoraWan käyttöönotto Raspberry PI 5:ssä

Tein aiemmin kotelon Raspberry PI5 + SeedStudio WM1302 moduulille + hatulle + GPS/GNSS-antennille ja 868 MHz vertikaalipolariaation antennille. [Artikkelissani ohjeet](https://www.linkedin.com/pulse/ohje-kotelo-raspberry-pi-5lle-fusion-360lla-ja-jari-hiltunen-9vesf/).

LoraWan-moduulina on SeedStudion WM1302 istutettuna LoraHat:n jossa kommunikaatio tapahtuu SPI:llä. Valmistajan [Wiki-ohjeessa](https://wiki.seeedstudio.com/WM1302_module/) on ohjeet mitä Raspberryssä pitäisi tehdä, jotta moduulin saa toimimaan. Raspberry Pi5 ja Raspberry Lite-imagen kohdalla ongelmaksi tulee se, että **Raspberry Pi 5**:ssä (ja joissain uudemmissa kernel-versioissa) GPIO-ohjaus on siirretty RP1 I/O -sirulle, joten kernel käyttää **uutta offset-järjestelmää** (gpiochip0 alkaa numerosta 569 ja menee eteenpäin), GPIO 17 (BCM) = kernelissä gpio-588 GPIO 18 (BCM) = kernelissä gpio-589 GPIO 5 (BCM) = kernelissä gpio-576 (= GPIO5) ja lisäksi Raspberry Pi 5:ssä (ja uusissa Raspberry Pi OS -versioissa kernel 6.6+ Bookwormissa) vanha /sys/class/gpio-rajapinta (sysfs) on deprecated ja **_osittain rikki tai estetty normaalikäytössä_**.

Koska sysfs ei ole enää tuettuna muutenkaan, on viisainta käyttää ChirpStack Gateway OS:aa. Se on OpenWrt-pohjainen valmis image, joka sisältää kaiken tarvittavan (concentratord SX1302:lle, Basics Station -protokolla, MQTT/UDP-forwarding jne.), ja se tukee Raspberry Pi 5:ttä sekä Seeed WM1302 HAT:ia. Sen asennus on varsin helppoa.

## ChirpStack asentaminen dedikoituun raspiin

### 1\. Lataa oikea image Raspberry Pi 5:lle

Huom! Jos käytät tätä metodia, voit asentaa vain OpenWrt tukemia sovelluksia. Et siis voi asentaa esimerkiksi InfluxDB tietokantaa tällä metodilla.

Mene viralliselle sivulle: [https://www.chirpstack.io/docs/chirpstack-gateway-os/install/raspberry-pi.html](https://www.chirpstack.io/docs/chirpstack-gateway-os/install/raspberry-pi.html)

-   Valitse **Raspberry Pi 5** \-osio.
-   Suositus: **Full** \-image (sisältää sisäänrakennetun ChirpStack Network Serverin, MQTT forwarderin, Node-RED:in jne. – sopii hyvin jos haluat testata kaiken Pi:llä itsellään). Lataa **SD card factory image** (loppu .img.gz) Esim. tiedostonimi tyylillä: chirpstack-gateway-os-4.10.1-full-bcm27xx-bcm2712-rpi-5-squashfs-factory.img.gz (versio voi olla uudempi, ota uusin).
-   Tiedoston koko on alle 50 megaa.

### 2\. Flashaa image SD-kortille

-   Käytä ilmaista työkalua kuten **Raspberry Pi Imager** (suosituin ja helppo) tai **balenaEtcher**. Raspberry Pi Imager: Valitse "Choose OS" → "Use custom" → valitse ladattu .img.gz-tiedosto. Etcher: Valitse image, SD-kortti ja Flash.
-   Odota että flashaus valmistuu.

### 3\. Kokoa hardware ennen boottausta

-   WM1302-moduuli ruuvattuna kiinni Pi HAT:iin.
-   HAT kiinni Pi 5:n GPIO-headeriin (varmista että SPI-pinnit osuvat oikein).
-   Liitä Ethernet-kaapeli (WiFi toimii myöhemmin konffissa) tai käytä suoraan headless-moodia.
-   Laita SD-kortti Pi:hin ja kytke virta.

### 4\. Ensimmäinen boot ja konfigurointi

-   Pi boottaa suoraan ChirpStack Gateway OS:ään (OpenWrt-pohjainen).
-   Oletus-IP: **192.168.10.1** (jos Ethernetissä) tai etsi Pi:n IP verkostasi (esim. reitittimen DHCP-lista tai nmap).
-   Mene selaimella osoitteeseen: [**http://192.168.10.1**](http://192.168.10.1/) (tai löydetty IP). Kirjaudu: **root** / ei salasanaa aluksi (vaihda heti!).
-   Mene **ChirpStack** → **Concentratord**. Valitse **Global configuration - Enabled ja Enabled Chipset** SX1302 / SX1303 .
-   SX1302/SX1303 välilehdeltä **Antenna Gain: 2** on aika sopiva vertikaaliantennille, **Shield Model** SemTech - Lora(R) CoreCell 868 MHz.
-   **GNSS** mikäli haluat käyttää GPS/GNSS moduulia (kommunikaatio on I2C perusteinen)
-   Tallenna & Apply – gateway pitäisi käynnistyä automaattisesti.

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQFwykuNiiBzDg/article-inline_image-shrink_1500_2232/B4DZwtjKlPJcAU-/0/1770290719672?e=1772668800&v=beta&t=Gz4aEgXX2v_7ocSS8rfhotRkwwjhDwxoHl8KAMbsEXg)

Global configuration

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQGC-VZ21ddRzw/article-inline_image-shrink_1500_2232/B4DZwtj30eIYAU-/0/1770290904977?e=1772668800&v=beta&t=uGTnIkUKv1CDaYvYJOdhRHBnVkbOoY16rKIxo8G5lvU)

WM1302 taajuusalue. GNSS on GPS/GNSS moduuli (I2C)

### 5\. Yhdistä ulkoiseen LoRaWAN-verkkoon (MQTT-puoli)

-   Jos käytät **omaa ChirpStackia** (esim. toisella koneella tai cloudissa): Konffaa **Basics Station** \-forwarding (Gateway OS tukee sitä natiivisti). Anna server URL, auth-moodi (TLS tai ei), gateway EUI jne.
-   Vaihtoehtoisesti: **MQTT integration** suoraan (jos Full-imagessa).
-   Voit myös forwardata UDP:lla The Things Networkiin/Heliumiin jos haluat.

### 6\. Testaus

-   Logeista (Luci-webissä tai SSH:lla logread) näet jos concentratord käynnistyy ja näkee SX1302:n (esim. "chip version 0x10", SPI OK).
-   Jos gateway näkyy verkossa ja saa paketteja → onnistui!
-   WiFi-sensorisi: Voit puskea niiden dataa samaan MQTT-brokeriin (ChirpStack puskee LoRaWAN-uplinkit MQTT:hen topicilla kuten eu868/application/xxx/device/yyy/event/up).
-   Wifin ja LoraWan statukset näet myös **Status - Channel Analysis ja Bandwidth** kohdista.

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQEZT-qiBpv_ow/article-inline_image-shrink_1000_1488/B4DZwtlANpJIAQ-/0/1770291201399?e=1772668800&v=beta&t=0ybDRIWLII_V50XEZ7p2idw35mzgcTr_UoGX3J5bi_g)

2.4 GHz WiFi

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQHjY0nE_xMM4g/article-inline_image-shrink_1500_2232/B4DZwtlmebJ0AU-/0/1770291358268?e=1772668800&v=beta&t=ojioZuLmu2-TY7CHQZ9uskAF4mhc1DCjSbC2ilUsVy8)

LoraWan bandwidth (tässä tyhjä)

### Mahdolliset sudenkuopat Pi 5 + WM1302

-   Jos concentratord ei käynnisty: Tarkista web-konffista että **SPI enabled** ja **shield model** oikein. Joskus reboot auttaa.
-   Jos GPS ei toimi: Tarkista että I2C on konffattu oikein (Gateway OS yleensä hoitaa).
-   Jos tarvitset custom konffia: SSH sisään (root) ja muokkaa /etc/config/chirpstack-concentratord jne.

Virrankulutus on varsin pientä. Raspberry 5PI mukana tulee 5V/5A virtalähde ja se riittää vallan mainiosti vaikka tähän tulee vielä mqtt, influxdb ja Grafana.

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQHEnHZd2z6cBg/article-inline_image-shrink_1000_1488/B4DZwt16C1HwAQ-/0/1770295632786?e=1772668800&v=beta&t=P0nkrK7xghpViccKgPGPw1dIaqdGAbiMqnMWxCWhWbU)