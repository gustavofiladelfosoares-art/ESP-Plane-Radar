// Weather page: animated sky for the current conditions, big temperature,
// feels-like and today's min / max / chance of rain.

#include <cmath>
#include <cstdio>

#include "ui/draw_util.h"
#include "ui/i18n.h"
#include "ui/pages.h"

namespace ui {

namespace {

using draw::kCx;
using draw::kDegToRad;
using fonts::Id;

enum class Scene { Clear, Partly, Cloudy, Fog, Drizzle, Rain, Showers, Snow, Storm };

Scene sceneFor(int code) {
  if (code == 0) return Scene::Clear;
  if (code == 1 || code == 2) return Scene::Partly;
  if (code == 3) return Scene::Cloudy;
  if (code == 45 || code == 48) return Scene::Fog;
  if (code >= 51 && code <= 57) return Scene::Drizzle;
  if (code >= 61 && code <= 67) return Scene::Rain;
  if (code >= 80 && code <= 82) return Scene::Showers;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return Scene::Snow;
  if (code >= 95) return Scene::Storm;
  return Scene::Cloudy;
}

struct Sky {
  Rgb top;
  Rgb bottom;
};

Sky skyFor(Scene s, bool day) {
  if (day) {
    switch (s) {
      case Scene::Clear: return {{18, 92, 196}, {84, 160, 230}};
      case Scene::Partly: return {{30, 88, 170}, {98, 150, 204}};
      case Scene::Cloudy: return {{62, 76, 98}, {112, 126, 146}};
      case Scene::Fog: return {{92, 102, 114}, {138, 144, 150}};
      case Scene::Snow: return {{90, 110, 140}, {128, 146, 172}};
      case Scene::Storm: return {{22, 22, 40}, {64, 58, 88}};
      default: return {{40, 52, 72}, {84, 100, 122}};  // drizzle / rain / showers
    }
  }
  switch (s) {
    case Scene::Clear:
    case Scene::Partly: return {{4, 8, 28}, {24, 36, 80}};
    case Scene::Cloudy:
    case Scene::Fog: return {{16, 20, 34}, {46, 52, 72}};
    case Scene::Snow: return {{26, 34, 54}, {70, 82, 106}};
    case Scene::Storm: return {{8, 6, 18}, {36, 30, 56}};
    default: return {{10, 14, 26}, {36, 44, 62}};
  }
}

Rgb skyAt(const Sky& sky, int y) {
  return lerp(sky.top, sky.bottom, static_cast<float>(y) / draw::kSize);
}

/** Cheap deterministic hash → 0..1 for particle placement. */
float hash01(uint32_t i) {
  i ^= i >> 16;
  i *= 0x7feb352dU;
  i ^= i >> 15;
  i *= 0x846ca68bU;
  i ^= i >> 16;
  return static_cast<float>(i & 0xFFFF) / 65535.0f;
}

constexpr int kIconY = 50;

// ---- particles ----

void drawRain(lgfx::LovyanGFX& g, const Sky& sky, float cloud_x, uint32_t t, float intensity,
              int count, float speed, int streak) {
  if (intensity <= 0.0f) {
    return;
  }
  const Rgb drop{150, 200, 255};
  const int top = kIconY + 16;
  constexpr float kFall = 30.0f;
  const int n = static_cast<int>(count * intensity);
  for (int i = 0; i < n; ++i) {
    const float x0 = cloud_x - 26.0f + hash01(i * 7 + 1) * 52.0f;
    const float phase = hash01(i * 13 + 5);
    const float f = fmodf(t * speed * (0.8f + 0.4f * hash01(i)) + phase * kFall, kFall);
    const float y = top + f;
    const float fade = 1.0f - f / kFall;
    const uint16_t c = mix(skyAt(sky, static_cast<int>(y)), drop, 0.35f + 0.65f * fade);
    g.drawLine(static_cast<int>(x0 - f * 0.12f), static_cast<int>(y),
               static_cast<int>(x0 - f * 0.12f - 2), static_cast<int>(y + streak), c);
  }
}

void drawSnow(lgfx::LovyanGFX& g, const Sky& sky, float cloud_x, uint32_t t, float intensity) {
  const int n = static_cast<int>(14 * intensity);
  const int top = kIconY + 14;
  constexpr float kFall = 34.0f;
  for (int i = 0; i < n; ++i) {
    const float x0 = cloud_x - 26.0f + hash01(i * 7 + 3) * 52.0f;
    const float f = fmodf(t * 0.018f * (0.7f + 0.6f * hash01(i + 9)) + hash01(i * 3) * kFall, kFall);
    const float x = x0 + sinf(t * 0.003f + i) * 3.0f;
    const float y = top + f;
    const float fade = 1.0f - f / kFall;
    g.fillCircle(static_cast<int>(x), static_cast<int>(y), 2,
                 mix(skyAt(sky, static_cast<int>(y)), Rgb{245, 250, 255}, 0.3f + 0.7f * fade));
  }
}

void drawStars(lgfx::LovyanGFX& g, const Sky& sky, uint32_t t) {
  for (int i = 0; i < 26; ++i) {
    const float a = hash01(i * 11 + 2) * 2.0f * draw::kPi;
    const float r = 30.0f + hash01(i * 5 + 7) * 80.0f;
    const int x = static_cast<int>(kCx + cosf(a) * r);
    const int y = static_cast<int>(20 + hash01(i * 17 + 1) * 110.0f);
    if ((x - 120) * (x - 120) + (y - 120) * (y - 120) > 112 * 112) {
      continue;
    }
    const float tw = 0.5f + 0.5f * sinf(t * 0.004f + hash01(i) * 6.28f);
    const uint16_t c = mix(skyAt(sky, y), Rgb{255, 255, 235}, 0.35f + 0.65f * tw);
    if (hash01(i * 23) > 0.8f) {
      g.fillSmoothCircle(x, y, 1, c);
    } else {
      g.drawPixel(x, y, c);
    }
  }
  // A shooting star every ~9 s.
  const uint32_t cycle = t % 9000;
  if (cycle < 700) {
    const float p = cycle / 700.0f;
    const float x = 40 + p * 110;
    const float y = 34 + p * 38;
    const uint16_t head = rgb(255, 255, 230);
    draw::thickLine(g, x - 26, y - 9, x, y, 0.3f, 1.2f,
                    mix(skyAt(sky, static_cast<int>(y)), Rgb{255, 255, 230}, 1.0f - p * 0.6f));
    g.fillSmoothCircle(static_cast<int>(x), static_cast<int>(y), 1, head);
  }
}

void drawBolt(lgfx::LovyanGFX& g, int x, int y, uint16_t color) {
  g.fillTriangle(x - 4, y, x + 7, y, x - 3, y + 15, color);
  g.fillTriangle(x + 3, y + 9, x + 11, y + 9, x - 5, y + 28, color);
}

void drawFogBands(lgfx::LovyanGFX& g, const Sky& sky, uint32_t t, float appear) {
  for (int i = 0; i < 3; ++i) {
    const int y = kIconY + 14 + i * 8;
    const float dx = sinf(t * 0.0012f + i * 1.7f) * 8.0f;
    const float w = (48.0f - i * 6.0f) * appear;
    const uint16_t c = mix(skyAt(sky, y), Rgb{214, 220, 226}, 0.75f);
    g.fillRoundRect(static_cast<int>(kCx - w + dx), y - 2, static_cast<int>(2 * w), 5, 2, c);
  }
}

// ---- text helpers ----

void formatTemp(char* out, size_t len, float c) {
  snprintf(out, len, "%d°", static_cast<int>(lroundf(c)));
}

/** Fade a text line in from the sky color while sliding it up a few px. */
void fadeLine(lgfx::LovyanGFX& g, const Sky& sky, Id font, const char* s, int y, const Rgb& color,
              uint32_t t, uint32_t start) {
  const float p = draw::easeOutCubic(draw::progress(t, start, 450));
  if (p <= 0.0f) {
    return;
  }
  const int yy = y + static_cast<int>((1.0f - p) * 10.0f);
  draw::text(g, font, s, kCx, yy, mix(skyAt(sky, yy), color, p));
}

void drawStatsRow(lgfx::LovyanGFX& g, const Sky& sky, const WeatherModel& w, uint32_t t) {
  const float p = draw::easeOutCubic(draw::progress(t, 750, 450));
  if (p <= 0.0f) {
    return;
  }
  const int y = 194 + static_cast<int>((1.0f - p) * 10.0f);
  const Rgb bg = skyAt(sky, y);
  char lo[8];
  char hi[8];
  char rain[8];
  formatTemp(lo, sizeof(lo), w.t_min);
  formatTemp(hi, sizeof(hi), w.t_max);
  snprintf(rain, sizeof(rain), "%d%%", w.rain_prob);

  constexpr int kIcon = 12;
  constexpr int kGap = 14;
  const int w_lo = draw::textWidth(g, Id::S17, lo);
  const int w_hi = draw::textWidth(g, Id::S17, hi);
  const int w_rain = draw::textWidth(g, Id::S17, rain);
  const int total = kIcon + w_lo + kGap + kIcon + w_hi + kGap + kIcon + w_rain;
  int x = kCx - total / 2;

  const Rgb white{255, 255, 255};
  draw::triangleMark(g, x + 5, y, false, mix(bg, Rgb{120, 190, 255}, p));
  x += kIcon;
  draw::text(g, Id::S17, lo, x, y, mix(bg, white, p), textdatum_t::middle_left);
  x += w_lo + kGap;
  draw::triangleMark(g, x + 5, y, true, mix(bg, Rgb{255, 150, 80}, p));
  x += kIcon;
  draw::text(g, Id::S17, hi, x, y, mix(bg, white, p), textdatum_t::middle_left);
  x += w_hi + kGap;
  draw::raindrop(g, x + 5, y, 12, mix(bg, Rgb{110, 180, 255}, p));
  x += kIcon;
  draw::text(g, Id::S17, rain, x, y, mix(bg, white, p), textdatum_t::middle_left);
}

void drawNoData(lgfx::LovyanGFX& g, const Model& m, uint32_t t) {
  const Sky sky{{16, 26, 48}, {40, 56, 86}};
  draw::verticalGradient(g, sky.top, sky.bottom);
  const float bob = sinf(t * 0.004f) * 3.0f;
  draw::cloud(g, kCx, 90 + bob, 1.0f, Rgb{190, 200, 215}, Rgb{120, 132, 150});
  draw::text(g, Id::S17, i18n::tr(m.wifi_ok ? i18n::S::LoadingWeather : i18n::S::NoWifi), kCx, 150,
             rgb(230, 236, 245));
  draw::pageDots(g, static_cast<int>(Page::Weather), kPageCount);
}

}  // namespace

uint32_t drawWeatherPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t) {
  const WeatherModel& w = m.weather;
  if (!w.valid) {
    drawNoData(g, m, t);
    return 40;
  }

  const Scene scene = sceneFor(w.code);
  Sky sky = skyFor(scene, w.is_day);

  // Lightning brightens the whole sky for a blink.
  bool flash = false;
  if (scene == Scene::Storm && t > 900) {
    const uint32_t c = (t - 900) % 3400;
    flash = c < 90 || (c > 170 && c < 230);
    if (flash) {
      sky.top = lerp(sky.top, Rgb{200, 200, 255}, 0.35f);
      sky.bottom = lerp(sky.bottom, Rgb{200, 200, 255}, 0.25f);
    }
  }
  draw::verticalGradient(g, sky.top, sky.bottom);

  if (!w.is_day && (scene == Scene::Clear || scene == Scene::Partly)) {
    drawStars(g, sky, t);
  }

  // ---- icon ----
  const float rise = draw::easeOutBack(draw::progress(t, 0, 800));
  const float rays = draw::easeOutCubic(draw::progress(t, 300, 800));
  const float slide = draw::easeOutBack(draw::progress(t, 150, 850));
  const float drift = sinf(t * 0.0009f) * 5.0f;
  const float rot = t * 0.012f;
  const float breathe = 1.0f + 0.08f * sinf(t * 0.003f);
  const Rgb cloud_light{232, 238, 246};
  const Rgb cloud_shade{150, 164, 184};
  const Rgb dark_light{150, 160, 178};
  const Rgb dark_shade{92, 100, 118};

  const float cloud_x = -70.0f + (kCx + 70.0f) * slide + drift;
  const bool has_cloud = scene != Scene::Clear;

  if (scene == Scene::Clear) {
    if (w.is_day) {
      draw::sun(g, kCx, kIconY + (1.0f - rise) * 40.0f, 21, rays * breathe, rot, skyAt(sky, kIconY));
    } else {
      draw::moon(g, kCx, static_cast<int>(kIconY + (1.0f - rise) * 40.0f), 20, Rgb{240, 236, 210});
    }
  } else if (scene == Scene::Partly || scene == Scene::Showers) {
    const float sy = kIconY - 10 + (1.0f - rise) * 40.0f;
    if (w.is_day) {
      draw::sun(g, kCx + 18, sy, 15, rays * breathe, rot, skyAt(sky, kIconY - 10));
    } else {
      draw::moon(g, kCx + 18, static_cast<int>(sy), 15, Rgb{240, 236, 210});
    }
  }

  if (has_cloud) {
    const bool dark = scene == Scene::Rain || scene == Scene::Storm || scene == Scene::Drizzle ||
                      scene == Scene::Showers;
    const float cx = (scene == Scene::Partly || scene == Scene::Showers) ? cloud_x - 8.0f : cloud_x;
    if (scene == Scene::Cloudy) {
      // A second, smaller cloud drifting behind for depth.
      draw::cloud(g, cx + 24 - drift * 0.6f, kIconY - 12, 0.65f, Rgb{190, 198, 212}, Rgb{130, 140, 158});
    }
    draw::cloud(g, cx, kIconY + (scene == Scene::Fog ? -6.0f : 0.0f), 1.0f,
                dark ? dark_light : cloud_light, dark ? dark_shade : cloud_shade);

    const float fall_in = draw::progress(t, 700, 500);
    switch (scene) {
      case Scene::Drizzle:
        drawRain(g, sky, cx, t, fall_in, 10, 0.05f, 4);
        break;
      case Scene::Rain:
        drawRain(g, sky, cx, t, fall_in, w.code == 65 ? 22 : 16, 0.075f, 7);
        break;
      case Scene::Showers:
        drawRain(g, sky, cx, t, fall_in, 16, 0.08f, 7);
        break;
      case Scene::Storm:
        drawRain(g, sky, cx, t, fall_in, 18, 0.09f, 7);
        if (flash) {
          drawBolt(g, static_cast<int>(cx) - 2, kIconY + 12, rgb(255, 230, 90));
        }
        break;
      case Scene::Snow:
        drawSnow(g, sky, cx, t, fall_in);
        break;
      case Scene::Fog:
        drawFogBands(g, sky, t, draw::easeOutCubic(draw::progress(t, 500, 600)));
        break;
      default:
        break;
    }
  }

  // ---- temperature (counts up on entry) ----
  const float count = draw::easeOutCubic(draw::progress(t, 200, 900));
  const float shown = w.temp_c - 12.0f * (1.0f - count);
  char temp[10];
  formatTemp(temp, sizeof(temp), shown);
  const float tp = draw::progress(t, 100, 300);
  draw::text(g, Id::Digits54, temp, kCx + 6, 114, mix(skyAt(sky, 114), Rgb{255, 255, 255}, tp));

  // ---- details ----
  fadeLine(g, sky, Id::S17, weatherLabel(w.code, w.is_day), 150, Rgb{255, 255, 255}, t, 450);
  char feels[24];
  snprintf(feels, sizeof(feels), i18n::tr(i18n::S::FeelsFmt), static_cast<int>(lroundf(w.feels_c)));
  fadeLine(g, sky, Id::S14, feels, 170, Rgb{220, 230, 245}, t, 600);
  drawStatsRow(g, sky, w, t);

  draw::pageDots(g, static_cast<int>(Page::Weather), kPageCount);
  return 40;
}

}  // namespace ui
