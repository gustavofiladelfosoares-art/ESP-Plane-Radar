#include "ui/radar_map.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <lgfx/utility/lgfx_tjpgd.h>

#include "ui/color.h"
#include "ui/radar_theme.h"

namespace ui::radar_map {

namespace {

constexpr double kPi = 3.14159265358979;
constexpr double kKmPerDeg = 111.0;  // same flat projection as the radar
constexpr int kTilePx = 256;

// Esri Dark Gray Base: land is one flat tone (~70), water clearly darker,
// built-up areas slightly lighter, roads clearly lighter. Offsets are taken
// from the dominant tone so decoder differences don't shift the classes.
constexpr int kWaterBelowLand = 18;
constexpr int kUrbanAboveLand = 6;
constexpr int kRoadAboveLand = 14;

uint8_t classify(int lum, const Thresholds& th) {
  if (lum < th.water_max) return 1;
  if (lum >= th.road_min) return 3;
  if (lum >= th.urban_min) return 2;
  return 0;
}

int luminance(const uint8_t* px) {
  // Symmetric in R/B, so byte order of the decoder output doesn't matter.
  return (px[0] + 2 * px[1] + px[2]) >> 2;
}

// When several tile pixels land on one screen pixel, keep the most telling
// class: road beats water beats built-up beats nothing.
constexpr uint8_t kPriority[4] = {0, 2, 1, 3};

void setLevel(uint8_t* map, int x, int y, uint8_t v) {
  const size_t i = static_cast<size_t>(y) * kSize + x;
  const int shift = (i & 3) * 2;
  map[i >> 2] = static_cast<uint8_t>((map[i >> 2] & ~(3 << shift)) | (v << shift));
}

struct Decode {
  const TilePlan* plan;
  const Thresholds* th;
  Histogram* hist;
  const uint8_t* data;
  size_t len;
  size_t pos;
  double ox;  // tile origin in global pixels
  double oy;
  uint8_t* map;
};

uint32_t readJpg(void* dev, uint8_t* buf, uint32_t n) {
  auto* d = static_cast<Decode*>(dev);
  const size_t left = d->len - d->pos;
  if (n > left) n = static_cast<uint32_t>(left);
  if (buf != nullptr) memcpy(buf, d->data + d->pos, n);
  d->pos += n;
  return n;
}

/** Screen pixel span [a, b) covered by tile pixel coordinate g (global). */
void span(double g, double center, double scale, int* a, int* b) {
  *a = static_cast<int>(std::ceil((g - center) * scale + kSize / 2.0 - 0.5));
  *b = static_cast<int>(std::ceil((g + 1.0 - center) * scale + kSize / 2.0 - 0.5));
}

uint32_t writeBlock(void* dev, void* bitmap, JRECT* r) {
  auto* d = static_cast<Decode*>(dev);
  const uint8_t* px = static_cast<const uint8_t*>(bitmap);
  const int w = r->right - r->left + 1;
  if (d->plan->scale < 1.0) {
    // Downsampling (the usual case): each tile pixel lands on the screen pixel
    // under its centre and merges by priority, so 1 px roads survive.
    const double s = d->plan->scale;
    for (uint32_t y = r->top; y <= r->bottom; ++y) {
      const int sy = static_cast<int>(std::floor((d->oy + y + 0.5 - d->plan->gy_c) * s + kSize / 2.0));
      if (sy < 0 || sy >= kSize) {
        px += w * 3;
        continue;
      }
      for (uint32_t x = r->left; x <= r->right; ++x, px += 3) {
        const int sx = static_cast<int>(std::floor((d->ox + x + 0.5 - d->plan->gx_c) * s + kSize / 2.0));
        if (sx < 0 || sx >= kSize) continue;
        const uint8_t v = classify(luminance(px), *d->th);
        if (kPriority[v] > kPriority[level(d->map, sx, sy)]) setLevel(d->map, sx, sy, v);
      }
    }
    return 1;
  }
  for (uint32_t y = r->top; y <= r->bottom; ++y) {
    int sy0 = 0;
    int sy1 = 0;
    span(d->oy + y, d->plan->gy_c, d->plan->scale, &sy0, &sy1);
    if (sy1 <= 0 || sy0 >= kSize) {
      px += w * 3;
      continue;
    }
    for (uint32_t x = r->left; x <= r->right; ++x, px += 3) {
      int sx0 = 0;
      int sx1 = 0;
      span(d->ox + x, d->plan->gx_c, d->plan->scale, &sx0, &sx1);
      if (sx1 <= 0 || sx0 >= kSize) continue;
      const uint8_t v = classify(luminance(px), *d->th);
      for (int sy = sy0 < 0 ? 0 : sy0; sy < sy1 && sy < kSize; ++sy) {
        for (int sx = sx0 < 0 ? 0 : sx0; sx < sx1 && sx < kSize; ++sx) {
          setLevel(d->map, sx, sy, v);
        }
      }
    }
  }
  return 1;
}

uint32_t countBlock(void* dev, void* bitmap, JRECT* r) {
  auto* d = static_cast<Decode*>(dev);
  const uint8_t* px = static_cast<const uint8_t*>(bitmap);
  const uint32_t n = (r->right - r->left + 1) * (r->bottom - r->top + 1);
  for (uint32_t i = 0; i < n; ++i, px += 3) {
    ++d->hist->bins[luminance(px)];
  }
  return 1;
}

bool decode(Decode* d, uint32_t (*out)(void*, void*, JRECT*)) {
  constexpr size_t kPool = 3900;
  void* pool = malloc(kPool);
  if (pool == nullptr) {
    return false;
  }
  lgfxJdec jd;
  const bool ok = lgfx_jd_prepare(&jd, readJpg, pool, kPool, d) == JDR_OK &&
                  lgfx_jd_decomp(&jd, out, 0) == JDR_OK;
  free(pool);
  return ok;
}

}  // namespace

TilePlan plan(double lat, double lon, float outer_km) {
  TilePlan p;
  const double lat_rad = lat * kPi / 180.0;
  const double px_per_km = radar::kGridOuterRadius / static_cast<double>(outer_km);
  // Screen px per degree of longitude vs tile px per degree at zoom z.
  const double screen_px_per_deg = kKmPerDeg * std::cos(lat_rad) * px_per_km;
  // Fetch 1–2 zoom levels more detail than the screen needs (scale 0.375..0.75):
  // thin roads only show up in higher-zoom tiles; writeBlock merges them down.
  const double fit = screen_px_per_deg * 360.0 / kTilePx;
  int z = static_cast<int>(std::floor(std::log2(fit)));
  z += fit / std::pow(2.0, z) < 1.5 ? 1 : 2;
  if (z < 3) z = 3;
  if (z > 16) z = 16;
  const double world = kTilePx * std::pow(2.0, z);
  p.z = z;
  p.scale = screen_px_per_deg / (world / 360.0);
  p.gx_c = (lon + 180.0) / 360.0 * world;
  p.gy_c = (1.0 - std::asinh(std::tan(lat_rad)) / kPi) / 2.0 * world;
  const double half = (kSize / 2.0) / p.scale;
  p.tx0 = static_cast<int>(std::floor((p.gx_c - half) / kTilePx));
  p.tx1 = static_cast<int>(std::floor((p.gx_c + half) / kTilePx));
  p.ty0 = static_cast<int>(std::floor((p.gy_c - half) / kTilePx));
  p.ty1 = static_cast<int>(std::floor((p.gy_c + half) / kTilePx));
  return p;
}

bool accumulate(const uint8_t* jpg, size_t len, Histogram* h) {
  Decode d{nullptr, nullptr, h, jpg, len, 0, 0.0, 0.0, nullptr};
  return decode(&d, countBlock);
}

Thresholds calibrate(const Histogram& h) {
  Thresholds th;
  uint32_t best = 0;
  int land = -1;
  // Ignore very dark bins so a mostly-sea view still finds the land tone.
  for (int v = 40; v < 256; ++v) {
    if (h.bins[v] > best) {
      best = h.bins[v];
      land = v;
    }
  }
  if (land < 0) {
    return th;  // defaults
  }
  th.water_max = static_cast<uint8_t>(land > kWaterBelowLand ? land - kWaterBelowLand : 0);
  th.urban_min = static_cast<uint8_t>(land + kUrbanAboveLand);
  th.road_min = static_cast<uint8_t>(land + kRoadAboveLand);
  return th;
}

bool paintTile(const TilePlan& p, int tx, int ty, const uint8_t* jpg, size_t len,
               const Thresholds& th, uint8_t* map) {
  Decode d{&p, &th, nullptr, jpg, len, 0, static_cast<double>(tx) * kTilePx,
           static_cast<double>(ty) * kTilePx, map};
  return decode(&d, writeBlock);
}

void despeckle(uint8_t* map) {
  // A built-up or water pixel survives only if at least 3 of its 8
  // neighbours share its class; roads are thin lines and are left alone.
  // Works in place: keeps the original copies of the previous and current
  // packed rows (60 bytes each) while rewriting the map.
  constexpr size_t kRow = kSize / 4;
  uint8_t prev[kRow];
  uint8_t cur[kRow];
  memcpy(prev, map, kRow);
  auto at = [&](const uint8_t* row, int x) -> uint8_t { return (row[x >> 2] >> ((x & 3) * 2)) & 3; };
  for (int y = 1; y < kSize - 1; ++y) {
    memcpy(cur, map + y * kRow, kRow);
    const uint8_t* next = map + (y + 1) * kRow;  // not rewritten yet
    for (int x = 1; x < kSize - 1; ++x) {
      const uint8_t v = at(cur, x);
      if (v != 1 && v != 2) continue;
      int same = 0;
      for (int dx = -1; dx <= 1; ++dx) {
        same += (at(prev, x + dx) == v) + (at(next, x + dx) == v) + (dx != 0 && at(cur, x + dx) == v);
      }
      if (same < 3) setLevel(map, x, y, 0);
    }
    memcpy(prev, cur, kRow);
  }
}

void draw(lgfx::LovyanGFX& g, const uint8_t* map, uint16_t* sprite_buffer, int y0, int y1) {
  if (map == nullptr) {
    return;
  }
  const uint16_t water = rgb(34, 84, 150);
  const uint16_t urban = rgb(30, 70, 128);
  const uint16_t road = rgb(58, 116, 190);
  // Sprites keep RGB565 big-endian in memory.
  const uint16_t colors[4] = {0, static_cast<uint16_t>(__builtin_bswap16(water)),
                              static_cast<uint16_t>(__builtin_bswap16(urban)),
                              static_cast<uint16_t>(__builtin_bswap16(road))};
  constexpr int kR = kSize / 2 - 1;
  for (int y = y0 < 0 ? 0 : y0; y < kSize && y < y1; ++y) {
    const int dy = y - kSize / 2;
    const int half = static_cast<int>(std::sqrt(static_cast<double>(kR * kR - dy * dy)));
    uint16_t* row = sprite_buffer != nullptr ? sprite_buffer + y * kSize : nullptr;
    for (int x = kSize / 2 - half; x < kSize / 2 + half; ++x) {
      const uint8_t v = level(map, x, y);
      // Water: 50% checker; built-up: sparse dots (like the reference radar's
      // land); roads: solid.
      const bool on = v == 3 || (v == 1 && ((x + y) & 1) == 0) ||
                      (v == 2 && (x & 1) == 0 && (y & 1) == 0);
      if (!on) continue;
      if (row != nullptr) {
        row[x] = colors[v];
      } else {
        g.drawPixel(x, y, v == 1 ? water : v == 2 ? urban : road);
      }
    }
  }
}

}  // namespace ui::radar_map
