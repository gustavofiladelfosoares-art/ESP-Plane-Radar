#pragma once

#include <cstddef>
#include <cstdint>

namespace services::adsb {

struct Aircraft {
  float lat;
  float lon;
  float nose_deg;
  float track_deg;
  float gs_knots;
  char callsign[9];
  char type[5];
  char alt[12];
  /** Barometric (or geometric) altitude in feet; valid only when has_alt. */
  int32_t alt_ft;
  bool has_alt;
  bool on_ground;
  /** Model description, e.g. "AIRBUS A-321neo". */
  char desc[26];
  /** Registration, e.g. "PR-XMG". */
  char reg[10];
  /** Transponder code (0 = unknown), e.g. 7700. */
  uint16_t squawk;
  /** kFlagEmergency / kFlagMilitary. */
  uint8_t flags;
};

constexpr uint8_t kFlagEmergency = 1;  // squawk 7500/7600/7700 or declared emergency
constexpr uint8_t kFlagMilitary = 2;   // adsb.fi dbFlags bit 0

constexpr size_t kMaxAircraft = 48;

size_t aircraftCount();
const Aircraft* aircraftList();

/** Latest list; read it while holding services::SharedLock (the network task
 *  swaps in new data under that lock). */

/** Hook invoked during long HTTP I/O (e.g. wifiLoop). Optional. */
using PollFn = void (*)();
void setPollFn(PollFn fn);

/** Fetch aircraft within fetch_radius_km of center_lat/lon from adsb.fi. */
bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km);

}  // namespace services::adsb
