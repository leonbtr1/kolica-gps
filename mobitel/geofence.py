# geofence.py — logika "geo-ograde" za kolica.
# Radi i na običnom Pythonu (mobitel, računalo) i na MicroPythonu (ESP32),
# pa se ista datoteka kasnije može prebaciti na kolica bez izmjena.

import math

RADIJUS_ZEMLJE_M = 6371000.0

# Stanja
NEPOZNATO = 0
UNUTRA = 1
IZVAN = 2

# Događaji koje vraća Ograda.azuriraj()
NISTA = 0
IZASLA = 1
VRATILA_SE = 2


def udaljenost_m(lat1, lon1, lat2, lon2):
    """Udaljenost između dvije GPS točke u metrima (Haversine formula)."""
    d_lat = math.radians(lat2 - lat1)
    d_lon = math.radians(lon2 - lon1)
    a = (math.sin(d_lat / 2) ** 2 +
         math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) *
         math.sin(d_lon / 2) ** 2)
    return RADIJUS_ZEMLJE_M * 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))


class Ograda:
    def __init__(self, radijus_m, histereza_m=10.0, broj_potvrda=3):
        self.radijus_m = radijus_m
        # Povratak se priznaje tek unutar (radijus - histereza), da alarm
        # ne "treperi" kad su kolica točno na granici.
        self.histereza_m = histereza_m
        # Koliko uzastopnih očitanja treba za promjenu stanja — filtrira
        # pojedinačne GPS "skokove".
        self.broj_potvrda = broj_potvrda
        self.ima_centar = False
        self.centar_lat = 0.0
        self.centar_lon = 0.0
        self.zadnja_udaljenost = 0.0
        self.reset()

    def postavi_centar(self, lat, lon):
        self.centar_lat = lat
        self.centar_lon = lon
        self.ima_centar = True
        self.reset()

    def reset(self):
        self.stanje = NEPOZNATO
        self._kandidat = NEPOZNATO
        self._brojac = 0

    def azuriraj(self, lat, lon):
        """Poziva se za svako ispravno očitanje. Vraća NISTA, IZASLA ili VRATILA_SE."""
        if not self.ima_centar:
            return NISTA

        d = udaljenost_m(self.centar_lat, self.centar_lon, lat, lon)
        self.zadnja_udaljenost = d

        if self.stanje == IZVAN:
            kandidat = UNUTRA if d <= self.radijus_m - self.histereza_m else IZVAN
        else:
            kandidat = IZVAN if d > self.radijus_m else UNUTRA

        if kandidat == self.stanje:
            self._brojac = 0
            return NISTA

        if kandidat != self._kandidat:
            self._kandidat = kandidat
            self._brojac = 0
        self._brojac += 1
        if self._brojac < self.broj_potvrda:
            return NISTA

        prethodno = self.stanje
        self.stanje = kandidat
        self._kandidat = NEPOZNATO
        self._brojac = 0

        if kandidat == IZVAN:
            return IZASLA
        if prethodno == IZVAN:
            return VRATILA_SE
        return NISTA  # prvo potvrđeno stanje je "unutra" — nema događaja
