# Kolica — test verzija za mobitel

Mobitel glumi GPS uređaj na kolicima. Skripta pamti mjesto gdje je pokrenuta (centar)
i šalje **e-mail** kad se mobitel udalji više od 100 m, te kad se vrati.
Mail sadrži udaljenost i link na Google Maps.

| Datoteka | Opis |
|---|---|
| `kolica_mobitel.py` | Glavna skripta za mobitel. Postavke su na vrhu. |
| `geofence.py` | Logika geo-ograde. Ista datoteka kasnije radi i na ESP32 (MicroPython). |

> Na mobitelu se koristi običan Python. MicroPython je inačica Pythona za mikrokontrolere
> poput ESP32. `geofence.py` je napisan tako da radi u oba.

---

## 1. Gmail lozinka za aplikacije (App Password)

Gmail ne dopušta prijavu skripti s običnom lozinkom, nego traži posebnu "lozinku za aplikacije".

1. Na Google računu uključi **potvrdu u dva koraka** (*Sigurnost → Potvrda u dva koraka*).
2. Otvori <https://myaccount.google.com/apppasswords>, upiši naziv npr. `Kolica` i klikni *Izradi*.
3. Kopiraj dobivenu lozinku od 16 znakova.
4. Na vrhu `kolica_mobitel.py` upiši:
   ```python
   POSILJATELJ = "tvoj.mail@gmail.com"
   APP_LOZINKA = "abcd efgh ijkl mnop"
   PRIMATELJ   = "tvoj.mail@gmail.com"   # može biti i neka druga adresa
   ```

Tko ima tu datoteku, može slati mailove s tvog računa. Nemoj je dijeliti, a nakon
testiranja možeš lozinku obrisati na istoj stranici. Još sigurnije je za slanje
napraviti zaseban Gmail račun.

---

## 2a. Android: Termux

1. Instaliraj **F-Droid** (<https://f-droid.org>), a preko njega aplikacije **Termux** i **Termux:API**.
   Obje moraju biti iz F-Droida, jer verzije iz Play Storea ne rade zajedno s ovima s F-Droida.
2. *Postavke → Aplikacije → Termux:API → Dopuštenja → Lokacija → Dopusti*
   (ako postoji opcija *Uvijek dopusti*, odaberi nju).
3. *Postavke → Aplikacije → Termux → Baterija → Neograničeno*, inače ga Android ugasi.
4. Preuzmi `kolica_mobitel.py` i `geofence.py` u mapu *Download*. Zatim u Termuxu redom:

```bash
pkg update && pkg install python termux-api
```
```bash
termux-setup-storage
```
```bash
mkdir -p ~/kolica && cp ~/storage/downloads/kolica_mobitel.py ~/storage/downloads/geofence.py ~/kolica/ && cd ~/kolica
```
```bash
termux-location
```
Zadnja naredba treba ispisati koordinate. Ako u kući ništa ne ispiše, izađi van
ili probaj `termux-location -p network`.

Ako postavke maila nisi upisao prije prebacivanja, uredi datoteku s `nano kolica_mobitel.py`
(spremanje: Ctrl+O, Enter, izlaz: Ctrl+X).

Pokretanje (vidi korake testiranja niže):
```bash
termux-wake-lock
```
```bash
python kolica_mobitel.py
```
`termux-wake-lock` sprječava da Android uspava skriptu kad zaključaš ekran.

## 2b. iPhone: Pythonista 3

1. Instaliraj **Pythonista 3** iz App Storea (aplikacija se plaća).
2. Stavi `kolica_mobitel.py` i `geofence.py` u **istu mapu** u Pythonisti
   (preko aplikacije *Datoteke* ili kopiraj-zalijepi sadržaj u nove skripte istog imena).
3. Upiši postavke maila na vrh `kolica_mobitel.py`.
4. Pokreni s ▶. Za argumente poput `--test-mail` **drži ▶** i odaberi *Run with Arguments*.
5. Kad iPhone pita, dopusti pristup lokaciji.

**Ograničenje iOS-a:** skripta radi samo dok je Pythonista otvorena na ekranu.
Skripta sama sprječava gašenje ekrana, ali ne prebacuj se u druge aplikacije.

---

## 3. Testiranje, redom

| Korak | Naredba | Što očekivati |
|---|---|---|
| 1 | `python kolica_mobitel.py --simulacija --bez-maila` | Izmišljena šetnja do 210 m i natrag. U ispisu se pojave 2 "maila". Ne treba GPS ni internet. |
| 2 | `python kolica_mobitel.py --test-mail` | U sandučić stiže "Kolica: probni mail". Provjeri i *Spam*. |
| 3 | `python kolica_mobitel.py --simulacija` | Ista šetnja, ali stižu 2 prava maila. |
| 4 | `python kolica_mobitel.py` | Pravi test, opisan niže. |

**Pravi test:** pokreni skriptu vani i stoj na mjestu oko 30 sekundi, dok se ne ispiše `UNUTRA`.
Zatim se udalji više od 100 m. Mail stiže nakon 3 uzastopna očitanja izvan zone (oko 30 s).
Kad se vratiš bliže od 90 m, stiže mail "vratila su se". Dok si vani, mail s novom lokacijom
ponavlja se svakih 5 minuta. Za kraj pritisni Ctrl+C ili zaustavi skriptu.

Ako mail ne uspije (nema signala), skripta ga čuva i šalje čim se veza vrati.

## Postavke (vrh `kolica_mobitel.py`)

| Postavka | Zadano | Značenje |
|---|---|---|
| `RADIJUS_M` | 100 | Dopušteni radijus. Za kraći test stavi 50, ali ne ispod ~30 m (GPS griješi nekoliko metara). |
| `HISTEREZA_M` | 10 | Povratak se priznaje tek na `RADIJUS_M - HISTEREZA_M` |
| `BROJ_POTVRDA` | 3 | Koliko očitanja zaredom mora biti vani prije maila |
| `INTERVAL_S` | 10 | Sekundi između očitanja |
| `MAX_NETOCNOST_M` | 30 | Očitanja s lošijom točnošću se preskaču |
| `PONAVLJANJE_S` | 300 | Ponavljanje maila dok su kolica vani (0 = isključeno) |
| `CENTAR_LAT/LON` | `None` | Fiksni centar umjesto mjesta pokretanja |
| `ANDROID_IZVOR` | `"gps"` | `"network"` radi i u zatvorenom, ali je manje točan |
