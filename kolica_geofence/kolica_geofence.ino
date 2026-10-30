// =====================================================================
//  Električna kolica — GPS geo-ograda
//  Ploča: ESP32 | GPS: NEO-6M/7M/8M (UART) | opcionalno: SIM800L za SMS
//
//  - Pamti "centar" (početnu lokaciju) u flash memoriji.
//  - Kad kolica izađu iz zadanog radijusa: zujalica + poruka (Serial/SMS).
//  - Tipka: dugi pritisak (3 s) = trenutna lokacija postaje novi centar,
//           kratki pritisak     = utišaj zujalicu.
//
//  Potrebna biblioteka: TinyGPSPlus (Mikal Hart) — Library Manager.
// =====================================================================

#include <TinyGPSPlus.h>
#include <Preferences.h>
#include "geofence.h"

// ============================ POSTAVKE ===============================
const double RADIJUS_M      = 100.0;  // dopušteni radijus oko centra (m)
const double HISTEREZA_M    = 10.0;   // povratak se priznaje tek na RADIJUS_M - HISTEREZA_M
const int    BROJ_POTVRDA   = 5;      // uzastopnih očitanja izvan zone prije alarma (~5 s)

const int           MIN_SATELITA   = 5;     // manje satelita = nepouzdana lokacija
const double        MAX_HDOP       = 2.5;   // veći HDOP = lošija preciznost
const unsigned long GPS_TIMEOUT_MS = 30000; // bez ispravnog signala ovoliko = upozorenje

// Dok su kolica izvan zone, ponavljaj obavijest s novom lokacijom (0 = isključeno)
const unsigned long PONAVLJANJE_OBAVIJESTI_MS = 5UL * 60UL * 1000UL;

// Ako centar još nije spremljen, prvi ispravan GPS fix postaje centar.
// Želiš li fiksnu lokaciju, upiši je ovdje i stavi KORISTI_FIKSNI_CENTAR na true.
const bool   KORISTI_FIKSNI_CENTAR = false;
const double FIKSNI_CENTAR_LAT     = 45.815011;  // primjer: Zagreb
const double FIKSNI_CENTAR_LON     = 15.981919;

// SMS preko SIM800L modula (1 = uključeno, 0 = samo Serial ispis)
#define KORISTI_GSM 0
const char* BROJ_TELEFONA = "+385911234567";

// ============================== PINOVI ===============================
const int GPS_RX_PIN = 16;  // spoji na TX pin GPS modula
const int GPS_TX_PIN = 17;  // spoji na RX pin GPS modula
const int GSM_RX_PIN = 26;  // spoji na TX pin SIM800L
const int GSM_TX_PIN = 27;  // spoji na RX pin SIM800L
const int ZUJALICA_PIN = 25;  // aktivna zujalica (preko tranzistora)
const int LED_PIN      = 2;   // ugrađena LED na većini ESP32 ploča
const int TIPKA_PIN    = 0;   // BOOT tipka na ESP32 ploči (ili vanjska tipka na GND)
// =====================================================================

TinyGPSPlus gps;
Preferences prefs;
geofence::Monitor ograda({RADIJUS_M, HISTEREZA_M, BROJ_POTVRDA});

#if KORISTI_GSM
HardwareSerial& gsm = Serial1;
#endif

bool alarmAktivan = false;
bool alarmUtisan = false;
bool gpsIzgubljen = false;
bool imaZadnjuLokaciju = false;
double zadnjiLat = 0, zadnjiLon = 0;
unsigned long zadnjiIspravanFixMs = 0;
unsigned long zadnjaObavijestMs = 0;

// ---------------------------------------------------------------------

String linkNaKartu(double lat, double lon) {
  return "https://maps.google.com/?q=" + String(lat, 6) + "," + String(lon, 6);
}

void posaljiSMS(const String& poruka) {
#if KORISTI_GSM
  gsm.println("AT+CMGF=1");  // tekstualni način rada
  delay(300);
  gsm.print("AT+CMGS=\"");
  gsm.print(BROJ_TELEFONA);
  gsm.println("\"");
  delay(300);
  gsm.print(poruka);
  delay(100);
  gsm.write(26);  // Ctrl+Z = pošalji
#else
  (void)poruka;
#endif
}

// Poruke su bez dijakritika jer ih SMS (GSM 7-bit) ne prikazuje ispravno.
void obavijesti(const String& poruka) {
  Serial.println(">>> " + poruka);
  posaljiSMS(poruka);
}

void spremiCentar(double lat, double lon) {
  prefs.putDouble("lat", lat);
  prefs.putDouble("lon", lon);
  prefs.putBool("ima", true);
  ograda.setCenter(lat, lon);
  alarmAktivan = false;
  Serial.printf("Novi centar spremljen: %.6f, %.6f (radijus %.0f m)\n", lat, lon, RADIJUS_M);
}

bool ispravanFix() {
  return gps.location.isValid() &&
         gps.location.age() < 2000 &&
         gps.satellites.isValid() && (int)gps.satellites.value() >= MIN_SATELITA &&
         gps.hdop.isValid() && gps.hdop.hdop() <= MAX_HDOP;
}

// Kratki pritisak = utišaj alarm, dugi (3 s) = postavi centar na trenutnu lokaciju.
void provjeriTipku() {
  static bool bioPritisnut = false;
  static bool dugiObraden = false;
  static unsigned long pocetakPritiska = 0;

  bool pritisnut = digitalRead(TIPKA_PIN) == LOW;
  unsigned long sada = millis();

  if (pritisnut && !bioPritisnut) {
    pocetakPritiska = sada;
    dugiObraden = false;
  }

  if (pritisnut && !dugiObraden && sada - pocetakPritiska >= 3000) {
    dugiObraden = true;
    if (imaZadnjuLokaciju) {
      spremiCentar(zadnjiLat, zadnjiLon);
    } else {
      Serial.println("Nema ispravne GPS lokacije — centar nije promijenjen.");
    }
  }

  if (!pritisnut && bioPritisnut && !dugiObraden && sada - pocetakPritiska > 50) {
    if (alarmAktivan) {
      alarmUtisan = true;
      Serial.println("Zujalica utisana.");
    }
  }

  bioPritisnut = pritisnut;
}

// Zujalica: 200 ms uključeno / 800 ms isključeno dok je alarm aktivan.
void azurirajZujalicu() {
  bool zuji = alarmAktivan && !alarmUtisan && (millis() % 1000) < 200;
  digitalWrite(ZUJALICA_PIN, zuji ? HIGH : LOW);
}

// LED: sporo treptanje = čeka GPS/centar, stalno upaljena = unutar zone,
//      brzo treptanje = izvan zone.
void azurirajLed() {
  bool upaljena;
  if (ograda.status() == geofence::Status::OUTSIDE) {
    upaljena = (millis() % 250) < 125;
  } else if (ograda.status() == geofence::Status::INSIDE && !gpsIzgubljen) {
    upaljena = true;
  } else {
    upaljena = (millis() % 2000) < 100;
  }
  digitalWrite(LED_PIN, upaljena ? HIGH : LOW);
}

const char* nazivStanja(geofence::Status s) {
  switch (s) {
    case geofence::Status::INSIDE:  return "UNUTRA";
    case geofence::Status::OUTSIDE: return "IZVAN ZONE";
    default:                        return "NEPOZNATO";
  }
}

void obradiLokaciju(double lat, double lon) {
  zadnjiLat = lat;
  zadnjiLon = lon;
  imaZadnjuLokaciju = true;

  if (!ograda.hasCenter()) {
    spremiCentar(lat, lon);
    return;
  }

  geofence::Event dogadaj = ograda.update(lat, lon);

  Serial.printf("Lat %.6f Lon %.6f | udaljenost %.1f m | sat %d | HDOP %.1f | %s\n",
                lat, lon, ograda.lastDistance(), (int)gps.satellites.value(),
                gps.hdop.hdop(), nazivStanja(ograda.status()));

  switch (dogadaj) {
    case geofence::Event::EXITED:
      alarmAktivan = true;
      alarmUtisan = false;
      zadnjaObavijestMs = millis();
      obavijesti("UPOZORENJE: kolica su izasla iz zone! Udaljenost " +
                 String(ograda.lastDistance(), 0) + " m. Lokacija: " + linkNaKartu(lat, lon));
      break;

    case geofence::Event::RETURNED:
      alarmAktivan = false;
      obavijesti("Kolica su se vratila u zonu.");
      break;

    case geofence::Event::NONE:
      if (alarmAktivan && PONAVLJANJE_OBAVIJESTI_MS > 0 &&
          millis() - zadnjaObavijestMs >= PONAVLJANJE_OBAVIJESTI_MS) {
        zadnjaObavijestMs = millis();
        obavijesti("Kolica su i dalje izvan zone (" + String(ograda.lastDistance(), 0) +
                   " m). Lokacija: " + linkNaKartu(lat, lon));
      }
      break;
  }
}

// ---------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  Serial2.setRxBufferSize(1024);  // da se NMEA podaci ne izgube dok se šalje SMS
  Serial2.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

#if KORISTI_GSM
  gsm.begin(9600, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
#endif

  pinMode(ZUJALICA_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(TIPKA_PIN, INPUT_PULLUP);

  prefs.begin("kolica", false);

  if (KORISTI_FIKSNI_CENTAR) {
    ograda.setCenter(FIKSNI_CENTAR_LAT, FIKSNI_CENTAR_LON);
  } else if (prefs.getBool("ima", false)) {
    ograda.setCenter(prefs.getDouble("lat", 0), prefs.getDouble("lon", 0));
  }

  if (ograda.hasCenter()) {
    Serial.printf("Centar: %.6f, %.6f | radijus %.0f m\n",
                  ograda.centerLat(), ograda.centerLon(), RADIJUS_M);
  } else {
    Serial.println("Centar nije postavljen — bit ce postavljen na prvi ispravan GPS fix.");
  }

  zadnjiIspravanFixMs = millis();
}

void loop() {
  while (Serial2.available()) {
    gps.encode(Serial2.read());
  }

  provjeriTipku();
  azurirajZujalicu();
  azurirajLed();

  if (gps.location.isUpdated()) {
    if (ispravanFix()) {
      zadnjiIspravanFixMs = millis();
      if (gpsIzgubljen) {
        gpsIzgubljen = false;
        Serial.println("GPS signal ponovno ispravan.");
      }
      obradiLokaciju(gps.location.lat(), gps.location.lng());
    } else {
      gps.location.lat();  // označi očitanje obrađenim (resetira isUpdated)
    }
  }

  if (!gpsIzgubljen && millis() - zadnjiIspravanFixMs > GPS_TIMEOUT_MS) {
    gpsIzgubljen = true;
    Serial.println("Upozorenje: nema ispravnog GPS signala (zatvoren prostor ili kvar antene?).");
  }
}
