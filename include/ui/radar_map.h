#pragma once

#include <cstddef>
#include <cstdint>

#include <LovyanGFX.hpp>

/**
 * Radar background map: a 240×240, 2-bit-per-pixel layer built from
 * Esri "World Dark Gray Base" JPEG tiles (no labels). Levels:
 *   0 = nothing, 1 = water, 2 = built-up area, 3 = road.
 * Tiles are fetched once per location/range and the result is cached.
 */
namespace ui::radar_map {

constexpr int kSize = 240;
constexpr size_t kBytes = kSize * kSize / 4;

/** Bump when classification or layout changes so cached maps rebuild. */
constexpr uint8_t kFormatVersion = 2;

/** Tile URL; printf with (z, y, x). */
constexpr char kTileUrlFormat[] =
    "https://server.arcgisonline.com/ArcGIS/rest/services/Canvas/"
    "World_Dark_Gray_Base/MapServer/tile/%d/%d/%d";

struct TilePlan {
  int z = 0;
  double gx_c = 0.0;  // radar center in global tile pixels at zoom z
  double gy_c = 0.0;
  double scale = 1.0;  // screen pixels per tile pixel (>= 1)
  int tx0 = 0;
  int ty0 = 0;
  int tx1 = 0;
  int ty1 = 0;
};

/** Pick zoom and tiles so the map matches the radar scale for outer_km. */
TilePlan plan(double lat, double lon, float outer_km);

/** Luminance histogram over every tile, used to calibrate the classes. */
struct Histogram {
  uint32_t bins[256] = {};
};

/** Luminance cut-offs for water / built-up / road (see calibrate()). */
struct Thresholds {
  uint8_t water_max = 52;
  uint8_t urban_min = 76;
  uint8_t road_min = 84;
};

/** Pass 1: add one JPEG tile's pixels to the histogram. */
bool accumulate(const uint8_t* jpg, size_t len, Histogram* h);

/** Derive thresholds from the dominant (land) tone of the histogram. */
Thresholds calibrate(const Histogram& h);

/** Pass 2: decode one JPEG tile and paint its classified pixels into map. */
bool paintTile(const TilePlan& p, int tx, int ty, const uint8_t* jpg, size_t len,
               const Thresholds& th, uint8_t* map);

/** Remove speckles left by JPEG noise (isolated built-up / water pixels). */
void despeckle(uint8_t* map);

inline uint8_t level(const uint8_t* map, int x, int y) {
  const size_t i = static_cast<size_t>(y) * kSize + x;
  return (map[i >> 2] >> ((i & 3) * 2)) & 3;
}

/**
 * Draw the map layer (dithered water / dotted city / solid roads). When the
 * target is a 16-bit sprite, pass its buffer to write pixels directly.
 */
void draw(lgfx::LovyanGFX& g, const uint8_t* map, uint16_t* sprite_buffer = nullptr,
          int y0 = 0, int y1 = kSize);

}  // namespace ui::radar_map
