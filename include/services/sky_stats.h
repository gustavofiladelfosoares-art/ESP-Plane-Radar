#pragma once

#include <cstddef>

#include "services/adsb_client.h"
#include "ui/model.h"

namespace services::stats {

/**
 * Count the aircraft of a fresh ADS-B list into today's summary (distinct
 * aircraft, highest, fastest, closest pass, most seen airline). Starts over
 * when the local date changes. Call with SharedLock held.
 */
void record(const adsb::Aircraft* list, size_t n, double home_lat, double home_lon, int yday);

/** Copy of today's summary (call with SharedLock held). */
void snapshot(ui::SummaryModel* out);

}  // namespace services::stats
