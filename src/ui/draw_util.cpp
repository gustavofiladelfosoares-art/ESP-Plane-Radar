#include "ui/draw_util.h"

#include <cmath>
#include <cstring>

namespace ui::draw {

float clamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }

float progress(uint32_t t_ms, uint32_t start_ms, uint32_t dur_ms) {
  if (t_ms <= start_ms) {
    return 0.0f;
  }
  if (dur_ms == 0) {
    return 1.0f;
  }
  return clamp01(static_cast<float>(t_ms - start_ms) / static_cast<float>(dur_ms));
}

float easeOutCubic(float t) {
  const float u = 1.0f - clamp01(t);
  return 1.0f - u * u * u;
}

float easeInOutSine(float t) { return 0.5f - 0.5f * cosf(kPi * clamp01(t)); }

float easeOutBack(float t) {
  constexpr float c1 = 1.70158f;
  constexpr float c3 = c1 + 1.0f;
  const float u = clamp01(t) - 1.0f;
  return 1.0f + c3 * u * u * u + c1 * u * u;
}

void verticalGradient(lgfx::LovyanGFX& g, const Rgb& top, const Rgb& bottom) {
  g.fillGradientRect(0, 0, kSize, kSize, rgb888(top), rgb888(bottom),
                     lgfx::VLINEAR);
}

void radialGradient(lgfx::LovyanGFX& g, const Rgb& center, const Rgb& edge) {
  g.fillScreen(rgb(edge));
  constexpr int kSteps = 16;
  for (int i = 0; i < kSteps; ++i) {
    const float t = static_cast<float>(i) / (kSteps - 1);
    const int r = static_cast<int>(kCx * (1.0f - t)) + 2;
    g.fillCircle(kCx, kCy, r, mix(edge, center, t));
  }
}

void text(lgfx::LovyanGFX& g, fonts::Id font, const char* s, int x, int y,
          uint16_t color, textdatum_t datum) {
  fonts::use(g, font);
  g.setTextDatum(datum);
  g.setTextColor(color);
  g.drawString(s, x, y);
}

int textWidth(lgfx::LovyanGFX& g, fonts::Id font, const char* s) {
  fonts::use(g, font);
  return g.textWidth(s);
}

void fitText(lgfx::LovyanGFX& g, fonts::Id font, const char* in, int max_w,
             char* out, size_t out_len) {
  if (out_len == 0) {
    return;
  }
  strncpy(out, in, out_len - 1);
  out[out_len - 1] = '\0';
  fonts::use(g, font);
  if (g.textWidth(out) <= max_w) {
    return;
  }
  // Trim whole UTF-8 characters from the end until "text…" fits.
  size_t n = strlen(out);
  while (n > 0) {
    --n;
    while (n > 0 && (static_cast<uint8_t>(out[n]) & 0xC0) == 0x80) {
      --n;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*s…", static_cast<int>(n), out);
    if (g.textWidth(buf) <= max_w || n == 0) {
      strncpy(out, buf, out_len - 1);
      out[out_len - 1] = '\0';
      return;
    }
  }
}

void pageDots(lgfx::LovyanGFX& g, int index, int count) {
  constexpr int kY = 227;
  constexpr int kGap = 11;
  const int x0 = kCx - (count - 1) * kGap / 2;
  for (int i = 0; i < count; ++i) {
    const int x = x0 + i * kGap;
    if (i == index) {
      g.fillSmoothCircle(x, kY, 3, rgb(255, 255, 255));
    } else {
      g.fillSmoothCircle(x, kY, 2, rgb(120, 130, 150));
    }
  }
}

// ---- fast shapes ----

namespace {

void polarPt(int cx, int cy, float r, float deg, int* x, int* y) {
  const float a = deg * kDegToRad;
  *x = cx + static_cast<int>(lroundf(sinf(a) * r));
  *y = cy - static_cast<int>(lroundf(cosf(a) * r));
}

int stepsFor(float a0, float a1, float max_step) {
  const int n = static_cast<int>(ceilf(fabsf(a1 - a0) / max_step));
  return n < 1 ? 1 : n;
}

}  // namespace

void pie(lgfx::LovyanGFX& g, int cx, int cy, int r, float a0, float a1, uint16_t color) {
  const int n = stepsFor(a0, a1, 6.0f);
  int px = 0;
  int py = 0;
  polarPt(cx, cy, r, a0, &px, &py);
  for (int i = 1; i <= n; ++i) {
    int x = 0;
    int y = 0;
    polarPt(cx, cy, r, a0 + (a1 - a0) * i / n, &x, &y);
    g.fillTriangle(cx, cy, px, py, x, y, color);
    px = x;
    py = y;
  }
}

void ring(lgfx::LovyanGFX& g, int cx, int cy, int r_in, int r_out, float a0, float a1,
          uint16_t color) {
  const int n = stepsFor(a0, a1, 5.0f);
  int ix0 = 0, iy0 = 0, ox0 = 0, oy0 = 0;
  polarPt(cx, cy, r_in, a0, &ix0, &iy0);
  polarPt(cx, cy, r_out, a0, &ox0, &oy0);
  for (int i = 1; i <= n; ++i) {
    const float a = a0 + (a1 - a0) * i / n;
    int ix1 = 0, iy1 = 0, ox1 = 0, oy1 = 0;
    polarPt(cx, cy, r_in, a, &ix1, &iy1);
    polarPt(cx, cy, r_out, a, &ox1, &oy1);
    g.fillTriangle(ix0, iy0, ox0, oy0, ox1, oy1, color);
    g.fillTriangle(ix0, iy0, ox1, oy1, ix1, iy1, color);
    ix0 = ix1;
    iy0 = iy1;
    ox0 = ox1;
    oy0 = oy1;
  }
}

void thickLine(lgfx::LovyanGFX& g, float x0, float y0, float x1, float y1, float r0, float r1,
               uint16_t color) {
  if (r0 <= 0.75f && r1 <= 0.75f) {
    g.drawLine(static_cast<int>(lroundf(x0)), static_cast<int>(lroundf(y0)),
               static_cast<int>(lroundf(x1)), static_cast<int>(lroundf(y1)), color);
    return;
  }
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.5f) {
    g.fillCircle(static_cast<int>(lroundf(x0)), static_cast<int>(lroundf(y0)),
                 static_cast<int>(lroundf(fmaxf(r0, r1))), color);
    return;
  }
  const float nx = -dy / len;
  const float ny = dx / len;
  const int ax = static_cast<int>(lroundf(x0 + nx * r0));
  const int ay = static_cast<int>(lroundf(y0 + ny * r0));
  const int bx = static_cast<int>(lroundf(x0 - nx * r0));
  const int by = static_cast<int>(lroundf(y0 - ny * r0));
  const int cx = static_cast<int>(lroundf(x1 + nx * r1));
  const int cy = static_cast<int>(lroundf(y1 + ny * r1));
  const int ex = static_cast<int>(lroundf(x1 - nx * r1));
  const int ey = static_cast<int>(lroundf(y1 - ny * r1));
  g.fillTriangle(ax, ay, bx, by, cx, cy, color);
  g.fillTriangle(bx, by, ex, ey, cx, cy, color);
  if (r0 >= 1.5f) g.fillCircle(static_cast<int>(lroundf(x0)), static_cast<int>(lroundf(y0)), static_cast<int>(r0), color);
  if (r1 >= 1.5f) g.fillCircle(static_cast<int>(lroundf(x1)), static_cast<int>(lroundf(y1)), static_cast<int>(r1), color);
}

// ---- icons ----

void sun(lgfx::LovyanGFX& g, float cx, float cy, float r, float ray_scale,
         float rot_deg, const Rgb& sky) {
  const Rgb glow{255, 190, 60};
  // Soft halo: widening rings that fade into the sky color.
  constexpr int kHalo = 6;
  for (int i = 0; i < kHalo; ++i) {
    const float k = static_cast<float>(kHalo - i) / kHalo;  // 1 = outermost
    const float rr = r * (1.0f + 0.9f * k * ray_scale);
    g.fillCircle(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(rr),
                 mix(sky, glow, 0.10f + 0.10f * (1.0f - k)));
  }

  if (ray_scale > 0.01f) {
    const uint16_t ray = rgb(255, 196, 50);
    for (int i = 0; i < 12; ++i) {
      const float a = (rot_deg + i * 30.0f) * kDegToRad;
      const float len = r * (i % 2 == 0 ? 0.75f : 0.5f) * ray_scale;
      const float r0 = r + 4.0f;
      const float x0 = cx + sinf(a) * r0;
      const float y0 = cy - cosf(a) * r0;
      const float x1 = cx + sinf(a) * (r0 + len);
      const float y1 = cy - cosf(a) * (r0 + len);
      thickLine(g, x0, y0, x1, y1, 1.6f, 0.6f, ray);
    }
  }

  g.fillSmoothCircle(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(r),
                     rgb(255, 184, 28));
  g.fillSmoothCircle(static_cast<int>(cx - r * 0.18f), static_cast<int>(cy - r * 0.18f),
                     static_cast<int>(r * 0.72f), rgb(255, 208, 70));
  g.fillSmoothCircle(static_cast<int>(cx - r * 0.3f), static_cast<int>(cy - r * 0.3f),
                     static_cast<int>(r * 0.32f), rgb(255, 234, 150));
}

namespace {

struct Puff {
  float dx;
  float dy;
  float r;
};

// Cloud silhouette at scale 1.0 (about 64 x 38 px).
constexpr Puff kPuffs[] = {
    {-19.0f, 5.0f, 12.0f}, {-6.0f, -6.0f, 15.0f}, {10.0f, -2.0f, 13.0f},
    {21.0f, 6.0f, 10.0f},  {2.0f, 7.0f, 12.0f},
};

void cloudShape(lgfx::LovyanGFX& g, float cx, float cy, float s, uint16_t color, bool smooth) {
  for (const Puff& p : kPuffs) {
    const int x = static_cast<int>(cx + p.dx * s);
    const int y = static_cast<int>(cy + p.dy * s);
    const int r = static_cast<int>(p.r * s);
    if (smooth) {
      g.fillSmoothCircle(x, y, r, color);
    } else {
      g.fillCircle(x, y, r, color);
    }
  }
  g.fillRoundRect(static_cast<int>(cx - 30 * s), static_cast<int>(cy + 3 * s),
                  static_cast<int>(60 * s), static_cast<int>(13 * s), static_cast<int>(6 * s), color);
}

}  // namespace

void cloud(lgfx::LovyanGFX& g, float cx, float cy, float scale, const Rgb& light,
           const Rgb& shade) {
  cloudShape(g, cx, cy + 3.0f * scale, scale, rgb(shade), false);
  cloudShape(g, cx, cy, scale, rgb(light), true);
  // Highlight on the top puff for a little volume.
  const Rgb hi = lerp(light, Rgb{255, 255, 255}, 0.55f);
  g.fillSmoothCircle(static_cast<int>(cx - 9 * scale), static_cast<int>(cy - 10 * scale),
                     static_cast<int>(7 * scale), rgb(hi));
}

void moon(lgfx::LovyanGFX& g, int cx, int cy, int r, const Rgb& color) {
  // Crescent: moon disc minus a shadow disc offset to the upper left.
  const float sx = cx - r * 0.55f;
  const float sy = cy - r * 0.25f;
  const float sr = r * 0.95f;
  const uint16_t c = rgb(color);
  for (int dy = -r; dy <= r; ++dy) {
    const float y = static_cast<float>(cy + dy);
    const float w1 = sqrtf(static_cast<float>(r * r - dy * dy));
    float left = cx - w1;
    const float right = cx + w1;
    const float ry = y - sy;
    if (fabsf(ry) < sr) {
      const float w2 = sqrtf(sr * sr - ry * ry);
      left = fmaxf(left, sx + w2);
    }
    if (right > left) {
      g.drawFastHLine(static_cast<int>(lroundf(left)), static_cast<int>(y),
                      static_cast<int>(lroundf(right - left)), c);
    }
  }
}

void raindrop(lgfx::LovyanGFX& g, int cx, int cy, int h, uint16_t color) {
  const int r = h / 3;
  const int by = cy + h / 2 - r;
  g.fillSmoothCircle(cx, by, r, color);
  g.fillTriangle(cx, cy - h / 2, cx - r + 1, by - 1, cx + r - 1, by - 1, color);
}

namespace {

struct Pt {
  float x;
  float y;
};

// Airplane parts in local units (nose toward -y); each part is convex.
constexpr Pt kFuselage[] = {{0.0f, -8.5f}, {1.3f, -6.0f}, {1.1f, 7.0f}, {-1.1f, 7.0f}, {-1.3f, -6.0f}};
constexpr Pt kWingR[] = {{1.0f, -2.6f}, {8.6f, 1.6f}, {8.6f, 3.0f}, {1.0f, 1.2f}};
constexpr Pt kWingL[] = {{-1.0f, -2.6f}, {-1.0f, 1.2f}, {-8.6f, 3.0f}, {-8.6f, 1.6f}};
constexpr Pt kTailR[] = {{0.8f, 5.0f}, {3.8f, 7.6f}, {3.8f, 8.6f}, {0.8f, 7.8f}};
constexpr Pt kTailL[] = {{-0.8f, 5.0f}, {-0.8f, 7.8f}, {-3.8f, 8.6f}, {-3.8f, 7.6f}};

void fillConvex(lgfx::LovyanGFX& g, const Pt* pts, size_t n, float cx, float cy,
                float s, float sin_h, float cos_h, uint16_t color) {
  int xs[8];
  int ys[8];
  for (size_t i = 0; i < n; ++i) {
    const float x = pts[i].x * s;
    const float y = pts[i].y * s;
    xs[i] = static_cast<int>(lroundf(cx + x * cos_h - y * sin_h));
    ys[i] = static_cast<int>(lroundf(cy + x * sin_h + y * cos_h));
  }
  for (size_t i = 1; i + 1 < n; ++i) {
    g.fillTriangle(xs[0], ys[0], xs[i], ys[i], xs[i + 1], ys[i + 1], color);
  }
}

}  // namespace

void airplane(lgfx::LovyanGFX& g, float cx, float cy, float heading_deg, float size,
              uint16_t color) {
  const float a = heading_deg * kDegToRad;
  const float sh = sinf(a);
  const float ch = cosf(a);
  const float s = size / 17.0f;
  fillConvex(g, kWingR, 4, cx, cy, s, sh, ch, color);
  fillConvex(g, kWingL, 4, cx, cy, s, sh, ch, color);
  fillConvex(g, kTailR, 4, cx, cy, s, sh, ch, color);
  fillConvex(g, kTailL, 4, cx, cy, s, sh, ch, color);
  fillConvex(g, kFuselage, 5, cx, cy, s, sh, ch, color);
}

void arrow(lgfx::LovyanGFX& g, float cx, float cy, float r, float deg, uint16_t color) {
  const float a = deg * kDegToRad;
  const float sx = sinf(a);
  const float cy_ = cosf(a);
  const float tip_x = cx + sx * r;
  const float tip_y = cy - cy_ * r;
  const float tail_x = cx - sx * r * 0.7f;
  const float tail_y = cy + cy_ * r * 0.7f;
  thickLine(g, tail_x, tail_y, cx + sx * r * 0.2f, cy - cy_ * r * 0.2f, 1.2f, 1.2f, color);
  const float bx = cx + sx * r * 0.15f;
  const float by = cy - cy_ * r * 0.15f;
  const float px = cy_ * r * 0.5f;
  const float py = sx * r * 0.5f;
  g.fillTriangle(static_cast<int>(tip_x), static_cast<int>(tip_y), static_cast<int>(bx + px),
                 static_cast<int>(by + py), static_cast<int>(bx - px), static_cast<int>(by - py),
                 color);
}

void triangleMark(lgfx::LovyanGFX& g, int cx, int cy, bool up, uint16_t color) {
  if (up) {
    g.fillTriangle(cx, cy - 4, cx - 5, cy + 4, cx + 5, cy + 4, color);
  } else {
    g.fillTriangle(cx, cy + 4, cx - 5, cy - 4, cx + 5, cy - 4, color);
  }
}

}  // namespace ui::draw
