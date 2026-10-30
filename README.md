# Kolica GPS

GPS geo-ograda za električna kolica: kad se kolica udalje više od zadanog radijusa
od svog centra, oglasi se alarm i stiže obavijest s lokacijom.

Projekt ima tri verzije s istom logikom: radijus, histereza od 10 m i potvrda kroz
više uzastopnih očitanja, da pojedinačni GPS skokovi ne pale alarm.

| Verzija | Gdje radi | Obavijest |
|---|---|---|
| [Web app](#web-app) (`index.html`) | Chrome na mobitelu | e-mail, zvuk, vibracija |
| [Python](mobitel/README.md) (`mobitel/`) | Termux (Android), Pythonista (iPhone) | e-mail preko Gmaila |
| [ESP32](#esp32) (`kolica_geofence/`) | uređaj ugrađen u kolica | zujalica, SMS (SIM800L) |

---

## Web app

**Otvori:** <https://leonbtr1.github.io/kolica-gps/>

1. Otvori adresu u Chromeu na mobitelu.
2. U *Postavke* upiši svoju e-mail adresu i pritisni **Pošalji probni mail**.
   Prvi put stiže mail od FormSubmita: klikni **Activate Form**, pa ponovno pošalji probni mail.
3. Pritisni **Simuliraj šetnju**. Za oko 20 sekundi trebaju stići dva maila s oznakom `[SIMULACIJA]`.
4. Pravi test: pritisni **Pokreni praćenje** i dopusti lokaciju. Mjesto gdje stojiš postaje centar.
   Odšetaj dalje od radijusa (zadano 100 m).

Mail stiže nakon 5 uzastopnih očitanja izvan zone (oko 5 s). Dok su kolica vani,
novi mail s lokacijom stiže svakih 5 minuta, a kad se vrate, stiže mail o povratku.
Ako nema interneta, mail čeka i šalje se čim se veza vrati.

**Ograničenje preglednika:** lokacija se prati samo dok je stranica otvorena i ekran upaljen.
Stranica sama sprječava gašenje ekrana. Za praćenje sa zaključanim ekranom
koristi Python verziju u Termuxu ili ESP32 uređaj.

Postavke (e-mail, radijus, centar) spremaju se samo u pregledniku na tvom mobitelu.

Slanje ide preko [FormSubmita](https://formsubmit.co), bez registracije. Za slanje preko
vlastitog Gmaila otvori *Slanje preko vlastitog Gmaila (EmailJS)* u postavkama i prati upute.

---

## ESP32

Arduino sketch za **ESP32** s GPS modulom **NEO-6M**. Opcionalno šalje SMS preko **SIM800L**.

### Komponente

- **ESP32 DevKit** (klasični ESP32, ima 3 UART-a)
- **GPS modul** NEO-6M / NEO-7M / NEO-8M s antenom
- **Aktivna zujalica** 5 V + NPN tranzistor (npr. 2N2222) i otpornik 1 kΩ
- **DC-DC step-down pretvarač** (baterija kolica 24/36/48 V → 5 V)
- *Opcionalno:* **SIM800L** + SIM kartica za SMS obavijesti

### Spajanje

| Komponenta | Pin komponente | ESP32 |
|---|---|---|
| GPS | VCC | 5V (ili 3V3, ovisno o modulu) |
| GPS | GND | GND |
| GPS | TX | GPIO 16 |
| GPS | RX | GPIO 17 |
| Zujalica (preko tranzistora) | baza tranzistora kroz 1 kΩ | GPIO 25 |
| Tipka | jedna strana na GND | GPIO 0 (ili ugrađena BOOT tipka) |
| SIM800L | TX | GPIO 26 |
| SIM800L | RX | GPIO 27 |

> **SIM800L** treba zasebno napajanje od **4,0 V** koje može dati vršnih **2 A**
> (zaseban step-down pretvarač). Iz 3V3 ili 5V pina ESP32 neće raditi pouzdano.
> GND svih modula mora biti zajednički.

GPS antena mora "vidjeti nebo", dakle ne pod metalnim dijelovima kolica.

### Instalacija

1. Arduino IDE → *Boards Manager* → instaliraj **esp32** (Espressif Systems).
2. *Library Manager* → instaliraj **TinyGPSPlus** (Mikal Hart).
3. Otvori `kolica_geofence/kolica_geofence.ino`, odaberi ploču *ESP32 Dev Module* i uploadaj.
4. Otvori Serial Monitor na **115200** bauda.

### Postavke (vrh `.ino` datoteke)

| Postavka | Zadano | Značenje |
|---|---|---|
| `RADIJUS_M` | 100 | Dopušteni radijus u metrima |
| `HISTEREZA_M` | 10 | Povratak u zonu priznaje se tek na 90 m, pa alarm ne "treperi" na granici |
| `BROJ_POTVRDA` | 5 | Koliko uzastopnih očitanja (≈ sekundi) mora biti izvan zone prije alarma |
| `MIN_SATELITA`, `MAX_HDOP` | 5, 2.5 | Lošija GPS očitanja se ignoriraju |
| `PONAVLJANJE_OBAVIJESTI_MS` | 5 min | Dok su kolica vani, obavijest s novom lokacijom se ponavlja |
| `KORISTI_FIKSNI_CENTAR` | false | `true` = centar je uvijek `FIKSNI_CENTAR_LAT/LON` |
| `KORISTI_GSM` | 0 | `1` = šalji SMS na `BROJ_TELEFONA` |

### Korištenje

- **Prvo pokretanje:** ako centar nije spremljen, prvi ispravan GPS fix postaje centar
  i pamti se i nakon gašenja.
- **Tipka držana 3 s:** trenutna lokacija postaje novi centar.
- **Kratki pritisak tipke:** utišava zujalicu. Obavijesti i dalje stižu.
- **LED:** sporo treptanje = čeka GPS, stalno upaljena = unutar zone,
  brzo treptanje = izvan zone.

Ne drži BOOT tipku (GPIO 0) dok se ESP32 uključuje jer će ući u način za programiranje.

---

## Testovi logike

Iz glavne mape projekta:
```bash
g++ -std=c++17 -Ikolica_geofence test/test_geofence.cpp -o test/test_geofence && test/test_geofence
```
```bash
python3 test/test_geofence.py
```
