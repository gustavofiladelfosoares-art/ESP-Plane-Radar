// Air / UV / Sun page: two ring gauges and the sun's path across the day.

#include <cmath>
#include <cstdio>

#include "ui/draw_util.h"
#include "ui/pages.h"

namespace ui {

namespace {

using draw::kCx;
using draw::kDegToRad;
using fonts::Id;

const Rgb kTop{8, 22, 42};
const Rgb kBottom{4, 10, 22};
const Rgb kMuted{140, 158, 184};

Rgb bgAt(int y) { return lerp(kTop, kBottom, y / 240.0f); }

struct Category {
  const char* label;
  Rgb color;
};

Category aqiCategory(int aqi) {
  if (aqi <= 50) return {"Boa", {80, 210, 120}};
  if (aqi <= 100) return {"Moderada", {240, 208, 60}};
  if (aqi <= 150) return {"Ruim p/ sens.", {255, 150, 50}};
  if (aqi <= 200) return {"Ruim", {240, 72, 60}};
  if (aqi <= 300) return {"Muito ruim", {176, 96, 210}};
  return {"Péssima", {160, 50, 80}};
}

Category uvCategory(float uv) {
  if (uv < 2.5f) return {"Baixo", {80, 210, 120}};
  if (uv < 5.5f) return {"Moderado", {240, 208, 60}};
  if (uv < 7.5f) return {"Alto", {255, 150, 50}};
  if (uv < 10.5f) return {"Muito alto", {240, 72, 60}};
  return {"Extremo", {176, 96, 210}};
}

/** 270° ring gauge with a rounded tip, value in the middle, caption below. */
void gauge(lgfx::LovyanGFX& g, int cx, int cy, float frac, const char* value,
           const char* caption, const Category& cat, float p, uint32_t t) {
  constexpr int kR0 = 34;
  constexpr int kR1 = 27;
  constexpr float kStart = 135.0f;
  constexpr float kSweep = 270.0f;
  const Rgb bg = bgAt(cy);
  // Compass angles for draw::ring (LovyanGFX arcs use 0° = east; +90 converts).
  draw::ring(g, cx, cy, kR1, kR0, kStart + 90.0f, kStart + 90.0f + kSweep, mix(bg, Rgb{40, 56, 84}, 0.9f));
  const float f = draw::clamp01(frac) * p;
  if (f > 0.005f) {
    const float end = kStart + kSweep * f;
    draw::ring(g, cx, cy, kR1, kR0, kStart + 90.0f, end + 90.0f, rgb(cat.color));
    // Rounded caps at both ends.
    const float rm = (kR0 + kR1) * 0.5f;
    const float cap = (kR0 - kR1) * 0.5f;
    for (float a : {kStart, end}) {
      g.fillSmoothCircle(static_cast<int>(cx + cosf(a * kDegToRad) * rm),
                         static_cast<int>(cy + sinf(a * kDegToRad) * rm), static_cast<int>(cap),
                         rgb(cat.color));
    }
    // A soft pulse on the tip once filled.
    if (p >= 1.0f) {
      const float pulse = 0.5f + 0.5f * sinf(t * 0.004f);
      g.fillSmoothCircle(static_cast<int>(cx + cosf(end * kDegToRad) * rm),
                         static_cast<int>(cy + sinf(end * kDegToRad) * rm), static_cast<int>(cap + 1),
                         mix(cat.color, Rgb{255, 255, 255}, 0.35f * pulse));
    }
  }
  draw::text(g, Id::S22, value, cx, cy - 3, mix(bg, Rgb{255, 255, 255}, draw::clamp01(p * 2)));
  draw::text(g, Id::S14, caption, cx, cy + 16, mix(bg, kMuted, draw::clamp01(p * 2)));
}

void formatHm(char* out, size_t len, int minutes) {
  minutes = ((minutes % 1440) + 1440) % 1440;
  snprintf(out, len, "%02d:%02d", minutes / 60, minutes % 60);
}

void formatSpan(char* out, size_t len, int minutes) {
  if (minutes < 60) {
    snprintf(out, len, "%d min", minutes);
  } else {
    snprintf(out, len, "%dh%02d", minutes / 60, minutes % 60);
  }
}

void drawSunArc(lgfx::LovyanGFX& g, const Model& m, uint32_t t) {
  constexpr int kCy = 200;
  constexpr int kR = 62;
  const SunModel& sun = m.sun;
  const float pin = draw::easeOutCubic(draw::progress(t, 300, 500));

  // Horizon stubs on either side of the arc.
  const uint16_t horizon = mix(bgAt(kCy), Rgb{90, 110, 140}, pin);
  g.drawFastHLine(kCx - kR - 16, kCy, 12, horizon);
  g.drawFastHLine(kCx + kR + 4, kCy, 12, horizon);
  // Dotted path of the sun.
  for (int a = 180; a <= 360; a += 9) {
    const int x = static_cast<int>(kCx + cosf(a * kDegToRad) * kR);
    const int y = static_cast<int>(kCy + sinf(a * kDegToRad) * kR);
    g.fillSmoothCircle(x, y, 1, mix(bgAt(y), Rgb{110, 128, 160}, pin));
  }

  if (!sun.valid || !m.time.valid) {
    draw::text(g, Id::S14, "Sol: aguardando…", kCx, 180, mix(bgAt(180), kMuted, pin));
    return;
  }

  const int now = m.time.hour * 60 + m.time.minute;
  const int day_len = sun.sunset_min - sun.sunrise_min;
  const bool is_day = now >= sun.sunrise_min && now < sun.sunset_min && day_len > 0;
  const float travel = draw::easeOutCubic(draw::progress(t, 400, 1200));

  char left[8];
  char right[8];
  formatHm(left, sizeof(left), sun.sunrise_min);
  formatHm(right, sizeof(right), sun.sunset_min);
  constexpr int kTy = 214;
  const uint16_t tc = mix(bgAt(kTy), Rgb{220, 228, 240}, pin);
  draw::triangleMark(g, kCx - 66, kTy, true, mix(bgAt(kTy), Rgb{255, 190, 80}, pin));
  draw::text(g, Id::S14, left, kCx - 58, kTy, tc, textdatum_t::middle_left);
  draw::triangleMark(g, kCx + 18, kTy, false, mix(bgAt(kTy), Rgb{255, 130, 70}, pin));
  draw::text(g, Id::S14, right, kCx + 26, kTy, tc, textdatum_t::middle_left);

  char span[12];
  if (is_day) {
    const float f = static_cast<float>(now - sun.sunrise_min) / day_len * travel;
    const float a = 180.0f + 180.0f * f;
    if (f > 0.01f) {
      draw::ring(g, kCx, kCy, kR - 1, kR + 1, 270.0f, a + 90.0f, rgb(255, 186, 64));
    }
    const float sx = kCx + cosf(a * kDegToRad) * kR;
    const float sy = kCy + sinf(a * kDegToRad) * kR;
    draw::sun(g, sx, sy, 7, travel * 0.8f, t * 0.03f, bgAt(static_cast<int>(sy)));
    formatSpan(span, sizeof(span), sun.sunset_min - now);
    draw::text(g, Id::S22, span, kCx, 174, mix(bgAt(174), Rgb{255, 255, 255}, pin));
    draw::text(g, Id::S14, "até o pôr do sol", kCx, 193, mix(bgAt(193), kMuted, pin));
  } else {
    const int until = now < sun.sunrise_min ? sun.sunrise_min - now : sun.sunrise_min + 1440 - now;
    draw::moon(g, kCx, kCy - kR + 2, 9, lerp(bgAt(kCy - kR), Rgb{236, 232, 206}, pin));
    formatSpan(span, sizeof(span), until);
    draw::text(g, Id::S22, span, kCx, 174, mix(bgAt(174), Rgb{255, 255, 255}, pin));
    draw::text(g, Id::S14, "até o sol nascer", kCx, 193, mix(bgAt(193), kMuted, pin));
  }
}

}  // namespace

uint32_t drawAirSunPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t) {
  draw::verticalGradient(g, kTop, kBottom);

  const float fill = draw::easeOutCubic(draw::progress(t, 150, 1000));
  constexpr int kGy = 78;

  if (m.air.valid) {
    char v[8];
    const Category ca = aqiCategory(m.air.aqi);
    snprintf(v, sizeof(v), "%d", static_cast<int>(lroundf(m.air.aqi * fill)));
    gauge(g, 74, kGy, m.air.aqi / 200.0f, v, "AR", ca, fill, t);
    const Category cu = uvCategory(m.air.uv);
    snprintf(v, sizeof(v), "%d", static_cast<int>(lroundf(m.air.uv * fill)));
    gauge(g, 166, kGy, m.air.uv / 12.0f, v, "UV", cu, fill, t);

    const float pc = draw::easeOutCubic(draw::progress(t, 700, 450));
    draw::text(g, Id::S14, ca.label, 74, 124, mix(bgAt(124), ca.color, pc));
    draw::text(g, Id::S14, cu.label, 166, 124, mix(bgAt(124), cu.color, pc));
  } else {
    draw::text(g, Id::S17, m.wifi_ok ? "Carregando ar e UV…" : "Sem Wi-Fi", kCx, 96,
               rgb(220, 228, 240));
  }

  drawSunArc(g, m, t);
  draw::pageDots(g, static_cast<int>(Page::AirSun), kPageCount);
  return 40;
}

}  // namespace ui
