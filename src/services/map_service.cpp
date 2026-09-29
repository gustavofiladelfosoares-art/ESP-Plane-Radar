#include "services/map_service.h"

#include <Arduino.h>
#include <esp_partition.h>
#include <esp_spi_flash.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "services/http_util.h"
#include "ui/radar_map.h"
#include "ui/radar_range.h"

namespace services::map {

namespace {

namespace rm = ui::radar_map;

// Maps live in the "spiffs" data partition as raw slots (one per range) and
// are read straight from flash through the MMU, so they cost no RAM.
constexpr size_t kSlotSize = 16 * 1024;
constexpr size_t kSlots = ui::radar::kRangePresetCount;

struct Header {
  char magic[4];
  uint8_t version;
  uint8_t range;
  uint8_t pad[2];
  float lat;
  float lon;
};
static_assert(sizeof(Header) + rm::kBytes <= kSlotSize, "map slot too small");

const esp_partition_t* s_part = nullptr;
const uint8_t* s_mapped = nullptr;
spi_flash_mmap_handle_t s_handle = 0;
SemaphoreHandle_t s_lock = nullptr;

// Ranges already handled for the current location (built, loaded or failed).
uint8_t s_done_mask = 0;
float s_done_lat = NAN;
float s_done_lon = NAN;
unsigned long s_retry_after = 0;

bool sameSpot(float a_lat, float a_lon, double lat, double lon) {
  return fabs(a_lat - lat) < 1e-4 && fabs(a_lon - lon) < 1e-4;
}

void remap() {
  if (s_handle != 0) {
    spi_flash_munmap(s_handle);
    s_handle = 0;
    s_mapped = nullptr;
  }
  const void* ptr = nullptr;
  if (esp_partition_mmap(s_part, 0, kSlotSize * kSlots, SPI_FLASH_MMAP_DATA, &ptr, &s_handle) ==
      ESP_OK) {
    s_mapped = static_cast<const uint8_t*>(ptr);
  }
}

const Header* slot(uint8_t range) {
  return s_mapped == nullptr ? nullptr
                             : reinterpret_cast<const Header*>(s_mapped + range * kSlotSize);
}

bool slotValid(uint8_t range, double lat, double lon) {
  const Header* h = slot(range);
  return h != nullptr && memcmp(h->magic, "RMAP", 4) == 0 && h->version == rm::kFormatVersion &&
         h->range == range && sameSpot(h->lat, h->lon, lat, lon);
}

bool writeSlot(uint8_t range, double lat, double lon, const uint8_t* map) {
  Header h{{'R', 'M', 'A', 'P'}, rm::kFormatVersion, range, {0, 0},
           static_cast<float>(lat), static_cast<float>(lon)};
  const size_t off = range * kSlotSize;
  // The UI may be drawing from the mapping; take the lock before touching it.
  xSemaphoreTake(s_lock, portMAX_DELAY);
  bool ok = esp_partition_erase_range(s_part, off, kSlotSize) == ESP_OK &&
            esp_partition_write(s_part, off + sizeof(h), map, rm::kBytes) == ESP_OK &&
            esp_partition_write(s_part, off, &h, sizeof(h)) == ESP_OK;  // header last
  remap();
  xSemaphoreGive(s_lock);
  return ok;
}

/**
 * Download the tiles for one range one at a time (centre tile first, which
 * also calibrates the classes), paint them into a map and store it.
 */
bool build(double lat, double lon, uint8_t range) {
  const float outer_km = ui::radar::kRangePresets[range].outer_km;
  const rm::TilePlan p = rm::plan(lat, lon, outer_km);
  const int nx = p.tx1 - p.tx0 + 1;
  const int ny = p.ty1 - p.ty0 + 1;
  if (nx * ny > 16) {
    return false;
  }
  int cx = 0;
  int cy = 0;
  rm::centerTile(p, &cx, &cy);

  auto* map = static_cast<uint8_t*>(malloc(rm::kBytes));
  if (map == nullptr) {
    return false;
  }
  memset(map, 0, rm::kBytes);
  rm::Thresholds th;
  bool ok = true;
  for (int k = -1; k < nx * ny && ok; ++k) {
    // k = -1 is the centre tile; then every other tile in the grid.
    const int tx = k < 0 ? cx : p.tx0 + k % nx;
    const int ty = k < 0 ? cy : p.ty0 + k / nx;
    if (k >= 0 && tx == cx && ty == cy) continue;
    char url[200];
    snprintf(url, sizeof(url), rm::kTileUrlFormat, p.z, ty, tx);
    uint8_t* jpg = nullptr;
    size_t len = 0;
    ok = http::getBytes(url, "map", &jpg, &len, 48 * 1024);
    if (ok && k < 0) {
      rm::Histogram hist;
      ok = rm::accumulate(jpg, len, &hist);
      th = rm::calibrate(hist);
    }
    if (ok) {
      ok = rm::paintTile(p, tx, ty, jpg, len, th, map);
    }
    free(jpg);
  }
  if (ok) {
    rm::despeckle(map);
    ok = writeSlot(range, lat, lon, map);
  }
  free(map);
  Serial.printf("map: range %u zoom %d, %d tiles %s\n", range, p.z, nx * ny, ok ? "ok" : "FAILED");
  return ok;
}

}  // namespace

void init() {
  s_lock = xSemaphoreCreateMutex();
  s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
                                    "spiffs");
  if (s_part == nullptr || s_part->size < kSlotSize * kSlots) {
    Serial.println("map: no storage partition — radar map disabled");
    s_part = nullptr;
    return;
  }
  remap();
}

bool service(double lat, double lon, uint8_t active_range) {
  if (s_part == nullptr || millis() < s_retry_after) {
    return false;
  }
  if (!sameSpot(s_done_lat, s_done_lon, lat, lon)) {
    s_done_mask = 0;
    s_done_lat = static_cast<float>(lat);
    s_done_lon = static_cast<float>(lon);
  }
  // Active range first, then the others in the background.
  for (uint8_t k = 0; k <= kSlots; ++k) {
    const uint8_t r = k == 0 ? active_range : static_cast<uint8_t>(k - 1);
    if (s_done_mask & (1 << r)) continue;
    if (slotValid(r, lat, lon)) {
      s_done_mask |= 1 << r;
      continue;
    }
    if (build(lat, lon, r)) {
      s_done_mask |= 1 << r;
    } else {
      s_retry_after = millis() + 60000;  // tiles unreachable; try again later
    }
    return true;
  }
  return false;
}

const uint8_t* lockCurrent(double lat, double lon, uint8_t range) {
  if (s_lock == nullptr) return nullptr;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  if (range < kSlots && slotValid(range, lat, lon)) {
    return s_mapped + range * kSlotSize + sizeof(Header);
  }
  return nullptr;
}

void unlock() {
  if (s_lock != nullptr) xSemaphoreGive(s_lock);
}

}  // namespace services::map
