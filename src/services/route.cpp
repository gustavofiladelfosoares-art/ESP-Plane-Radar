#include "services/route.h"

#include <Arduino.h>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "services/adsb_client.h"
#include "services/http_util.h"
#include "services/radar_location.h"
#include "services/shared.h"
#include "ui/radar_range.h"

namespace services::route {

namespace {

// Small LRU cache of looked-up callsigns (known routes and known misses), so
// each flight is asked about once while it is around.
struct Entry {
  char callsign[9];
  bool known;
  char airline[32];
  char from_iata[4];
  char to_iata[4];
  char from_city[28];
  char to_city[28];
  uint32_t used;
};

constexpr int kEntries = 12;
constexpr unsigned long kMinGapMs = 1500;  // be gentle with adsbdb

Entry s_cache[kEntries] = {};
uint32_t s_tick = 0;
char s_wanted[9] = "";
unsigned long s_last_lookup = 0;

/** Airline flights look like "TAM3456" / "AZU4521A": 3 letters then a digit. */
bool looksLikeAirline(const char* cs) {
  return strlen(cs) >= 4 && isalpha(cs[0]) && isalpha(cs[1]) && isalpha(cs[2]) &&
         isdigit(cs[3]);
}

void copy(char* dst, size_t len, const char* src) {
  snprintf(dst, len, "%s", src != nullptr ? src : "");
}

/** Caller holds SharedLock. */
Entry* find(const char* cs) {
  for (Entry& e : s_cache) {
    if (e.callsign[0] != '\0' && strcmp(e.callsign, cs) == 0) {
      e.used = ++s_tick;
      return &e;
    }
  }
  return nullptr;
}

/** Caller holds SharedLock. */
Entry* oldest() {
  Entry* best = &s_cache[0];
  for (Entry& e : s_cache) {
    if (e.callsign[0] == '\0') return &e;
    if (e.used < best->used) best = &e;
  }
  return best;
}

/** HTTP lookup (no lock held). Returns false on network trouble (not cached). */
bool lookup(const char* cs) {
  char url[96];
  snprintf(url, sizeof(url), "https://api.adsbdb.com/v0/callsign/%s", cs);
  JsonDocument filter;
  JsonObject f = filter["response"]["flightroute"].to<JsonObject>();
  f["airline"]["name"] = true;
  for (const char* end : {"origin", "destination"}) {
    f[end]["iata_code"] = true;
    f[end]["municipality"] = true;
  }
  JsonDocument doc;
  s_last_lookup = millis();
  if (!http::getJson(url, "route", doc, &filter)) {
    return false;
  }
  JsonObject r = doc["response"]["flightroute"];
  Entry e{};
  copy(e.callsign, sizeof(e.callsign), cs);
  e.known = !r.isNull();
  if (e.known) {
    copy(e.airline, sizeof(e.airline), r["airline"]["name"]);
    copy(e.from_iata, sizeof(e.from_iata), r["origin"]["iata_code"]);
    copy(e.to_iata, sizeof(e.to_iata), r["destination"]["iata_code"]);
    copy(e.from_city, sizeof(e.from_city), r["origin"]["municipality"]);
    copy(e.to_city, sizeof(e.to_city), r["destination"]["municipality"]);
    Serial.printf("route: %s %s -> %s\n", cs, e.from_iata, e.to_iata);
  } else {
    Serial.printf("route: %s unknown\n", cs);
  }
  SharedLock lock;
  Entry* slot = find(cs);
  if (slot == nullptr) slot = oldest();
  *slot = e;
  slot->used = ++s_tick;
  return true;
}

/** Closest airline callsign on the radar without a cached route (or ""). */
void pickRadarCandidate(char* out, size_t len) {
  out[0] = '\0';
  const double lat = location::lat();
  const double lon = location::lon();
  const float cos_lat = cosf(static_cast<float>(lat) * 0.01745329f);
  const float max_km = ui::radar::fetchRadiusKm();
  float best = 1e9f;
  SharedLock lock;
  const adsb::Aircraft* planes = adsb::aircraftList();
  for (size_t i = 0; i < adsb::aircraftCount(); ++i) {
    const adsb::Aircraft& p = planes[i];
    if (!looksLikeAirline(p.callsign) || find(p.callsign) != nullptr) continue;
    const float dx = static_cast<float>(p.lon - lon) * 111.0f * cos_lat;
    const float dy = static_cast<float>(p.lat - lat) * 111.0f;
    const float d = sqrtf(dx * dx + dy * dy);
    if (d <= max_km && d < best) {
      best = d;
      copy(out, len, p.callsign);
    }
  }
}

}  // namespace

void request(const char* callsign) {
  SharedLock lock;
  copy(s_wanted, sizeof(s_wanted), callsign);
}

void service(bool radar_routes) {
  if (millis() - s_last_lookup < kMinGapMs) {
    return;
  }
  char cs[9];
  {
    SharedLock lock;
    copy(cs, sizeof(cs), s_wanted);
    if (!looksLikeAirline(cs) || find(cs) != nullptr) cs[0] = '\0';
  }
  if (cs[0] == '\0' && radar_routes) {
    pickRadarCandidate(cs, sizeof(cs));
  }
  if (cs[0] != '\0') {
    lookup(cs);
  }
}

void snapshot(ui::RouteModel* out) {
  SharedLock lock;
  *out = ui::RouteModel{};
  const Entry* e = s_wanted[0] != '\0' ? find(s_wanted) : nullptr;
  if (e == nullptr || !e->known) return;
  out->valid = true;
  copy(out->callsign, sizeof(out->callsign), e->callsign);
  copy(out->airline, sizeof(out->airline), e->airline);
  copy(out->from_iata, sizeof(out->from_iata), e->from_iata);
  copy(out->to_iata, sizeof(out->to_iata), e->to_iata);
  copy(out->from_city, sizeof(out->from_city), e->from_city);
  copy(out->to_city, sizeof(out->to_city), e->to_city);
}

size_t copyCodes(ui::RouteCodes* out, size_t max) {
  SharedLock lock;
  size_t n = 0;
  for (const Entry& e : s_cache) {
    if (n >= max) break;
    if (e.callsign[0] == '\0' || !e.known || e.from_iata[0] == '\0' || e.to_iata[0] == '\0') continue;
    copy(out[n].callsign, sizeof(out[n].callsign), e.callsign);
    copy(out[n].from, sizeof(out[n].from), e.from_iata);
    copy(out[n].to, sizeof(out[n].to), e.to_iata);
    ++n;
  }
  return n;
}

}  // namespace services::route
