// Next-days page: five days of forecast, one row each — weekday, icon, rain
// chance and a min/max temperature bar on a shared scale.

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "ui/draw_util.h"
#include "ui/i18n.h"
#include "ui/pages.h"

namespace ui {

namespace {

using draw::kCx;
using fonts::Id;
using i18n::S;

const Rgb kTop{16, 40, 78};
const Rgb kBottom{6, 14, 32};
const Rgb kAccent{110, 190, 255};
const Rgb kWhite{238, 244, 252};
const Rgb kMuted{140, 160, 190};
const Rgb kRain{110, 180, 255};
const Rgb kCold{90, 170, 255};
const Rgb kWarm{255, 170, 70};
const Rgb kHot{255, 96, 70};

constexpr int kRowY = 74;
constexpr int kRowH = 29;

Rgb bgAt(int y) { return lerp(kTop, kBottom, y / 240.0f); }

float appear(uint32_t t, uint32_t start, uint32_t dur = 450) {
  return draw::easeOutCubic(draw::progress(t, start, dur));
}

/** Color for a temperature: blue when cold, orange when warm, red when hot. */
Rgb tempColor(float c) {
  if (c <= 15.0f) return kCold;
  if (c <= 25.0f) return lerp(kCold, kWarm, (c - 15.0f) / 10.0f);
  return lerp(kWarm, kHot, std::min(1.0f, (c - 25.0f) / 10.0f));
}

/** Small weather icon (~22 px) for a WMO code. */
void icon(lgfx::LovyanGFX& g, int cx, int cy, int code, float p, uint32_t t) {
  const Rgb bg = bgAt(cy);
  const Rgb light = lerp(bg, Rgb{214, 222, 236}, p);
  const Rgb shade = lerp(bg, Rgb{140, 152, 172}, p);
  const bool sunny = code <= 1;
  const bool partly = code == 2;
  const bool rain = (code >= 51 && code <= 67) || (code >= 80 && code <= 82);
  const bool snow = (code >= 71 && code <= 77) || code == 85 || code == 86;
  const bool storm = code >= 95;
  if (sunny) {
    draw::sun(g, cx, cy, 5, 0.55f * p, t * 0.02f, bg);
    return;
  }
  if (partly) {
    draw::sun(g, cx + 5, cy - 4, 4, 0.45f * p, t * 0.02f, bg);
  }
  const float dy = rain || snow || storm ? -3.0f : 0.0f;
  draw::cloud(g, cx - (partly ? 2 : 0), cy + dy, 0.30f, light, shade);
  if (rain) {
    // Drops fall a little, staggered.
    for (int i = 0; i < 3; ++i) {
      const float fall = fmodf(t / 700.0f + i * 0.33f, 1.0f);
      const int x = cx - 6 + i * 6;
      const int y = cy + 6 + static_cast<int>(fall * 5.0f);
      g.drawFastVLine(x, y, 3, mix(bg, kRain, p * (1.0f - fall * 0.7f)));
    }
  } else if (snow) {
    for (int i = 0; i < 3; ++i) {
      g.fillSmoothCircle(cx - 6 + i * 6, cy + 8 + (i % 2) * 2, 1, mix(bg, kWhite, p));
    }
  } else if (storm) {
    const uint16_t bolt = mix(bg, Rgb{255, 220, 80}, p);
    draw::thickLine(g, cx + 1, cy + 3, cx - 3, cy + 9, 1.4f, 1.4f, bolt);
    draw::thickLine(g, cx - 3, cy + 9, cx + 2, cy + 9, 1.4f, 1.4f, bolt);
    draw::thickLine(g, cx + 2, cy + 9, cx - 2, cy + 15, 1.4f, 1.0f, bolt);
  } else if (code == 45 || code == 48) {
    for (int i = 0; i < 2; ++i) {
      g.drawFastHLine(cx - 9 + i * 3, cy + 7 + i * 3, 16, mix(bg, kMuted, p));
    }
  }
}

/**
 * 12-hour chart: temperature line with labels every 3 h, rain-chance bars
 * underneath and the hour along the bottom. The line draws itself in.
 */
void drawHours(lgfx::LovyanGFX& g, const HourlyModel& h, uint32_t t) {
  constexpr int kX0 = kCx - 88;
  constexpr int kX1 = kCx + 88;
  constexpr int kTopY = 92;   // warmest point
  constexpr int kBotY = 140;  // coldest point
  constexpr int kRainY = 176; // rain bars grow up from here
  constexpr int kRainH = 22;
  float lo = h.temp_c[0];
  float hi = h.temp_c[0];
  for (int i = 1; i < h.count; ++i) {
    lo = std::min(lo, h.temp_c[i]);
    hi = std::max(hi, h.temp_c[i]);
  }
  const float span = std::max(3.0f, hi - lo);
  const float mid = (hi + lo) * 0.5f;
  auto px = [&](int i) { return kX0 + (kX1 - kX0) * i / std::max(1, h.count - 1); };
  auto py = [&](int i) {
    return (kTopY + kBotY) / 2.0f - (h.temp_c[i] - mid) / span * (kBotY - kTopY);
  };

  const float reveal = appear(t, 100, 900);  // fraction of the line drawn
  const float pf = appear(t, 0);
  char buf[12];
  // Rain bars (only where there is some chance) and hour labels.
  const int bw = std::max(4, (kX1 - kX0) / std::max(1, h.count) - 4);
  for (int i = 0; i < h.count; ++i) {
    const int x = px(i);
    const float p = appear(t, 150 + i * 40);
    if (h.rain_prob[i] >= 5) {
      const int bh = std::max(2, static_cast<int>(h.rain_prob[i] / 100.0f * kRainH * p));
      g.fillRoundRect(x - bw / 2, kRainY - bh, bw, bh, 2,
                      mix(bgAt(kRainY), kRain, 0.35f + 0.65f * h.rain_prob[i] / 100.0f));
    }
    if (i % 3 == 0) {
      snprintf(buf, sizeof(buf), "%dh", h.hour[i]);
      draw::text(g, Id::S14, buf, x, 192, mix(bgAt(192), kMuted, p));
    }
  }
  // Label the rainiest hour when the chance is worth mentioning.
  int wet = 0;
  for (int i = 1; i < h.count; ++i) {
    if (h.rain_prob[i] > h.rain_prob[wet]) wet = i;
  }
  if (h.rain_prob[wet] >= 30) {
    snprintf(buf, sizeof(buf), "%d%%", h.rain_prob[wet]);
    const int ty = kRainY - static_cast<int>(h.rain_prob[wet] / 100.0f * kRainH) - 9;
    draw::text(g, Id::S14, buf, px(wet), ty, mix(bgAt(ty), kRain, appear(t, 600)));
  }
  g.drawFastHLine(kX0 - 4, kRainY + 1, kX1 - kX0 + 8, mix(bgAt(kRainY), kAccent, 0.25f * pf));

  // Temperature line, colored by temperature, revealed left to right.
  const float last = reveal * (h.count - 1);
  for (int i = 0; i + 1 < h.count && i < last; ++i) {
    const float k = std::min(1.0f, last - i);
    const float x0 = px(i);
    const float y0 = py(i);
    const float x1 = x0 + (px(i + 1) - x0) * k;
    const float y1 = y0 + (py(i + 1) - y0) * k;
    draw::thickLine(g, x0, y0, x1, y1, 2.2f, 2.2f, rgb(tempColor(h.temp_c[i])));
  }
  for (int i = 0; i < h.count; i += 3) {
    if (i > last + 0.01f) break;
    const int x = px(i);
    const int y = static_cast<int>(py(i));
    g.fillSmoothCircle(x, y, 3, rgb(tempColor(h.temp_c[i])));
    snprintf(buf, sizeof(buf), "%d°", static_cast<int>(lroundf(h.temp_c[i])));
    draw::text(g, Id::S14, buf, x, y - 13, mix(bgAt(y - 13), kWhite, appear(t, 200 + i * 60)));
  }
}

}  // namespace

uint32_t drawForecastPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t) {
  draw::verticalGradient(g, kTop, kBottom);
  // Animations restart each time the view flips between days and hours.
  const uint32_t tv = m.forecast_view_ago_ms < t ? m.forecast_view_ago_ms : t;
  const float ph = appear(tv, 0);
  draw::text(g, Id::S14, i18n::tr(m.forecast_hours ? S::NextHours : S::NextDays), kCx, 40,
             mix(bgAt(40), kAccent, ph));
  g.drawFastHLine(kCx - 60, 54, 120, mix(bgAt(54), kAccent, 0.35f * ph));

  if (m.forecast_hours && m.hourly.valid) {
    drawHours(g, m.hourly, tv);
    draw::pageDots(g, static_cast<int>(Page::Forecast), kPageCount);
    return 40;
  }
  t = tv;
  const ForecastModel& f = m.forecast;
  if (!f.valid) {
    draw::text(g, Id::S17, i18n::tr(m.wifi_ok ? S::LoadingWeather : S::NoWifi), kCx, 120,
               rgb(kMuted));
    draw::pageDots(g, static_cast<int>(Page::Forecast), kPageCount);
    return 200;
  }

  // One temperature scale for the whole week, so the bars compare.
  float lo = f.days[0].t_min;
  float hi = f.days[0].t_max;
  for (int i = 1; i < f.count; ++i) {
    lo = std::min(lo, f.days[i].t_min);
    hi = std::max(hi, f.days[i].t_max);
  }
  const float span = std::max(1.0f, hi - lo);

  constexpr int kBarX0 = kCx + 36;
  constexpr int kBarW = 24;
  for (int i = 0; i < f.count; ++i) {
    const ForecastDay& d = f.days[i];
    const int y = kRowY + i * kRowH;
    const float p = appear(t, 150 + i * 90);
    if (p <= 0.0f) continue;
    const Rgb bg = bgAt(y);
    const int slide = static_cast<int>((1.0f - p) * 14.0f);
    char buf[16];

    draw::text(g, Id::S14, i18n::weekday(d.wday), kCx - 90 + slide, y,
               mix(bg, i == 0 ? kWhite : Rgb{200, 212, 232}, p), textdatum_t::middle_left);
    icon(g, kCx - 42 + slide, y, d.code, p, t);
    if (d.rain_prob >= 10) {
      snprintf(buf, sizeof(buf), "%d%%", d.rain_prob);
      draw::text(g, Id::S14, buf, kCx - 26 + slide, y, mix(bg, kRain, p * (d.rain_prob >= 40 ? 1.0f : 0.6f)),
                 textdatum_t::middle_left);
    }

    snprintf(buf, sizeof(buf), "%d°", static_cast<int>(lroundf(d.t_min)));
    draw::text(g, Id::S14, buf, kBarX0 - 4 + slide, y, mix(bg, kMuted, p), textdatum_t::middle_right);
    // Track, then the day's range growing in from the left.
    g.fillRoundRect(kBarX0 + slide, y - 2, kBarW, 5, 2, mix(bg, Rgb{40, 60, 96}, p));
    const float grow = appear(t, 350 + i * 90, 600);
    const int a = kBarX0 + static_cast<int>((d.t_min - lo) / span * kBarW);
    const int b = kBarX0 + static_cast<int>((d.t_max - lo) / span * kBarW);
    const int w = std::max(5, static_cast<int>((b - a) * grow));
    for (int x = 0; x < w; ++x) {
      const float k = (d.t_min + (d.t_max - d.t_min) * x / std::max(1, b - a));
      g.drawFastVLine(a + x + slide, y - 2, 5, mix(bg, tempColor(k), p));
    }
    snprintf(buf, sizeof(buf), "%d°", static_cast<int>(lroundf(d.t_max)));
    draw::text(g, Id::S14, buf, kBarX0 + kBarW + 5 + slide, y, mix(bg, kWhite, p),
               textdatum_t::middle_left);
  }

  draw::pageDots(g, static_cast<int>(Page::Forecast), kPageCount);
  return 40;
}

}  // namespace ui
