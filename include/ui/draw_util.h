#pragma once

#include <cstddef>
#include <cstdint>

#include <LovyanGFX.hpp>

#include "ui/color.h"
#include "ui/fonts.h"
#include "ui/shapes.h"

namespace ui::draw {

constexpr int kSize = 240;
constexpr int kCx = kSize / 2;
constexpr int kCy = kSize / 2;
constexpr float kPi = 3.14159265f;
constexpr float kDegToRad = kPi / 180.0f;

// ---- timing / easing ----
float clamp01(float t);
/** 0..1 progress of an animation that starts at start_ms and lasts dur_ms. */
float progress(uint32_t t_ms, uint32_t start_ms, uint32_t dur_ms);
float easeOutCubic(float t);
float easeInOutSine(float t);
/** Ease out with a small overshoot (things "land"). */
float easeOutBack(float t);

// ---- backgrounds ----
void verticalGradient(lgfx::LovyanGFX& g, const Rgb& top, const Rgb& bottom);
void radialGradient(lgfx::LovyanGFX& g, const Rgb& center, const Rgb& edge);

// ---- text ----
/** Draw UTF-8 text with a smooth font (transparent background). */
void text(lgfx::LovyanGFX& g, fonts::Id font, const char* s, int x, int y,
          uint16_t color, textdatum_t datum = textdatum_t::middle_center);
int textWidth(lgfx::LovyanGFX& g, fonts::Id font, const char* s);
/** Copy `in` to `out`, cutting with "…" so it fits max_w pixels. */
void fitText(lgfx::LovyanGFX& g, fonts::Id font, const char* in, int max_w,
             char* out, size_t out_len);

/** Page indicator dots along the bottom of the round screen. */
void pageDots(lgfx::LovyanGFX& g, int index, int count);

// ---- icons ----
/** Sun disc with a soft glow and rotating rays (ray_scale 0..1 grows them in). */
void sun(lgfx::LovyanGFX& g, float cx, float cy, float r, float ray_scale,
         float rot_deg, const Rgb& sky);
/** Puffy cloud centered on (cx, cy); scale 1.0 ≈ 64 px wide. */
void cloud(lgfx::LovyanGFX& g, float cx, float cy, float scale, const Rgb& light,
           const Rgb& shade);
/** Crescent moon (lit on the right). */
void moon(lgfx::LovyanGFX& g, int cx, int cy, int r, const Rgb& color);
/** Teardrop pointing up, centered on (cx, cy). */
void raindrop(lgfx::LovyanGFX& g, int cx, int cy, int h, uint16_t color);
/** Airplane silhouette, nose toward heading_deg (0 = up / north). */
void airplane(lgfx::LovyanGFX& g, float cx, float cy, float heading_deg, float size,
              uint16_t color);
/** Arrow pointing toward deg (0 = up), inside a circle of radius r. */
void arrow(lgfx::LovyanGFX& g, float cx, float cy, float r, float deg, uint16_t color);
/** Small up/down triangle used for min/max temperatures. */
void triangleMark(lgfx::LovyanGFX& g, int cx, int cy, bool up, uint16_t color);

}  // namespace ui::draw
