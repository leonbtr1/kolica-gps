// geofence.h — logika "geo-ograde" za kolica.
// Ne ovisi o Arduinu, pa se može testirati i na računalu (vidi test/).
#pragma once

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace geofence {

constexpr double EARTH_RADIUS_M = 6371000.0;

inline double toRad(double deg) { return deg * M_PI / 180.0; }

// Udaljenost između dvije GPS točke u metrima (Haversine formula).
inline double distanceMeters(double lat1, double lon1, double lat2, double lon2) {
  double dLat = toRad(lat2 - lat1);
  double dLon = toRad(lon2 - lon1);
  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(toRad(lat1)) * cos(toRad(lat2)) * sin(dLon / 2) * sin(dLon / 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return EARTH_RADIUS_M * c;
}

enum class Status { UNKNOWN, INSIDE, OUTSIDE };
enum class Event { NONE, EXITED, RETURNED };

struct Config {
  double radiusM;      // dopušteni radijus oko centra
  double hysteresisM;  // povratak se priznaje tek unutar (radiusM - hysteresisM)
  int confirmCount;    // koliko uzastopnih očitanja treba za promjenu stanja
};

class Monitor {
 public:
  explicit Monitor(const Config& cfg) : cfg_(cfg) {}

  void setCenter(double lat, double lon) {
    centerLat_ = lat;
    centerLon_ = lon;
    hasCenter_ = true;
    reset();
  }

  void reset() {
    status_ = Status::UNKNOWN;
    pending_ = Status::UNKNOWN;
    pendingCount_ = 0;
  }

  // Poziva se za svako ispravno GPS očitanje.
  // Vraća EXITED kad kolica (potvrđeno) izađu iz zone, RETURNED kad se vrate.
  Event update(double lat, double lon) {
    if (!hasCenter_) return Event::NONE;

    lastDistance_ = distanceMeters(centerLat_, centerLon_, lat, lon);

    // Histereza: da bi se izašlo treba prijeći radijus, a da bi se vratilo
    // treba ući malo dublje u zonu. Tako alarm ne "treperi" na samoj granici.
    Status candidate;
    if (status_ == Status::OUTSIDE) {
      candidate = (lastDistance_ <= cfg_.radiusM - cfg_.hysteresisM) ? Status::INSIDE
                                                                     : Status::OUTSIDE;
    } else {
      candidate = (lastDistance_ > cfg_.radiusM) ? Status::OUTSIDE : Status::INSIDE;
    }

    if (candidate == status_) {
      pendingCount_ = 0;
      return Event::NONE;
    }

    // Potvrda kroz više uzastopnih očitanja filtrira pojedinačne GPS "skokove".
    if (candidate != pending_) {
      pending_ = candidate;
      pendingCount_ = 0;
    }
    if (++pendingCount_ < cfg_.confirmCount) return Event::NONE;

    Status previous = status_;
    status_ = candidate;
    pending_ = Status::UNKNOWN;
    pendingCount_ = 0;

    if (status_ == Status::OUTSIDE) return Event::EXITED;
    if (previous == Status::OUTSIDE) return Event::RETURNED;
    return Event::NONE;  // prvo potvrđeno stanje je "unutra" — nema događaja
  }

  bool hasCenter() const { return hasCenter_; }
  Status status() const { return status_; }
  double lastDistance() const { return lastDistance_; }
  double centerLat() const { return centerLat_; }
  double centerLon() const { return centerLon_; }

 private:
  Config cfg_;
  double centerLat_ = 0, centerLon_ = 0;
  bool hasCenter_ = false;
  Status status_ = Status::UNKNOWN;
  Status pending_ = Status::UNKNOWN;
  int pendingCount_ = 0;
  double lastDistance_ = 0;
};

}  // namespace geofence
