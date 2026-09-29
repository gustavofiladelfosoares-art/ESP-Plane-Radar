#pragma once

#include <cstdint>

namespace services::map {

/** Find the flash partition that stores built maps (one slot per range). */
void init();

/**
 * Network task: make sure the map for (lat, lon, range) is stored in flash,
 * building it from tiles if needed, then the other ranges. Returns true when
 * it did any work (so the caller can pace itself).
 */
bool service(double lat, double lon, uint8_t active_range);

/**
 * UI task: the map for the active range, or nullptr when not ready. Always
 * pair with unlock() (even on nullptr); the pointer is valid until then.
 */
const uint8_t* lockCurrent(double lat, double lon, uint8_t range);
void unlock();

}  // namespace services::map
