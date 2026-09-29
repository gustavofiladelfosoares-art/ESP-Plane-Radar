// About page: animated title card — "PLANE RADAR, made by Gustavo Soares".

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/draw_util.h"
#include "ui/pages.h"

namespace ui {

namespace {

using draw::kCx;
using draw::kCy;
using draw::kDegToRad;
using fonts::Id;

const Rgb kTop{6, 10, 32};
const Rgb kBottom{0, 1, 8};
const Rgb kRing{28, 58, 108};
const Rgb kBlue{120, 190, 255};
const Rgb kGold{255, 204, 110};
const Rgb kMuted{150, 166, 190};
const Rgb kDim{96, 112, 138};

constexpr char kAuthor[] = "Gustavo Soares";
constexpr int kOrbitR = 96;

Rgb bgAt(int y) { return lerp(kTop, kBottom, y / 240.0f); }

float hash01(uint32_t i) {
  i ^= i >> 16;
  i *= 0x7feb352dU;
  i ^= i >> 15;
  i *= 0x846ca68bU;
  i ^= i >> 16;
  return static_cast<float>(i & 0xFFFF) / 65535.0f;
}

void stars(lgfx::LovyanGFX& g, uint32_t t) {
  for (int i = 0; i < 40; ++i) {
    const int x = static_cast<int>(hash01(i * 7 + 3) * 240);
    const int y = static_cast<int>(hash01(i * 13 + 5) * 240);
    if ((x - kCx) * (x - kCx) + (y - kCy) * (y - kCy) > 116 * 116) continue;
    const float tw = 0.5f + 0.5f * sinf(t * 0.003f + hash01(i) * 6.28f);
    g.drawPixel(x, y, mix(bgAt(y), Rgb{230, 236, 255}, 0.25f + 0.6f * tw));
  }
}

void rings(lgfx::LovyanGFX& g, uint32_t t) {
  const float grow = draw::easeOutCubic(draw::progress(t, 0, 700));
  for (int r : {58, kOrbitR, 112}) {
    const int rr = static_cast<int>(r * grow);
    if (rr > 2) g.drawCircle(kCx, kCy, rr, rgb(kRing));
  }
}

/** Plane circling the middle ring with a fading dotted contrail. */
void orbit(lgfx::LovyanGFX& g, uint32_t t) {
  const float appear = draw::progress(t, 500, 400);
  if (appear <= 0.0f) return;
  const float a = fmodf(t * 0.06f, 360.0f);  // one lap every 6 s
  for (int k = 1; k <= 14; ++k) {
    const float ta = (a - k * 4.0f) * kDegToRad;
    const int x = kCx + static_cast<int>(sinf(ta) * kOrbitR);
    const int y = kCy - static_cast<int>(cosf(ta) * kOrbitR);
    g.fillCircle(x, y, 1, mix(bgAt(y), Rgb{170, 210, 255}, appear * (1.0f - k / 15.0f)));
  }
  const float ar = a * kDegToRad;
  const float x = kCx + sinf(ar) * kOrbitR;
  const float y = kCy - cosf(ar) * kOrbitR;
  draw::airplane(g, x, y, a + 90.0f, 16.0f, mix(bgAt(static_cast<int>(y)), Rgb{255, 150, 50}, appear));
}

void fadeText(lgfx::LovyanGFX& g, Id font, const char* s, int y, const Rgb& c, uint32_t t,
              uint32_t start) {
  const float p = draw::easeOutCubic(draw::progress(t, start, 500));
  if (p <= 0.0f) return;
  const int yy = y + static_cast<int>((1.0f - p) * 8.0f);
  draw::text(g, font, s, kCx, yy, mix(bgAt(yy), c, p));
}

/** Name typed letter by letter, then a soft shimmer sweeping across it. */
void author(lgfx::LovyanGFX& g, uint32_t t) {
  constexpr uint32_t kStart = 1000;
  constexpr uint32_t kPerChar = 75;
  if (t < kStart) return;
  const int total = static_cast<int>(strlen(kAuthor));
  const int shown = std::min(total, static_cast<int>((t - kStart) / kPerChar) + 1);

  fonts::use(g, Id::S22);
  const int full_w = g.textWidth(kAuthor);
  int x = kCx - full_w / 2;
  constexpr int kY = 142;
  g.setTextDatum(textdatum_t::middle_left);
  const float sweep = fmodf((t - kStart) / 2200.0f, 1.0f) * (total + 6) - 3;  // char index
  char one[2] = {0, 0};
  for (int i = 0; i < shown; ++i) {
    one[0] = kAuthor[i];
    const float d = fabsf(i - sweep);
    const float glow = d < 2.5f ? (1.0f - d / 2.5f) * 0.6f : 0.0f;
    g.setTextColor(mix(kGold, Rgb{255, 250, 225}, glow));
    g.drawString(one, x, kY);
    x += g.textWidth(one);
  }
  // Blinking cursor while typing.
  if (shown < total && ((t / 250) & 1) == 0) {
    g.fillRect(x + 1, kY - 9, 2, 18, rgb(kGold));
  }
}

}  // namespace

uint32_t drawAboutPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t) {
  (void)m;
  draw::verticalGradient(g, kTop, kBottom);
  stars(g, t);
  rings(g, t);
  orbit(g, t);

  fadeText(g, Id::S22, "PLANE RADAR", 94, kBlue, t, 250);
  fadeText(g, Id::S14, "made by", 119, kMuted, t, 650);
  author(g, t);
  fadeText(g, Id::S14, "v2.1.0 • 2026", 176, kDim, t, 2200);
  fadeText(g, Id::S14, "base: MatixYo • MIT", 196, kDim, t, 2400);

  draw::pageDots(g, static_cast<int>(Page::About), kPageCount);
  return 40;
}

}  // namespace ui
