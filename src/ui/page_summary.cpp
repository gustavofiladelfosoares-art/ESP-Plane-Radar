// "Today in the sky": how many aircraft the device has seen since midnight,
// plus the highest, fastest and closest one and the most seen airline.

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

const Rgb kTop{24, 16, 52};
const Rgb kBottom{6, 6, 22};
const Rgb kAccent{190, 150, 255};
const Rgb kWhite{240, 240, 252};
const Rgb kMuted{140, 136, 172};

Rgb bgAt(int y) { return lerp(kTop, kBottom, y / 240.0f); }

float appear(uint32_t t, uint32_t start, uint32_t dur = 450) {
  return draw::easeOutCubic(draw::progress(t, start, dur));
}

void groupThousands(char* out, size_t len, int v) {
  if (v >= 1000) {
    snprintf(out, len, "%d%c%03d", v / 1000, i18n::thousandsSep(), v % 1000);
  } else {
    snprintf(out, len, "%d", v);
  }
}

/** One stat tile: small label, big value, callsign underneath. */
void tile(lgfx::LovyanGFX& g, int cx, int y, const char* label, const char* value,
          const char* sub, const Rgb& color, float p) {
  if (p <= 0.0f) return;
  const int dy = static_cast<int>((1.0f - p) * 8.0f);
  draw::text(g, Id::S14, label, cx, y + dy, mix(bgAt(y), kMuted, p));
  char buf[24];
  draw::fitText(g, Id::S17, value, 86, buf, sizeof(buf));
  draw::text(g, Id::S17, buf, cx, y + 18 + dy, mix(bgAt(y + 18), kWhite, p));
  if (sub != nullptr && sub[0] != '\0') {
    draw::text(g, Id::S14, sub, cx, y + 35 + dy, mix(bgAt(y + 35), color, p));
  }
}

}  // namespace

uint32_t drawSummaryPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t) {
  draw::verticalGradient(g, kTop, kBottom);
  const SummaryModel& s = m.summary;
  draw::text(g, Id::S14, i18n::tr(S::SkyToday), kCx, 30, mix(bgAt(30), kAccent, appear(t, 0)));

  if (s.seen == 0) {
    // Little plane circling while the first lists come in.
    const float a = t * 0.0025f;
    draw::airplane(g, kCx + cosf(a) * 26.0f, 112 + sinf(a) * 26.0f, a / draw::kDegToRad + 180.0f,
                   18.0f, rgb(kAccent));
    draw::text(g, Id::S17, i18n::tr(m.wifi_ok ? S::NoSkyYet : S::NoWifi), kCx, 168, rgb(kMuted));
    draw::pageDots(g, static_cast<int>(Page::Summary), kPageCount);
    return 40;
  }

  // Big counter rolling up to today's total.
  const float pc = appear(t, 100, 1200);
  char buf[32];
  groupThousands(buf, sizeof(buf), static_cast<int>(lroundf(s.seen * pc)));
  draw::text(g, Id::Digits54, buf, kCx, 64, mix(bgAt(66), kWhite, std::min(1.0f, pc * 2.0f)));
  draw::text(g, Id::S14, i18n::tr(S::AircraftSeen), kCx, 94, mix(bgAt(94), kMuted, appear(t, 300)));

  constexpr int kLeft = kCx - 46;
  constexpr int kRight = kCx + 46;
  char value[24];
  char n[12];
  if (s.highest_cs[0]) {
    groupThousands(n, sizeof(n), static_cast<int>(lroundf(s.highest_ft * 0.3048f)));
    snprintf(value, sizeof(value), "%s m", n);
    tile(g, kLeft, 113, i18n::tr(S::Highest), value, s.highest_cs, Rgb{255, 214, 120}, appear(t, 450));
  }
  if (s.fastest_cs[0]) {
    snprintf(value, sizeof(value), "%d km/h", s.fastest_kmh);
    tile(g, kRight, 113, i18n::tr(S::Fastest), value, s.fastest_cs, Rgb{150, 228, 170}, appear(t, 550));
  }
  if (s.closest_cs[0]) {
    const int tenths = static_cast<int>(lroundf(s.closest_km * 10.0f));
    if (tenths < 100) {
      snprintf(value, sizeof(value), "%d%c%d km", tenths / 10, i18n::decimalSep(), tenths % 10);
    } else {
      snprintf(value, sizeof(value), "%d km", tenths / 10);
    }
    tile(g, kLeft, 163, i18n::tr(S::Closest), value, s.closest_cs, Rgb{120, 200, 255}, appear(t, 650));
  }
  if (s.airline[0]) {
    snprintf(n, sizeof(n), i18n::tr(S::FlightsFmt), s.airline_count);
    tile(g, kRight, 163, i18n::tr(S::MostSeen), s.airline, n, kAccent, appear(t, 750));
  }
  // Thin cross between the four tiles.
  const float pl = appear(t, 400);
  g.drawFastVLine(kCx, 111, 92, mix(bgAt(160), kAccent, 0.25f * pl));
  g.drawFastHLine(kCx - 80, 155, 160, mix(bgAt(155), kAccent, 0.25f * pl));

  draw::pageDots(g, static_cast<int>(Page::Summary), kPageCount);
  return 40;
}

}  // namespace ui
