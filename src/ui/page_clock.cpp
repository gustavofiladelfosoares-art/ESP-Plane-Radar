// Clock page: analog face with sweeping second hand, date and temperature.

#include <cmath>
#include <cstdio>

#include "ui/draw_util.h"
#include "ui/pages.h"

namespace ui {

namespace {

using draw::kCx;
using draw::kCy;
using draw::kDegToRad;
using fonts::Id;

constexpr const char* kWeekdays[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SÁB"};
constexpr const char* kMonths[] = {"JAN", "FEV", "MAR", "ABR", "MAI", "JUN",
                                   "JUL", "AGO", "SET", "OUT", "NOV", "DEZ"};

const Rgb kBgCenter{24, 34, 60};
const Rgb kBgEdge{4, 6, 14};
const Rgb kAccent{70, 160, 255};

Rgb bgAt(int x, int y) {
  const float d = sqrtf(static_cast<float>((x - kCx) * (x - kCx) + (y - kCy) * (y - kCy)));
  return lerp(kBgCenter, kBgEdge, d / 120.0f);
}

void polar(float deg, float r, float* x, float* y) {
  const float a = deg * kDegToRad;
  *x = kCx + sinf(a) * r;
  *y = kCy - cosf(a) * r;
}

void hand(lgfx::LovyanGFX& g, float deg, float len, float tail, float w0, float w1,
          const Rgb& color) {
  float x0 = 0;
  float y0 = 0;
  float x1 = 0;
  float y1 = 0;
  polar(deg + 180.0f, tail, &x0, &y0);
  polar(deg, len, &x1, &y1);
  // Soft drop shadow first, offset down-right.
  const Rgb shadow = lerp(bgAt(kCx, kCy), Rgb{0, 0, 0}, 0.6f);
  draw::thickLine(g, x0 + 2, y0 + 3, x1 + 2, y1 + 3, w0, w1, rgb(shadow));
  draw::thickLine(g, x0, y0, x1, y1, w0, w1, rgb(color));
}

void drawTicks(lgfx::LovyanGFX& g, uint32_t t) {
  for (int i = 0; i < 60; ++i) {
    // Ticks sweep in clockwise on entry.
    const float p = draw::progress(t, i * 10, 160);
    if (p <= 0.0f) {
      continue;
    }
    const bool hour = i % 5 == 0;
    const float r1 = 111.0f;
    const float r0 = hour ? 99.0f : 106.0f;
    float x0 = 0;
    float y0 = 0;
    float x1 = 0;
    float y1 = 0;
    polar(i * 6.0f, r0 + (r1 - r0) * (1.0f - p), &x0, &y0);
    polar(i * 6.0f, r1, &x1, &y1);
    const Rgb c = hour ? Rgb{232, 238, 248} : Rgb{96, 108, 132};
    const uint16_t col = mix(bgAt(static_cast<int>(x1), static_cast<int>(y1)), c, p);
    g.drawLine(static_cast<int>(x0), static_cast<int>(y0), static_cast<int>(x1), static_cast<int>(y1), col);
    if (hour) {
      // Thicker hour marks: two more lines offset sideways.
      const float a = i * 6.0f * kDegToRad;
      const int ox = static_cast<int>(lroundf(cosf(a)));
      const int oy = static_cast<int>(lroundf(sinf(a)));
      g.drawLine(static_cast<int>(x0) + ox, static_cast<int>(y0) + oy, static_cast<int>(x1) + ox,
                 static_cast<int>(y1) + oy, col);
      g.drawLine(static_cast<int>(x0) - ox, static_cast<int>(y0) - oy, static_cast<int>(x1) - ox,
                 static_cast<int>(y1) - oy, col);
    }
  }
}

void drawNumbers(lgfx::LovyanGFX& g, uint32_t t) {
  const float p = draw::easeOutCubic(draw::progress(t, 350, 500));
  if (p <= 0.0f) {
    return;
  }
  const char* labels[] = {"12", "3", "6", "9"};
  for (int i = 0; i < 4; ++i) {
    float x = 0;
    float y = 0;
    polar(i * 90.0f, 84.0f + (1.0f - p) * 8.0f, &x, &y);
    draw::text(g, Id::S22, labels[i], static_cast<int>(x), static_cast<int>(y) + 1,
               mix(bgAt(static_cast<int>(x), static_cast<int>(y)), Rgb{205, 214, 230}, p));
  }
}

void drawSecondsRing(lgfx::LovyanGFX& g, float sec_deg, uint32_t t) {
  const float p = draw::easeOutCubic(draw::progress(t, 0, 900));
  g.drawCircle(kCx, kCy, 117, rgb(28, 40, 66));
  const float end = -90.0f + sec_deg * p;
  if (sec_deg * p > 0.5f) {
    draw::ring(g, kCx, kCy, 116, 118, 0.0f, end + 90.0f, rgb(kAccent));
  }
  float x = 0;
  float y = 0;
  polar(sec_deg * p, 117.0f, &x, &y);
  g.fillSmoothCircle(static_cast<int>(x), static_cast<int>(y), 3, rgb(150, 205, 255));
}

void drawMiniWeather(lgfx::LovyanGFX& g, const WeatherModel& w, uint32_t t) {
  if (!w.valid) {
    return;
  }
  const float p = draw::easeOutCubic(draw::progress(t, 500, 500));
  if (p <= 0.0f) {
    return;
  }
  char temp[10];
  snprintf(temp, sizeof(temp), "%d°", static_cast<int>(lroundf(w.temp_c)));
  const int tw = draw::textWidth(g, Id::S22, temp);
  const int icon_w = 22;
  const int x0 = kCx - (icon_w + 4 + tw) / 2;
  const int y = 76;
  const Rgb bg = bgAt(kCx, y);
  if (w.code == 0 && w.is_day) {
    draw::sun(g, x0 + 10, y, 7, p, t * 0.02f, bg);
  } else if (w.code == 0) {
    draw::moon(g, x0 + 10, y, 8, lerp(bg, Rgb{240, 236, 210}, p));
  } else {
    draw::cloud(g, x0 + 11, y + 1, 0.34f, lerp(bg, Rgb{220, 228, 240}, p), lerp(bg, Rgb{140, 152, 172}, p));
  }
  draw::text(g, Id::S22, temp, x0 + icon_w + 4, y + 1, mix(bg, Rgb{240, 244, 250}, p),
             textdatum_t::middle_left);
}

}  // namespace

uint32_t drawClockPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t) {
  draw::radialGradient(g, kBgCenter, kBgEdge);

  const TimeModel& tm = m.time;
  float sec = 0.0f;
  float minute = 0.0f;
  float hour = 0.0f;
  if (tm.valid) {
    sec = tm.second + tm.millis / 1000.0f;
    minute = tm.minute + sec / 60.0f;
    hour = (tm.hour % 12) + minute / 60.0f;
  } else {
    // Not synced yet: park the hands at 10:10 and let the seconds idle.
    minute = 10.0f;
    hour = 10.0f + 10.0f / 60.0f;
    sec = fmodf(t / 1000.0f, 60.0f);
  }

  drawSecondsRing(g, sec * 6.0f, t);
  drawTicks(g, t);
  drawNumbers(g, t);
  drawMiniWeather(g, m.weather, t);

  // Date + digital time under the center.
  const float info = draw::easeOutCubic(draw::progress(t, 600, 500));
  if (info > 0.0f) {
    char date[24];
    char hm[8];
    if (tm.valid) {
      snprintf(date, sizeof(date), "%s, %d %s", kWeekdays[tm.wday % 7], tm.day,
               kMonths[(tm.month + 11) % 12]);
      snprintf(hm, sizeof(hm), "%02d:%02d", tm.hour, tm.minute);
    } else {
      snprintf(date, sizeof(date), "Acertando…");
      snprintf(hm, sizeof(hm), "--:--");
    }
    draw::text(g, Id::S14, date, kCx, 160, mix(bgAt(kCx, 160), Rgb{120, 186, 255}, info));
    draw::text(g, Id::S17, hm, kCx, 180, mix(bgAt(kCx, 180), Rgb{176, 188, 208}, info));
  }

  // Hands spin from 12:00 to the current time on entry.
  const float spin = draw::easeOutCubic(draw::progress(t, 0, 1100));
  hand(g, hour * 30.0f * spin, 52.0f, 10.0f, 4.4f, 2.4f, Rgb{238, 242, 250});
  hand(g, minute * 6.0f * spin, 80.0f, 12.0f, 3.2f, 1.6f, Rgb{238, 242, 250});
  hand(g, sec * 6.0f * spin, 96.0f, 22.0f, 1.3f, 1.0f, Rgb{255, 112, 52});
  g.fillSmoothCircle(kCx, kCy, 6, rgb(255, 112, 52));
  g.fillSmoothCircle(kCx, kCy, 2, rgb(30, 20, 16));

  return 50;
}

}  // namespace ui
