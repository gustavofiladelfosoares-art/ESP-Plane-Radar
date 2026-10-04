#include "services/sky_stats.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace services::stats {

namespace {

constexpr size_t kMaxSeen = 512;     // distinct aircraft remembered per day
constexpr size_t kMaxAirlines = 32;  // airline counters per day

ui::SummaryModel s_sum;
uint32_t s_seen[kMaxSeen];
size_t s_seen_n = 0;
int s_yday = -1;

struct AirlineCount {
  char code[4];
  int count;
};
AirlineCount s_airlines[kMaxAirlines];
size_t s_airlines_n = 0;

/** ICAO airline codes seen around Brazil, with a friendly name. */
struct AirlineName {
  const char* code;
  const char* name;
};
constexpr AirlineName kNames[] = {
    {"AZU", "Azul"},       {"GLO", "GOL"},          {"TAM", "LATAM"},     {"LAN", "LATAM"},
    {"LPE", "LATAM"},      {"PTB", "VOEPASS"},      {"TTL", "Total"},     {"ARG", "Aerolíneas"},
    {"CMP", "Copa"},       {"AVA", "Avianca"},      {"TAP", "TAP"},       {"AAL", "American"},
    {"UAL", "United"},     {"DAL", "Delta"},        {"AFR", "Air France"}, {"KLM", "KLM"},
    {"DLH", "Lufthansa"},  {"IBE", "Iberia"},       {"BAW", "British"},   {"UAE", "Emirates"},
    {"QTR", "Qatar"},      {"ACA", "Air Canada"},   {"SKU", "Sky"},       {"JAT", "JetSMART"},
    {"FDX", "FedEx"},      {"UPS", "UPS"},          {"MWM", "Modern"},    {"ETH", "Ethiopian"},
    {"THY", "Turkish"},    {"ITY", "ITA"},          {"AEA", "Air Europa"}, {"BOV", "BoA"},
};

uint32_t hashCallsign(const char* s) {
  uint32_t h = 2166136261u;  // FNV-1a
  for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
  return h ? h : 1;
}

/** "AZU4521" -> "AZU"; registrations and hex codes have no airline. */
bool airlineCode(const char* cs, char out[4]) {
  if (strlen(cs) < 4) return false;
  for (int i = 0; i < 3; ++i) {
    if (!isupper(static_cast<unsigned char>(cs[i]))) return false;
  }
  if (!isdigit(static_cast<unsigned char>(cs[3]))) return false;
  memcpy(out, cs, 3);
  out[3] = '\0';
  return true;
}

const char* airlineName(const char* code) {
  for (const auto& a : kNames) {
    if (strcmp(a.code, code) == 0) return a.name;
  }
  return code;
}

void reset(int yday) {
  s_sum = ui::SummaryModel{};
  s_seen_n = 0;
  s_airlines_n = 0;
  s_yday = yday;
}

void countAirline(const char* cs) {
  char code[4];
  if (!airlineCode(cs, code)) return;
  AirlineCount* slot = nullptr;
  for (size_t i = 0; i < s_airlines_n; ++i) {
    if (strcmp(s_airlines[i].code, code) == 0) slot = &s_airlines[i];
  }
  if (slot == nullptr) {
    if (s_airlines_n >= kMaxAirlines) return;
    slot = &s_airlines[s_airlines_n++];
    memcpy(slot->code, code, 4);
    slot->count = 0;
  }
  ++slot->count;
  if (slot->count > s_sum.airline_count) {
    s_sum.airline_count = slot->count;
    snprintf(s_sum.airline, sizeof(s_sum.airline), "%s", airlineName(code));
  }
}

}  // namespace

void record(const adsb::Aircraft* list, size_t n, double home_lat, double home_lon, int yday) {
  if (yday != s_yday) reset(yday);
  const float cos_lat = cosf(static_cast<float>(home_lat) * 0.01745329f);
  for (size_t i = 0; i < n; ++i) {
    const adsb::Aircraft& p = list[i];
    if (p.on_ground || p.callsign[0] == '\0') continue;

    const uint32_t h = hashCallsign(p.callsign);
    bool known = false;
    for (size_t k = 0; k < s_seen_n && !known; ++k) known = s_seen[k] == h;
    if (!known) {
      if (s_seen_n < kMaxSeen) s_seen[s_seen_n++] = h;
      ++s_sum.seen;
      countAirline(p.callsign);
    }

    if (p.has_alt && p.alt_ft > s_sum.highest_ft) {
      s_sum.highest_ft = p.alt_ft;
      snprintf(s_sum.highest_cs, sizeof(s_sum.highest_cs), "%s", p.callsign);
    }
    const int kmh = static_cast<int>(lroundf(p.gs_knots * 1.852f));
    if (kmh > s_sum.fastest_kmh && kmh < 1300) {  // ignore glitches
      s_sum.fastest_kmh = kmh;
      snprintf(s_sum.fastest_cs, sizeof(s_sum.fastest_cs), "%s", p.callsign);
    }
    const float dx = static_cast<float>(p.lon - home_lon) * 111.0f * cos_lat;
    const float dy = static_cast<float>(p.lat - home_lat) * 111.0f;
    const float d = sqrtf(dx * dx + dy * dy);
    if (s_sum.closest_cs[0] == '\0' || d < s_sum.closest_km) {
      s_sum.closest_km = d;
      snprintf(s_sum.closest_cs, sizeof(s_sum.closest_cs), "%s", p.callsign);
    }
  }
}

void snapshot(ui::SummaryModel* out) { *out = s_sum; }

}  // namespace services::stats
