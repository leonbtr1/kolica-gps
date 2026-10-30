// Test logike geo-ograde na računalu (bez Arduina):
//   g++ -std=c++17 -I../kolica_geofence test_geofence.cpp -o test && ./test
#include <cstdio>
#include <cmath>
#include "geofence.h"

using namespace geofence;

static int greske = 0;

#define PROVJERI(uvjet)                                              \
  do {                                                               \
    if (!(uvjet)) {                                                  \
      std::printf("NEUSPJEH (linija %d): %s\n", __LINE__, #uvjet);   \
      greske++;                                                      \
    }                                                                \
  } while (0)

const double C_LAT = 45.815011, C_LON = 15.981919;  // Zagreb
const double M_PO_STUPNJU_LAT = EARTH_RADIUS_M * M_PI / 180.0;

// Točka udaljena d metara sjeverno od centra.
static double latNa(double d) { return C_LAT + d / M_PO_STUPNJU_LAT; }

static Event ponovi(Monitor& m, double d, int puta) {
  Event zadnji = Event::NONE;
  for (int i = 0; i < puta; i++) {
    Event e = m.update(latNa(d), C_LON);
    if (e != Event::NONE) zadnji = e;
  }
  return zadnji;
}

int main() {
  // --- Udaljenost ---
  PROVJERI(std::fabs(distanceMeters(0, 0, 0, 1) - 111194.9) < 1.0);
  PROVJERI(std::fabs(distanceMeters(C_LAT, C_LON, latNa(100), C_LON) - 100.0) < 0.01);
  // Zagreb – Split, zračna linija ~260 km
  double zgSt = distanceMeters(45.815011, 15.981919, 43.508133, 16.440193);
  PROVJERI(zgSt > 255000 && zgSt < 265000);

  Config cfg{100.0, 10.0, 5};

  // --- Bez centra nema događaja ---
  {
    Monitor m(cfg);
    PROVJERI(m.update(latNa(500), C_LON) == Event::NONE);
    PROVJERI(m.status() == Status::UNKNOWN);
  }

  // --- Normalan tijek: unutra -> izlazak -> povratak ---
  {
    Monitor m(cfg);
    m.setCenter(C_LAT, C_LON);

    PROVJERI(ponovi(m, 20, 5) == Event::NONE);
    PROVJERI(m.status() == Status::INSIDE);

    // Pojedinačni GPS "skokovi" van zone (4 < 5 potvrda) ne pale alarm
    PROVJERI(ponovi(m, 150, 4) == Event::NONE);
    PROVJERI(ponovi(m, 20, 1) == Event::NONE);
    PROVJERI(m.status() == Status::INSIDE);

    // Pravi izlazak: alarm točno na 5. uzastopnom očitanju
    PROVJERI(ponovi(m, 120, 4) == Event::NONE);
    PROVJERI(m.update(latNa(120), C_LON) == Event::EXITED);
    PROVJERI(m.status() == Status::OUTSIDE);
    PROVJERI(std::fabs(m.lastDistance() - 120) < 0.1);

    // Dalje vani — nema ponovnog EXITED
    PROVJERI(ponovi(m, 300, 10) == Event::NONE);

    // U pojasu histereze (95 m > 100-10) još se ne računa kao povratak
    PROVJERI(ponovi(m, 95, 20) == Event::NONE);
    PROVJERI(m.status() == Status::OUTSIDE);

    // Stvarni povratak
    PROVJERI(ponovi(m, 50, 4) == Event::NONE);
    PROVJERI(m.update(latNa(50), C_LON) == Event::RETURNED);
    PROVJERI(m.status() == Status::INSIDE);
  }

  // --- Kolica se uključe već izvan zone ---
  {
    Monitor m(cfg);
    m.setCenter(C_LAT, C_LON);
    PROVJERI(ponovi(m, 400, 5) == Event::EXITED);
    PROVJERI(m.status() == Status::OUTSIDE);
  }

  // --- Novi centar resetira stanje ---
  {
    Monitor m(cfg);
    m.setCenter(C_LAT, C_LON);
    ponovi(m, 400, 5);
    m.setCenter(latNa(400), C_LON);
    PROVJERI(m.status() == Status::UNKNOWN);
    PROVJERI(ponovi(m, 400, 5) == Event::NONE);
    PROVJERI(m.status() == Status::INSIDE);
  }

  if (greske == 0) {
    std::printf("Svi testovi prosli.\n");
    return 0;
  }
  std::printf("%d test(ova) palo.\n", greske);
  return 1;
}
