#pragma once

#include <cstdint>

#include <LovyanGFX.hpp>

namespace ui::draw {

// Fast, clip-safe shapes built from LovyanGFX's integer primitives
// (triangles, circles, lines). The ESP32-C3 has no FPU, and LovyanGFX's
// drawWideLine / drawWedgeLine also replace the caller's clip rect, which
// breaks the firmware's half-screen rendering — use these instead.

/** Filled pie slice; angles in compass degrees (0 = up, clockwise). */
void pie(lgfx::LovyanGFX& g, int cx, int cy, int r, float a0, float a1, uint16_t color);
/** Filled ring segment between radii r_in..r_out; compass degrees. */
void ring(lgfx::LovyanGFX& g, int cx, int cy, int r_in, int r_out, float a0, float a1,
          uint16_t color);
/**
 * Line from (x0, y0) to (x1, y1) with radius r0 at the start and r1 at the
 * end (tapered), rounded ends when thick. Radii <= 0.75 draw a 1 px line.
 */
void thickLine(lgfx::LovyanGFX& g, float x0, float y0, float x1, float y1, float r0, float r1,
               uint16_t color);

}  // namespace ui::draw
