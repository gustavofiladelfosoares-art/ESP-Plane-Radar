#pragma once

#include <cstdint>

namespace ui {

/** Logical 24-bit color used for blending before converting to RGB565. */
struct Rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

/**
 * GC9A01 modules in this build swap red/blue (see config::kDisplayRgbOrder).
 * Every page color goes through rgb() so logical colors render as intended.
 * The simulator leaves this false so previews show what the panel shows.
 */
extern bool g_swap_rb;

inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  if (g_swap_rb) {
    const uint8_t t = r;
    r = b;
    b = t;
  }
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

inline uint16_t rgb(const Rgb& c) { return rgb(c.r, c.g, c.b); }

inline Rgb lerp(const Rgb& a, const Rgb& b, float t) {
  if (t <= 0.0f) {
    return a;
  }
  if (t >= 1.0f) {
    return b;
  }
  return Rgb{static_cast<uint8_t>(a.r + (b.r - a.r) * t),
             static_cast<uint8_t>(a.g + (b.g - a.g) * t),
             static_cast<uint8_t>(a.b + (b.b - a.b) * t)};
}

inline uint16_t mix(const Rgb& a, const Rgb& b, float t) { return rgb(lerp(a, b, t)); }

/** RGB888 packed for LovyanGFX gradient APIs (swap applied like rgb()). */
inline uint32_t rgb888(const Rgb& c) {
  if (g_swap_rb) {
    return (static_cast<uint32_t>(c.b) << 16) | (c.g << 8) | c.r;
  }
  return (static_cast<uint32_t>(c.r) << 16) | (c.g << 8) | c.b;
}

}  // namespace ui
