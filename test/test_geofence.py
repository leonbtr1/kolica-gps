# Testovi za mobitel/geofence.py (isti slučajevi kao test_geofence.cpp):
#   python3 test_geofence.py
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "mobitel"))
import geofence as g  # noqa: E402

C_LAT, C_LON = 45.815011, 15.981919  # Zagreb
M_PO_STUPNJU = g.RADIJUS_ZEMLJE_M * math.pi / 180


def lat_na(d):
    """Točka udaljena d metara sjeverno od centra."""
    return C_LAT + d / M_PO_STUPNJU


def ponovi(o, d, puta):
    zadnji = g.NISTA
    for _ in range(puta):
        e = o.azuriraj(lat_na(d), C_LON)
        if e != g.NISTA:
            zadnji = e
    return zadnji


def test_udaljenost():
    assert abs(g.udaljenost_m(0, 0, 0, 1) - 111194.9) < 1.0
    assert abs(g.udaljenost_m(C_LAT, C_LON, lat_na(100), C_LON) - 100.0) < 0.01
    zg_st = g.udaljenost_m(45.815011, 15.981919, 43.508133, 16.440193)
    assert 255000 < zg_st < 265000


def test_bez_centra():
    o = g.Ograda(100, 10, 5)
    assert o.azuriraj(lat_na(500), C_LON) == g.NISTA
    assert o.stanje == g.NEPOZNATO


def test_izlazak_i_povratak():
    o = g.Ograda(100, 10, 5)
    o.postavi_centar(C_LAT, C_LON)

    assert ponovi(o, 20, 5) == g.NISTA
    assert o.stanje == g.UNUTRA

    # pojedinačni GPS skokovi ne pale alarm
    assert ponovi(o, 150, 4) == g.NISTA
    assert ponovi(o, 20, 1) == g.NISTA
    assert o.stanje == g.UNUTRA

    # pravi izlazak: točno na 5. uzastopnom očitanju
    assert ponovi(o, 120, 4) == g.NISTA
    assert o.azuriraj(lat_na(120), C_LON) == g.IZASLA
    assert o.stanje == g.IZVAN
    assert abs(o.zadnja_udaljenost - 120) < 0.1

    assert ponovi(o, 300, 10) == g.NISTA

    # pojas histereze (95 m) još nije povratak
    assert ponovi(o, 95, 20) == g.NISTA
    assert o.stanje == g.IZVAN

    assert ponovi(o, 50, 4) == g.NISTA
    assert o.azuriraj(lat_na(50), C_LON) == g.VRATILA_SE
    assert o.stanje == g.UNUTRA


def test_start_izvan_zone():
    o = g.Ograda(100, 10, 5)
    o.postavi_centar(C_LAT, C_LON)
    assert ponovi(o, 400, 5) == g.IZASLA


def test_novi_centar_resetira():
    o = g.Ograda(100, 10, 5)
    o.postavi_centar(C_LAT, C_LON)
    ponovi(o, 400, 5)
    o.postavi_centar(lat_na(400), C_LON)
    assert o.stanje == g.NEPOZNATO
    assert ponovi(o, 400, 5) == g.NISTA
    assert o.stanje == g.UNUTRA


if __name__ == "__main__":
    for ime, f in list(globals().items()):
        if ime.startswith("test_"):
            f()
            print("OK  " + ime)
    print("Svi testovi prosli.")
