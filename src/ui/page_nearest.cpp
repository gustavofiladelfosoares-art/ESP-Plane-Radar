// Nearest-aircraft page: who is flying closest to home, where it is going,
// and which way to look.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/draw_util.h"
#include "ui/pages.h"

namespace ui {

namespace {

using draw::kCx;
using draw::kDegToRad;
using fonts::Id;

const Rgb kTop{10, 20, 44};
const Rgb kBottom{4, 8, 20};
const Rgb kAccent{90, 172, 255};
const Rgb kAmber{255, 196, 90};
const Rgb kWhite{255, 255, 255};
const Rgb kMuted{150, 166, 190};

Rgb bgAt(int y) { return lerp(kTop, kBottom, y / 240.0f); }

const char* compass8(float deg) {
  static const char* kDirs[] = {"N", "NE", "L", "SE", "S", "SO", "O", "NO"};
  const int i = static_cast<int>(lroundf(fmodf(deg + 360.0f, 360.0f) / 45.0f)) % 8;
  return kDirs[i];
}

/** 10668 -> "10.668" (pt-BR thousands separator). */
void groupThousands(char* out, size_t len, int v) {
  if (v >= 1000) {
    snprintf(out, len, "%d.%03d", v / 1000, v % 1000);
  } else {
    snprintf(out, len, "%d", v);
  }
}

void formatDistance(char* out, size_t len, float km) {
  if (km < 10.0f) {
    const int tenths = static_cast<int>(lroundf(km * 10.0f));
    snprintf(out, len, "%d,%d km", tenths / 10, tenths % 10);
  } else {
    snprintf(out, len, "%d km", static_cast<int>(lroundf(km)));
  }
}

/** Fade + slide-up helper for staggered text. */
float appear(uint32_t t, uint32_t start) { return draw::easeOutCubic(draw::progress(t, start, 450)); }

void line(lgfx::LovyanGFX& g, Id font, const char* s, int y, const Rgb& c, float p) {
  if (p <= 0.0f) {
    return;
  }
  const int yy = y + static_cast<int>((1.0f - p) * 8.0f);
  draw::text(g, font, s, kCx, yy, mix(bgAt(yy), c, p));
}

/** Fly-by on entry: a plane crosses the screen leaving a fading contrail. */
void flyBy(lgfx::LovyanGFX& g, uint32_t t) {
  const float p = draw::progress(t, 0, 1200);
  if (p <= 0.0f || p >= 1.0f) {
    return;
  }
  const float e = draw::easeInOutSine(p);
  const float x = -30.0f + 300.0f * e;
  const float y = 104.0f - 18.0f * sinf(e * draw::kPi);
  const float tail = 90.0f;
  const Rgb trail{200, 220, 255};
  for (int i = 0; i < 6; ++i) {
    const float k0 = i / 6.0f;
    const float k1 = (i + 1) / 6.0f;
    draw::thickLine(g, x - tail * k1, y + 6 * k1, x - tail * k0 - 8, y + 6 * k0,
                    0.6f + 1.2f * (1.0f - k1), 0.6f + 1.2f * (1.0f - k0),
                    mix(bgAt(static_cast<int>(y)), trail, (1.0f - k1) * 0.7f));
  }
  draw::airplane(g, x, y, 80.0f, 22.0f, rgb(255, 255, 255));
}

void pingRings(lgfx::LovyanGFX& g, int cx, int cy, uint32_t t) {
  for (int k = 0; k < 2; ++k) {
    const float p = fmodf(t / 1600.0f + k * 0.5f, 1.0f);
    const int r = static_cast<int>(10 + p * 14);
    g.drawCircle(cx, cy, r, mix(bgAt(cy), kAccent, (1.0f - p) * 0.8f));
  }
}

void formatRadius(char* out, size_t len, float km) {
  snprintf(out, len, "%d km", static_cast<int>(lroundf(km)));
}

/** Big "Raio: 20 km" pill for a moment after the radius changes. */
void radiusToast(lgfx::LovyanGFX& g, const Model& m) {
  constexpr uint32_t kShowMs = 1600;
  constexpr uint32_t kFadeMs = 350;
  const uint32_t ago = m.nearest_radius_changed_ago_ms;
  if (ago >= kShowMs) {
    return;
  }
  const float in = draw::easeOutBack(draw::clamp01(ago / 220.0f));
  const float out = ago > kShowMs - kFadeMs ? 1.0f - (ago - (kShowMs - kFadeMs)) / static_cast<float>(kFadeMs) : 1.0f;
  const int w = static_cast<int>(156 * (0.7f + 0.3f * in));
  constexpr int kH = 46;
  const int y = 120;
  const Rgb panel{18, 46, 92};
  const Rgb bg = bgAt(y);
  g.fillRoundRect(kCx - w / 2, y - kH / 2, w, kH, kH / 2, mix(bg, panel, out));
  g.drawRoundRect(kCx - w / 2, y - kH / 2, w, kH, kH / 2, mix(bg, kAccent, out));
  char r[16];
  formatRadius(r, sizeof(r), m.nearest_radius_km);
  char buf[24];
  snprintf(buf, sizeof(buf), "Raio: %s", r);
  draw::text(g, Id::S22, buf, kCx, y + 1, mix(panel, kWhite, out));
}

void drawEmpty(lgfx::LovyanGFX& g, const Model& m, uint32_t t) {
  constexpr int kR = 46;
  constexpr int kY = 104;
  const Rgb ring{40, 90, 150};
  for (int i = 1; i <= 3; ++i) {
    g.drawCircle(kCx, kY, kR * i / 3, rgb(ring));
  }
  g.drawFastHLine(kCx - kR, kY, kR * 2, rgb(ring));
  g.drawFastVLine(kCx, kY - kR, kR * 2, rgb(ring));
  const float a = fmodf(t * 0.12f, 360.0f);
  for (int i = 0; i < 10; ++i) {
    const float k = i / 10.0f;
    draw::pie(g, kCx, kY, kR, a - (i + 1) * 5.0f, a - i * 5.0f,
              mix(bgAt(kY), Rgb{50, 140, 255}, 0.45f * (1.0f - k)));
  }
  float x = kCx + sinf(a * kDegToRad) * kR;
  float y = kY - cosf(a * kDegToRad) * kR;
  g.drawLine(kCx, kY, static_cast<int>(x), static_cast<int>(y), rgb(120, 190, 255));
  if (!m.wifi_ok) {
    draw::text(g, Id::S17, "Sem Wi-Fi", kCx, 172, rgb(230, 238, 250));
    draw::text(g, Id::S14, "tentando conectar…", kCx, 193, rgb(kMuted));
    return;
  }
  char r[16];
  formatRadius(r, sizeof(r), m.nearest_radius_km);
  char buf[32];
  snprintf(buf, sizeof(buf), "em até %s", r);
  draw::text(g, Id::S17, "Nenhum avião", kCx, 170, rgb(230, 238, 250));
  draw::text(g, Id::S14, buf, kCx, 190, rgb(kMuted));
  // Gentle blink so the hint gets noticed.
  const float blink = 0.55f + 0.45f * sinf(t * 0.004f);
  draw::text(g, Id::S14, "2 toques: mudar raio", kCx, 209, mix(bgAt(209), kAccent, blink));
}

}  // namespace

int nearestPlane(const Model& m, float* dist_km, float* bearing_deg) {
  int best = -1;
  float best_d = 1e9f;
  float best_b = 0.0f;
  const float cos_lat = cosf(static_cast<float>(m.lat) * kDegToRad);
  for (size_t i = 0; i < m.plane_count; ++i) {
    const auto& p = m.planes[i];
    if (p.on_ground) {
      continue;
    }
    const float dx = static_cast<float>(p.lon - m.lon) * 111.0f * cos_lat;
    const float dy = static_cast<float>(p.lat - m.lat) * 111.0f;
    const float d = sqrtf(dx * dx + dy * dy);
    if (d > m.nearest_radius_km) {
      continue;
    }
    if (d < best_d) {
      best_d = d;
      best = static_cast<int>(i);
      best_b = atan2f(dx, dy) / kDegToRad;
    }
  }
  if (best >= 0) {
    *dist_km = best_d;
    *bearing_deg = fmodf(best_b + 360.0f, 360.0f);
  }
  return best;
}

uint32_t drawNearestPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t) {
  draw::verticalGradient(g, kTop, kBottom);

  float dist = 0.0f;
  float bearing = 0.0f;
  const int idx = nearestPlane(m, &dist, &bearing);

  if (idx < 0) {
    drawEmpty(g, m, t);
    draw::pageDots(g, static_cast<int>(Page::Nearest), kPageCount);
    radiusToast(g, m);
    return 40;
  }

  const services::adsb::Aircraft& p = m.planes[idx];
  const bool route = m.route.valid && strcmp(m.route.callsign, p.callsign) == 0;

  line(g, Id::S14, "MAIS PRÓXIMO", 30, kAccent, appear(t, 250));
  line(g, Id::S28, p.callsign[0] ? p.callsign : "—", 56, kWhite, appear(t, 350));

  char buf[48];
  const char* subtitle = route && m.route.airline[0] ? m.route.airline : p.desc;
  draw::fitText(g, Id::S14, subtitle, 170, buf, sizeof(buf));
  line(g, Id::S14, buf, 82, kAmber, appear(t, 450));

  const float pr = appear(t, 550);
  if (route && pr > 0.0f) {
    // "CGH ✈ GYN" with a drawn plane between the airport codes.
    const int yy = 112 + static_cast<int>((1.0f - pr) * 8.0f);
    const int wf = draw::textWidth(g, Id::S22, m.route.from_iata);
    const int wt = draw::textWidth(g, Id::S22, m.route.to_iata);
    constexpr int kMid = 34;
    const int x0 = kCx - (wf + kMid + wt) / 2;
    const uint16_t c = mix(bgAt(yy), Rgb{215, 232, 255}, pr);
    draw::text(g, Id::S22, m.route.from_iata, x0, yy, c, textdatum_t::middle_left);
    draw::text(g, Id::S22, m.route.to_iata, x0 + wf + kMid, yy, c, textdatum_t::middle_left);
    const int px = x0 + wf + kMid / 2;
    const Rgb dash = lerp(bgAt(yy), kAccent, pr * 0.6f);
    for (int dx = -13; dx <= 13; dx += 5) {
      g.drawFastHLine(px + dx - 1, yy, 2, rgb(dash));
    }
    // The little plane glides back and forth along the route.
    const float glide = sinf(t * 0.002f) * 5.0f;
    draw::airplane(g, px + glide, yy, 90.0f, 16.0f, mix(bgAt(yy), kAmber, pr));

    char cities[64];
    snprintf(cities, sizeof(cities), "%s — %s", m.route.from_city, m.route.to_city);
    draw::fitText(g, Id::S14, cities, 204, buf, sizeof(buf));
    line(g, Id::S14, buf, 134, kMuted, appear(t, 650));
  } else {
    const char* model = route && m.route.airline[0] ? p.desc : p.type;
    draw::fitText(g, Id::S17, model[0] ? model : "Aeronave", 176, buf, sizeof(buf));
    line(g, Id::S17, buf, 112, Rgb{215, 232, 255}, pr);
    if (p.reg[0]) {
      line(g, Id::S14, p.reg, 134, kMuted, appear(t, 650));
    }
  }

  // Which way to look: arrow toward the plane + distance.
  const float pd = appear(t, 750);
  if (pd > 0.0f) {
    char d[16];
    formatDistance(d, sizeof(d), dist);
    snprintf(buf, sizeof(buf), "%s a %s", d, compass8(bearing));
    const int yy = 164 + static_cast<int>((1.0f - pd) * 8.0f);
    const int tw = draw::textWidth(g, Id::S17, buf);
    const int ax = kCx - (tw + 30) / 2 + 10;
    pingRings(g, ax, yy, t);
    g.fillSmoothCircle(ax, yy, 10, mix(bgAt(yy), Rgb{24, 52, 96}, pd));
    // Arrow eases toward the bearing on entry.
    const float swing = draw::easeOutBack(draw::progress(t, 750, 900));
    draw::arrow(g, ax, yy, 8.0f, bearing * swing, mix(bgAt(yy), Rgb{120, 200, 255}, pd));
    draw::text(g, Id::S17, buf, ax + 20, yy, mix(bgAt(yy), kWhite, pd), textdatum_t::middle_left);
  }

  char alt[20];
  if (p.on_ground) {
    snprintf(alt, sizeof(alt), "No solo");
  } else if (p.has_alt) {
    char n[12];
    groupThousands(n, sizeof(n), static_cast<int>(lroundf(p.alt_ft * 0.3048f)));
    snprintf(alt, sizeof(alt), "%s m", n);
  } else {
    snprintf(alt, sizeof(alt), "— m");
  }
  snprintf(buf, sizeof(buf), "%s  •  %d km/h", alt, static_cast<int>(lroundf(p.gs_knots * 1.852f)));
  line(g, Id::S17, buf, 189, Rgb{200, 214, 236}, appear(t, 850));

  char r[16];
  formatRadius(r, sizeof(r), m.nearest_radius_km);
  snprintf(buf, sizeof(buf), "raio %s", r);
  line(g, Id::S14, buf, 209, kMuted, appear(t, 950));

  flyBy(g, t);
  draw::pageDots(g, static_cast<int>(Page::Nearest), kPageCount);
  radiusToast(g, m);
  return 40;
}

}  // namespace ui
