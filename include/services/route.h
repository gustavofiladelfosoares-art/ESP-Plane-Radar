#pragma once

#include <cstddef>

#include "ui/model.h"

namespace services::route {

/** Ask for the route of callsign (nearest-aircraft page; looked up first). */
void request(const char* callsign);

/**
 * Network task: look up one route if one is pending — the requested callsign
 * first, then (when radar_routes) the closest radar aircraft without one.
 */
void service(bool radar_routes);

/** Route of the requested callsign, if known (thread-safe). */
void snapshot(ui::RouteModel* out);

/** Copy every known route's airport codes (thread-safe); returns the count. */
size_t copyCodes(ui::RouteCodes* out, size_t max);

}  // namespace services::route
