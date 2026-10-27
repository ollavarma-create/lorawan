Tämä ohje keskittyy lankatulostimiin (FDM), jotka ovat yleisimpiä kotikäytössä. Ohje on tarkoituksella tehty yksinkertaisilla esimerkeillä jotta aloittaminen olisi helppoa.

Esimerkkejä vastaavista malleista voit löytää Thingiversestä tai [Printables.com](http://printables.com/):sta, mutta suunnittele oma, jotta se sopii täydellisesti.

## Tarvittavat ohjelmistot

Voit käyttää seuraavia ohjelmistoja imaiseksi kotikäytössä ja ilman kaupallista tarkoitusta:

-   **Fusion 360**: CAD-ohjelmisto [Autodeskilta](https://www.autodesk.com/products/fusion-360/personal-download) 3D-mallinnukseen. Se sopii erinomaisesti tarkkoihin mittoihin ja parametreihin perustuvaan suunnitteluun. Lataa se Autodesk:in sivustolta. Toimii vain Windowsilla ja Macilla. Linuxissa käytä [VMWaressa](https://www.vmware.com/) Windowsia, sillä muut virtualisointiin tarkoitetut sovellukset eivät tue oikein DirectX11:ta (VMWaresta on ilmaiseksi 25H2 versio saatavilla Broadcomilta)
-   **Cura tai muu slicer**: [Ultimaker Cura](https://ultimaker.com/software/ultimaker-cura/) on suosittu ilmainen slicer, joka muuntaa 3D-mallin (STL-tiedosto) tulostusohjeiksi. Vaihtoehtoja ovat PrusaSlicer tai Bambu Studio, jos käytät tiettyä tulostinta.

Lisäksi tarvitset:

-   3D-tulostin (esim. Ender 3 tai vastaava FDM-tulostin).
-   Materiaali: PLA on helppo aloittelijoille, mutta ABS tai PETG kestää paremmin lämpöä Raspberry Pi:n kaltaisissa laitteissa.

## Hyvin lyhyet Fusion 360:n perusteet kotelon suunnitteluun

Ennen varsinaista suunnittelua, hanki Raspberry Pi 5:n ja tarvittaessa muiden osien mitat. Viralliset mitat löytyvät Raspberry Pi Foundationin sivustolta: piirilevy on noin 85.6 mm x 56 mm, ja kiinnitysreiät ovat standardit (58 mm x 49 mm väli). Käytä näitä mittoja pohjana. Toinen vaihtoehto on ladata [viralliset](https://pip.raspberrypi.com/categories/1096-design-files) Raspberry Pi5 suunnittelutiedostot ja muuntaa STEP-muotoiset tiedostot STL-tiedostoiksi, jolloin voit tuoda koko Raspberryn sellaisenaan osaksi suunnitelmaa. Tuolloin esimerkiksi USB-porttien kohdentaminen on paljon helpompaa.

Voit löytää myös muiden komponenttien STEP-tiedostot valmiina. Esimerkiksi tähän projektiin minun piti piirtää vain GPS/GNSS-moduuli. Löydät Fusion360 piirrokseni [tästä linkistä](https://outlook37929.autodesk360.com/g/shares/SH90d2dQT28d5b602811a4ee6c9d4a9d8285).

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQHKLXYWPRA2_g/article-inline_image-shrink_1000_1488/B4DZwopnwFJEAQ-/0/1770208526639?e=1772668800&v=beta&t=eNwJYT_AIcHx0kAvfdYD6oIWrHkfuSmMmY2NQ6SaTHM)

LoRaWan projektin roippeita, jotka pitää saada yhteen koteloon

### Millainen rako?

Lankatulostimissa (FDM) hyvä perussääntö on käyttää rakoa, joka vastaa tulostuskerroksen korkeutta (layer height), tyypillisesti 0.2 mm. Tämä kompensoi materiaalin kutistumista ja epätarkkuuksia.

-   **Esimerkki**: Jos antennin tai ruuvin halkaisija on 6.3 mm, tee Fusion 360:ssa reiästä 6.5 mm (tai lisää 0.2 mm molemmille puolille jos ei reikä). Testaa aina prototyypillä, koska PLA kutistuu vähemmän kuin ABS.
-   **Vinkki**: Käytä parametreja Fusion 360:ssa (Modify > Change Parameters) – määritä "clearance" muuttujaksi (esim. 0.2 mm), niin voit muuttaa sitä helposti koko mallissa.
-   Huomio: Eri materiaalit käyttäytyvät eri tavalla. PETG tarvitsee usein 0.3-0.4 mm rakoa, koska se laajenee enemmän. Testaa Cura-simulaatiolla ennen tulostusta.

### Askel-askeleelta ohje yksinkertaisen kotelon suunnitteluun Fusion 360:ssa

Tässä esimerkki Raspberry Pi 5 -kotelon suunnittelusta, joka sisältää pohjan, seinät, kannen ja aukot porteille (USB, HDMI, Ethernet jne.).

1. **Aloita uusi projekti**:

-   Avaa Fusion 360 ja luo uusi Hybrid Design. Älä käytä Part Design (oletus), sillä teet kuitenkin useamman osan samaan kuvaan. Mikäli aloitat piirustuksen Parts- asetuksella, sinulta puuttuu Create New Component menusta. Parts:lla luodun osan voi toki tuoda osaksi kokonaisuutta, mutta yksin tehdyssä kehityksessä se on riesa, koska joudut seilaamaan edestakaisin eri tiedostojen välillä.
-   Vaihda yksiköiksi mm, gram ellei jo ole

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQGHkN41fdk2rg/article-inline_image-shrink_1500_2232/B4DZwofbKjJsAU-/0/1770205852790?e=1772668800&v=beta&t=1c7bsWmOBVdHA72_uqAbsQThFgf5dWaX6A7fRYFXAI8)

Tärkein valinta heti aloituksessa

2. **Luo pohja (base)**:

Pohjan luominen riippuu siitä millaisen kotelon haluat tehdä. Tärkeää on ajatella miten 3D-tulostin voi tulostaa kappaleen ja miten saat osat kotelon sisään (lopussa hieman ohjetta aiheeseen).

Kaikki alkaa Sketch:stä ja sen sijoittamisella oikealla X/Y/Z-akselistolle. Voit asettaa esim. Top ja tehdä ensimmäisen kuvan ylhäältä alaspäin katsoen. Katselusuunnan voi kyllä vaihtaa jälkikäteen jos tarpeen. Seuraavassa yksi tapa tehdä Sketch.

-   Piirrä suorakaide (Rectangle) Pi:n mittojen mukaan: 85 mm x 56 mm (lisää marginaalia jos tarpeen) ja pyöristä reunat R2.
-   Tee seinämää varten offsetti edellisestä, yleensä 2 mm seinämä on riittävä. Syvempi tulee ongelmaksi USB ja muiden porttien liittimien kanssa, eli paksu seinä estää liittimen sisäänmenon liittimeen.
-   Tarvittaessa tee valmiiksi offsetti 0,20 mm raolle, jos aiot tehdä esimerkiksi kannen, joka pysyy liukumalla kiinni.
-   Muista kiinnittää kaikki mitat ja käytä Constraints-työkalua esimerkiksi reikien kohdentamiseen. Constraints helpottaa muutoksia, sillä mitat seuraavat tällöin referenssipisteitäsi.
-   Extrude (purista) ensimmäisenä pohjaa alaspäin 2 mm ja sen jälkeen seinämiä ylöspäin haluamaasi korkeuteen.
-   Lisää kiinnityspilarit (standoffs): Piirrä ympyrät kiinnitysreikien paikkoihin ja extrude ne 5-7 mm korkeiksi. Lisää yläpäähän kierteet, jos käytät ruuveja.

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQFGv_bc_-tTeA/article-inline_image-shrink_1500_2232/B4DZwohAiOKwAY-/0/1770206268277?e=1772668800&v=beta&t=58FVEhV2b-W5MB8gLNy5Fr1XMz-QJgfWvdiCIyGksDs)

Esimerkki pohjan ja seinämien sketchistä

3. **Rakenna seinät**:

Huomioi, että Raspberryn portit tulevat ulos piirilevyltä. Eli suunnittele kotelosi leveämmäksi kuin piirilevy tai pohdi miten osat saa kotelon sisään ja jaa tarvittaessa useampi osa kokoonpanoa varten. Seuraavissa kuvissa esimerkkinä on tiukasti sovitettu kotelo, joka koostuu todellisuudessa kolmesta eri osasta, base, middle ja lid. Raspberry sijoitetaan baseen, joka rajoittuu porttien keskikohtaan, päälle laitetaan keskiosa, johon tässä tapauksessa tuetaan myös SeedStudion Lora hattu ja lopuksi kasi, johon tulee 868 MHz antenni ja GPS/GNSS-moduli.

-   Extruudaa aiemmin tehdyllä Sketchillä seinämät, esimerkiksi 20-45 mm ylöspäin riippuen miten väljän kotelon tarvitset.
-   Tuo lataamasi Raspberry Pi5 stl-tiedosto Insert - Mesh- toiminnolla osaksi suunnitelmaa ja aseta Raspberryn piirilevyn kiinnitysreiät Sketchin piirtämiesi reikien tai tappien kohdalle.
-   Piirrä seinämiin uudet Sketchit ja sovita reiät vähintään 0,2 mm suuremmaksi kuin itse liitin on.
-   Mitat kannattaa aina kiinnittää johonkin konktreettiseen kiintopisteeseen. Et voi kuitenkaan suoraan projisoida tuotua Raspberry Pi5 stl-tiedostoa ja käyttää sitä referenssipisteenä. Tässä kuvassa olen käyttänyt vasenta laitaa ja alareunaa referenssipisteenä, mutta kuten sinisistä viivoista näkyy, osa mitoista ei ole kiinnitetty.

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQELqkdDISAQWQ/article-inline_image-shrink_1500_2232/B4DZwojzMFJIAU-/0/1770207000328?e=1772668800&v=beta&t=2p8Rq-VW6ucLKKpYlXUQlJByxwJZXQOmmEoQOisPPTQ)

USB-C ja 2x HDMI porttien paikat oikealla sivulla

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQEA77zlRLnmTA/article-inline_image-shrink_1000_1488/B4DZwokJ37K4AU-/0/1770207093203?e=1772668800&v=beta&t=WxrgL0Vft71u73oCZp7Z9iSHgH9UwfAVzxFVxQ5HW6o)

Ethernet-portti ja USB-A modulit.

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQGovwx0YGWr4g/article-inline_image-shrink_1500_2232/B4DZwokl4gG8AU-/0/1770207207886?e=1772668800&v=beta&t=H54uijwJio1UsRcRupbp_-o2qcYZAFoKc_rzEAJAJec)

SD-slotti, reset painike ja ledin paikka

4. **Lisää kansi ja snap-fit -kiinnitys**:

Kiinnityksiä voi olla monenlaisia. Voit tehdä helposti ruuvikiinnityksen tai vastaavan, mutta itse suosin Snap Fit-kiinnitystä. Se onnistuu jättämällä kappaleiden väliin 0,2 mm rako ja tarvittaessa Extrudessa Taper Anglella.

-   Luo erillinen komponentti kannelle. Jos New Component valintaa ei löydy, olet aloittanut tekemään Parts-piirrosta. Se ei tue suoraan useita komponentteja. Tallenna tähän saakka tekemäsi ja tee uusi piirros lopuille Hybrid-mallilla ja tuo aiempi Add toiminnollla.
-   Voit käyttää myös Joint-työkalua (esim. Snap Fit) kiinnittämään kansi pohjaan. Tämä on hyvä tapa tehdä irrotettava kansi ilman ruuveja, tai luo pieniä "koukkuja" seinien yläreunaan, jotka napsahtavat paikalleen.
-   Lisää ilmanvaihtoaukot (vents) lämpöongelmien välttämiseksi: Piirrä reikiä tai ritilöitä kanteen ja pohjaan.

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQH3zhUoYgG9NQ/article-inline_image-shrink_1500_2232/B4DZwoluAoG4Ao-/0/1770207503370?e=1772668800&v=beta&t=6Q6EXmbpctbNlEC1eCemfjSUi9769V2HC6zWhe-kfU8)

Taiteellisuus on sallittua kunhan se toimii

Sitten kun olet tehnyt reitykset ja muut asiat kuntoon, käytä Inspect-työkaluja hahmottaaksesi mahdolliset ongelmakohdat. Accessibility-työkalu on myös hyvä. Tarvittaessa tee Joints ja Assemble osat yhteen nähdäksesi toiminnallisuutta.

Vaikka teet tarkistuksia, on todennäköistä, että unohdat jotain. Esimerkiksi alla olevassa videossa näkyy virhe SD-kortin aukossa ja etupaneelin USB-A moduulien laskussa. SD-korttia ei olisi voinut käytännössä vaihtaa kuin pinseteillä ja USB-A porttien alareunasta on unohtunut leikata U:n muotoinen pieni pala pois, eli piirilevy ei mennyt pohjaan.

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQHMvxdLwXZvWw/article-inline_image-shrink_1000_1488/B4DZwotHizGQAQ-/0/1770209443668?e=1772668800&v=beta&t=OGyaJX-Rb_kxGzM6w_IKIZ9GhvpSXapCrDrXmJIdtzQ)

Piirilevy jäi kantamaan USB-A moduulien vuoksi

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQFKQP63LtkmfQ/article-inline_image-shrink_1000_1488/B4DZwotSVkIYAQ-/0/1770209487863?e=1772668800&v=beta&t=opYQ5UF9mYeOoCdbYOK1aYDZJdnXkQyMIjiidP9-_4Q)

SD-kortin slottiin ei pääse käsiksi

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQG562RREj99vQ/article-inline_image-shrink_1000_1488/B4DZwotapYJkAQ-/0/1770209521733?e=1772668800&v=beta&t=o3zd_3RaudMSKxDDaFa0GlUYNHmKq3R5T-HbVbBUluY)

Pimeässä vihreänä heijastava PLA on aika hauska

6. **Vie mallit STL-tiedostoksi**:

-   Valitse kappale ja sitten File - Print - 3D Print ja siellä oletukset High ja STL (Ascii)
-   Huom! Mikäli kappale on toisessa tiedostossa (Parts), voit joutua tekemään 3D Printing sieltä etenkin jos olet unohtanut alkuperäisessä tiedostossa kyseisen tulostettavan Bodyn piiloon.

Sitten kun olet valmis, siirrytään tulostamismallin luomiseen (näkyy alla olevalla videolla myös).

## Mitä voi tulostaa ja mitä ei?

Suunnittelussa on tärkeää ajatella tulostimen rajoituksia. FDM-tulostimet rakentavat kerros kerrokselta, joten:

**Mitä voi tulostaa**:

-   Yksinkertaiset muodot: suorat seinät, pyöreät aukot, tasaiset pinnat.
-   Ylitykset (overhangs) alle 45 astetta ilman tukia (supports).
-   Pienet yksityiskohdat yli 0.4 mm (nozzle-koko vaikuttaa).
-   Snap-fit -kiinnitykset tai kierteet, jos ne ovat vaaka-asennossa.

**Mitä ei kannata tulostaa**:

-   Suljetut reiät keskellä kappaletta: tukimateriaali (supports) on vaikea poistaa sisältä.
-   Ohuet seinät alle 0.8 mm: ne voivat vääntyä tai katkeilla.
-   Pienet osat (alle 5 mm): kerrokset eivät tartu hyvin, ja osa katkeaa helposti kerrosten välistä.
-   Suuret ylitykset ilman tukia: tulostin ei pysty rakentamaan "ilmassa".
-   Monimutkaiset kierteet pystysuunnassa: parempi suunnitella insertit (esim. heat-set insertit ruuveille).

Vinkki: suunnittele malli tulostusasento mielessä, jaa se osiin, jos tarpeen (esim. pohja ja kansi erikseen). Käytä Curaa tarkistaaksesi, tarvitseeko tukia.

## Tekeleen tulostaminen ja puhdistus

1. **Vie STL ja avaa Curassa**:

-   Aseta Cura-asetukset: layer height 0.2 mm, infill 20-30% (kotelolle riittää), supports tarvittaessa.
-   Pedin lämpötila riippuu filamentista, PLA:ssa yleensä 60 astetta riittää ja suuttimeen 220-240 astetta.
-   Cura:ssa on aika hyvin optimoitu siirtymät ja muut asiat, mutta jos tuleste esimerkiksi "räkii", hidasta nopeutta.
-   Orientoi malli oikein: litteä puoli alaspäin parhaan tartunnan saamiseksi (lay flat).

2. **Tulosta**:

-   Kalibroi tulostin (bed leveling, flow rate).
-   Käytä brim tai raft, tai nosta pedin lämpötilaa, jos tartunta on heikko.
-   Tulostusaika: yksinkertainen kotelo kestää 1-3 tuntia riippuen koosta.

3. **Puhdistus**:

-   Anna tulosteen jäähtyä kunnolla, jotta supportit irtoavat helpommin.
-   Poista tukimateriaalit pihdeillä ja veitsellä varovasti. Slottien tukimateriaali lähtee yleensä napauttamalla vaikkapa ruuvimeisselillä.
-   Hio tarvittaessa reunat hiekkapaperilla (220-400) sileämmiksi tai käytä tarkoitukseen tehtyä kaiverrinta (suosittelen).
-   Jos käytät inserttejä, lämmitä ne kolvilla ja paina paikalleen.
-   Testaa istuvuus: jos liian tiukka, hio tai suunnittele uudelleen isommalla raolla.

Tällä videolla näet hieman miten tarkistusta voi tehdä. Tämän tulosteen jälkeen syntyi ensimmäinen versio, jossa virheet tulivat selvästi esille.

Ensimmäisen draftin video

## Entä sitten se milestone-versio, miltä se näyttää?

Edellä olevan kotelon tulostusaika oli yli 5 tuntia ja tulostusjälki on riittävän hyvää, vaikka kaikki osat ovat kerrallaan pedillä. Jos haluat desingin,pyydä sitä. Poistin Thingiversestä kaiken, koska he blokkaavat aiheettomasti ihmisiä pois verkostaan (CloudFare). Etsin Eurooppalaista vaihtoehtoa desingeilleni. OpenSourcen ja OpenHardwaren idea on nimenomaan jakaa teoksia muiden kanssa. Minun tuotokseni ovat vapaasti käytettävissä ilman kaupallistamista (eli et saa tehdä tällä rahaa, CC BY-NC).

Tulostuksen jälkeen puhdista roiskeet ja ota supportit varovasti pois. Tässä tulosteessa oli hieman heittoa keskiosan sivussa, ilmeisesti filamentin syötössä tapahtunut jokin häiriö. Se ei kuitenkaan menoa haittaa. Alla muutamia kuvia valmiista projektista.

![Raspberry PI5 alaosaan kiinnitettynä](https://media.licdn.com/dms/image/v2/D4D12AQHxNbvpQHd4OA/article-inline_image-shrink_1000_1488/B4DZwpbRPeIUAU-/0/1770221541917?e=1772668800&v=beta&t=m00JCFYiUSqL8QuXr_YTJppPhwRVHzp_I-cwwIbW4Z8)

Bottom ja Raspberry PI5

Raspberryn piirilevy osuu juuri oikein reikiin ja USB-C, USB-A moduliet ja Ethernet portit osuvat kohdilleen.

![USB slottien sivusta katsottuna](https://media.licdn.com/dms/image/v2/D4D12AQF_Fl7dQ2pz7A/article-inline_image-shrink_1000_1488/B4DZwpbZD0IUAU-/0/1770221573806?e=1772668800&v=beta&t=e9P4mxdQp-IV87UR2WQbhyC9mBTdrkbn6X3Op4pfTng)

Mid

WM1302 SeedStudio LoraWan hatun USB-C reikäkin osuu johdalleen tarkasti ja portit ovat nyt kauniisti paikoillaan. Rakoja ei näy.

![Takaa katsottuna](https://media.licdn.com/dms/image/v2/D4D12AQFQAQoV0ENLRQ/article-inline_image-shrink_1000_1488/B4DZwpbf13IUAQ-/0/1770221601692?e=1772668800&v=beta&t=8wuWPNrxXRv-bLR17rxScYnavnrnr0gsMdzMDMpRcYM)

Back

Takaa katsottuna SD-kortti on nyt vaihdettavissa ja resetin voi painaa jos tarvitsee. Oikeassa reunassa näkyy pientä rakoa ja se johtuu kirjoittimesta tai käyttämästäni vihreässä loistavasta PLA-filamentista.

![Antennit kiinnitettynä](https://media.licdn.com/dms/image/v2/D4D12AQFnrkpgD7lnSA/article-inline_image-shrink_1000_1488/B4DZwpbmoUHwAQ-/0/1770221629546?e=1772668800&v=beta&t=fxFo6NPW3FDub-WNxrKugWuARJ5xQ5wEtQuLwkiPZH8)

Installation

Asennuksen kannalta 868 MHz antennin juuri ja asennus piirilevylle sekä GPS/GNSS modulin antennit on helppo laittaa paikalleen. GPS/GNSS moduli menee sivuttain kannen reiän lävitse.

![Valmis paketti](https://media.licdn.com/dms/image/v2/D4D12AQHAR6eKxRuAVg/article-inline_image-shrink_1000_1488/B4DZwpbvpzHMAc-/0/1770221666469?e=1772668800&v=beta&t=M__3d2GNATrcZ3XtpDbekyaNiira4abrMyX-6KP4H3Y)

Complete setup

Valmis paketti. Toimiva, joskaan ei ihan kultaisen leikkauksen mukainen (1,618 : 1) joka olisi silmälle se kaunein vaihtoehto.

![Takaa katsottuna, ensimmäisen kerran virrat päällä](https://media.licdn.com/dms/image/v2/D4D12AQEgu43ZG5FhZg/article-inline_image-shrink_1000_1488/B4DZwpcBRzHEAU-/0/1770221738419?e=1772668800&v=beta&t=h1FYWS6uNrA27Pb8e9IqAqzx1HZmIKMAVyeOufBkywY)

Back and powered on first time

Ensimmäisen kerran virrat päälle...

![Artikkelin sisältö](https://media.licdn.com/dms/image/v2/D4D12AQGbzU-MB-FbwQ/article-inline_image-shrink_1000_1488/B4DZwpc60zJkAU-/0/1770221974510?e=1772668800&v=beta&t=VzvjoJtB6XTGmNryd8zvgIHFwM0bHHy2K2FlmNVYQ_A)

First boot

Ja sieltäpä se Raspberry heräilee. Tässä vaiheessa siirrynkin sitten softapuoleen ja siirtämään vanhasta Raspberry 3B:stä mqtt, influxdb tietoineen ja Grafanan tähän laitteeseen. Lisäksi tulee LoraWan (sille myös sensoreita) ja kotiautomaatiota samaan purkkiin. Siksi Raspberry PI5 8GB muistilla ja erillisellä SSD-levyllä.

Nyt on sinun vuorosi suunnitella laite ja julkistaa se meidän muiden iloksi!

Kiitos lukemisesta!