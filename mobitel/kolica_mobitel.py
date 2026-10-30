#!/usr/bin/env python3
"""
Električna kolica — TEST verzija za mobitel.

Mobitel glumi GPS uređaj na kolicima: prati svoju lokaciju i šalje e-mail
kad se udalji više od zadanog radijusa od mjesta gdje je skripta pokrenuta.

Radi na:  Android (Termux + Termux:API)  i  iPhone (Pythonista 3).

    python kolica_mobitel.py                          # pravo praćenje
    python kolica_mobitel.py --test-mail              # samo pošalji probni mail
    python kolica_mobitel.py --simulacija --bez-maila # izmišljena ruta, ispis umjesto maila
"""

import argparse
import json
import shutil
import smtplib
import ssl
import subprocess
import sys
import time
from email.message import EmailMessage

import geofence

# ============================ POSTAVKE ===============================
RADIJUS_M = 100.0        # dopušteni radijus oko centra (m)
HISTEREZA_M = 10.0       # povratak se priznaje tek na RADIJUS_M - HISTEREZA_M
BROJ_POTVRDA = 3         # uzastopnih očitanja izvan zone prije obavijesti
INTERVAL_S = 10          # razmak između GPS očitanja (s)
MAX_NETOCNOST_M = 30.0   # očitanja lošije točnosti se ignoriraju
PONAVLJANJE_S = 5 * 60   # dok su kolica vani, ponovi mail s novom lokacijom (0 = isključeno)
GPS_TIMEOUT_S = 120      # bez ispravnog signala ovoliko = upozorenje u ispisu

# None = centar je mjesto gdje se skripta pokrene. Ili upiši fiksne koordinate.
CENTAR_LAT = None
CENTAR_LON = None

# Samo Android: "gps" (točnije, treba otvoreno nebo) ili "network" (radi i u kući)
ANDROID_IZVOR = "gps"

# Gmail: APP_LOZINKA je "App Password" od 16 znakova, NE tvoja obična lozinka.
POSILJATELJ = "tvoj.mail@gmail.com"
APP_LOZINKA = "xxxx xxxx xxxx xxxx"
PRIMATELJ = "tvoj.mail@gmail.com"
SMTP_SERVER = "smtp.gmail.com"
SMTP_PORT = 465
# =====================================================================

NAZIV_STANJA = {
    geofence.NEPOZNATO: "NEPOZNATO",
    geofence.UNUTRA: "UNUTRA",
    geofence.IZVAN: "IZVAN ZONE",
}


def vrijeme():
    return time.strftime("%H:%M:%S")


def link_na_kartu(lat, lon):
    return "https://maps.google.com/?q=%.6f,%.6f" % (lat, lon)


# ----------------------------- GPS izvori ----------------------------
# Svaki izvor ima procitaj() koji vraća (lat, lon, tocnost_m) ili None.

class TermuxGPS:
    naziv = "Android (Termux)"

    def procitaj(self):
        try:
            r = subprocess.run(["termux-location", "-p", ANDROID_IZVOR, "-r", "once"],
                               capture_output=True, text=True, timeout=90)
            d = json.loads(r.stdout)
        except (subprocess.TimeoutExpired, ValueError):
            return None
        if "latitude" not in d:
            return None
        return d["latitude"], d["longitude"], d.get("accuracy", 9999.0)


class PythonistaGPS:
    naziv = "iPhone (Pythonista)"

    def __init__(self):
        import console
        import location
        self.location = location
        location.start_updates()
        console.set_idle_timer_disabled(True)  # iOS gasi skriptu kad ekran zaspe

    def procitaj(self):
        d = self.location.get_location()
        if not d:
            return None
        return d["latitude"], d["longitude"], d.get("horizontal_accuracy", 9999.0)


class SimuliraniGPS:
    """Izmišljena šetnja: od centra 210 m na sjever i natrag, korak 35 m."""
    naziv = "simulacija"

    def __init__(self, lat, lon):
        m_po_stupnju = geofence.RADIJUS_ZEMLJE_M * 3.141592653589793 / 180
        put = [0] * 4 + list(range(35, 211, 35)) + [210] * 2 + list(range(175, -1, -35)) + [0] * 2
        self.tocke = [(lat + d / m_po_stupnju, lon) for d in put]
        self.gotovo = False

    def procitaj(self):
        lat, lon = self.tocke.pop(0)
        self.gotovo = not self.tocke
        return lat, lon, 5.0


def odaberi_gps(simulacija):
    if simulacija:
        lat = CENTAR_LAT if CENTAR_LAT is not None else 45.815011
        lon = CENTAR_LON if CENTAR_LON is not None else 15.981919
        return SimuliraniGPS(lat, lon)
    if shutil.which("termux-location"):
        return TermuxGPS()
    try:
        return PythonistaGPS()
    except ImportError:
        pass
    sys.exit("Nije pronađen GPS.\n"
             "  Android: u Termuxu pokreni 'pkg install termux-api' i instaliraj aplikaciju Termux:API.\n"
             "  iPhone:  pokreni skriptu u aplikaciji Pythonista 3.\n"
             "  Proba na računalu: python kolica_mobitel.py --simulacija --bez-maila")


# ------------------------------- E-mail ------------------------------

class Posta:
    """Šalje mailove; ako nema interneta, čuva ih i pokušava ponovno."""

    def __init__(self, bez_maila=False):
        self.bez_maila = bez_maila
        self.neposlano = []

    def posalji(self, naslov, tekst):
        self.neposlano.append((naslov, tekst))  # šalje se u isprazni() na kraju kruga

    def isprazni(self):
        while self.neposlano and self.posalji_odmah(*self.neposlano[0]):
            self.neposlano.pop(0)

    def posalji_odmah(self, naslov, tekst):
        print("[MAIL] " + naslov)
        if self.bez_maila:
            print("       " + tekst.replace("\n", "\n       "))
            return True

        poruka = EmailMessage()
        poruka["Subject"] = naslov
        poruka["From"] = POSILJATELJ
        poruka["To"] = PRIMATELJ
        poruka.set_content(tekst)
        try:
            with smtplib.SMTP_SSL(SMTP_SERVER, SMTP_PORT, timeout=30,
                                  context=ssl.create_default_context()) as s:
                s.login(POSILJATELJ, APP_LOZINKA.replace(" ", ""))
                s.send_message(poruka)
        except smtplib.SMTPAuthenticationError:
            print("       GREŠKA: Gmail je odbio prijavu — provjeri POSILJATELJ i APP_LOZINKA.")
            return False
        except (OSError, smtplib.SMTPException) as e:
            print("       Slanje nije uspjelo (%s) — pokušat ću ponovno." % e)
            return False
        print("       poslano na " + PRIMATELJ)
        return True


# ---------------------------- Glavna petlja --------------------------

def opis_lokacije(ograda, lat, lon, tocnost):
    return ("Vrijeme: %s\n"
            "Udaljenost od centra: %.0f m (dopušteno %.0f m)\n"
            "Točnost GPS-a: ±%.0f m\n"
            "Lokacija: %s"
            % (time.strftime("%d.%m.%Y. %H:%M:%S"), ograda.zadnja_udaljenost,
               RADIJUS_M, tocnost, link_na_kartu(lat, lon)))


def main():
    p = argparse.ArgumentParser(description="Kolica — GPS geo-ograda (test na mobitelu)")
    p.add_argument("--test-mail", action="store_true", help="pošalji probni mail i izađi")
    p.add_argument("--simulacija", action="store_true", help="izmišljena ruta umjesto GPS-a")
    p.add_argument("--bez-maila", action="store_true", help="ispiši mailove umjesto slanja")
    args = p.parse_args()

    posta = Posta(args.bez_maila)

    if args.test_mail:
        ok = posta.posalji_odmah("Kolica: probni mail",
                                 "Ako ovo čitaš, slanje obavijesti radi.")
        sys.exit(0 if ok else 1)

    gps = odaberi_gps(args.simulacija)
    interval = 0.3 if args.simulacija else INTERVAL_S
    ograda = geofence.Ograda(RADIJUS_M, HISTEREZA_M, BROJ_POTVRDA)
    if CENTAR_LAT is not None and not args.simulacija:
        ograda.postavi_centar(CENTAR_LAT, CENTAR_LON)

    print("GPS izvor: %s | radijus %.0f m | Ctrl+C za kraj" % (gps.naziv, RADIJUS_M))

    zadnji_ispravan = time.time()
    gps_izgubljen = False
    zadnja_obavijest = 0.0

    try:
        while not getattr(gps, "gotovo", False):
            ocitanje = gps.procitaj()
            sada = time.time()

            if ocitanje is None or ocitanje[2] > MAX_NETOCNOST_M:
                if ocitanje is None:
                    print("%s  nema GPS očitanja" % vrijeme())
                else:
                    print("%s  očitanje preneprecizno (±%.0f m), preskačem" % (vrijeme(), ocitanje[2]))
                if not gps_izgubljen and sada - zadnji_ispravan > GPS_TIMEOUT_S:
                    gps_izgubljen = True
                    print("Upozorenje: nema ispravnog GPS signala (zatvoren prostor?).")

            else:
                zadnji_ispravan = sada
                gps_izgubljen = False
                lat, lon, tocnost = ocitanje

                if not ograda.ima_centar:
                    ograda.postavi_centar(lat, lon)
                    print("%s  centar postavljen: %.6f, %.6f" % (vrijeme(), lat, lon))
                else:
                    dogadaj = ograda.azuriraj(lat, lon)
                    print("%s  udaljenost %6.1f m (±%.0f m)  %s"
                          % (vrijeme(), ograda.zadnja_udaljenost, tocnost,
                             NAZIV_STANJA[ograda.stanje]))

                    if dogadaj == geofence.IZASLA:
                        zadnja_obavijest = sada
                        posta.posalji("UPOZORENJE: kolica su izašla iz zone",
                                      opis_lokacije(ograda, lat, lon, tocnost))
                    elif dogadaj == geofence.VRATILA_SE:
                        posta.posalji("Kolica su se vratila u zonu",
                                      opis_lokacije(ograda, lat, lon, tocnost))
                    elif (ograda.stanje == geofence.IZVAN and PONAVLJANJE_S > 0
                          and sada - zadnja_obavijest >= PONAVLJANJE_S):
                        zadnja_obavijest = sada
                        posta.posalji("Kolica su i dalje izvan zone",
                                      opis_lokacije(ograda, lat, lon, tocnost))

            posta.isprazni()
            time.sleep(interval)
    except KeyboardInterrupt:
        print("\nZaustavljeno.")


if __name__ == "__main__":
    main()
