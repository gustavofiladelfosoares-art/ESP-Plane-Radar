// Radar page: map underlay, rotating sweep, rings, bezel ticks and aircraft.

#include <lgfx/v1/lgfx_fonts.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "hardware/display.h"
#include "hardware/display_font.h"
#include "services/adsb_client.h"
#include "services/radar_location.h"
#include "ui/color.h"
#include "ui/draw_util.h"
#include "ui/i18n.h"
#include "ui/pages.h"
#include "ui/radar_map.h"
#include "ui/radar_range.h"
#include "ui/radar_theme.h"
#include "ui/runway_overlay.h"

namespace ui {
namespace radar {

uint16_t kColorBackground = 0x0000;
uint16_t kColorGrid = 0x0320;
uint16_t kColorLabel = 0xFFFF;
uint16_t kColorCenter = 0xFFFF;
uint16_t kColorAircraft = 0x001F;
uint16_t kColorTrackVector = 0xFFFF;
uint16_t kColorTagType = 0x5DFF;
uint16_t kColorTagAltitude = 0xFFE0;
uint16_t kColorRunway = 0x4D5F;
uint16_t kColorRunwayLabel = 0x7DFF;

}  // namespace radar

namespace {

using services::adsb::Aircraft;

bool s_label_metrics_ready = false;
bool s_cardinal_use_vlw = false;
bool s_scale_use_vlw = false;
float s_cardinal_vlw_size = 0.56f;
float s_scale_vlw_size = 0.50f;
float s_tag_vlw_size = 0.56f;
const lgfx::GFXfont* s_cardinal_gfx = &lgfx::fonts::FreeSansBold12pt7b;
const lgfx::GFXfont* s_scale_gfx = &lgfx::fonts::FreeSansBold9pt7b;
const lgfx::GFXfont* s_tag_gfx = &lgfx::fonts::FreeSansBold12pt7b;

bool s_tag_label_metrics_ready = false;
bool s_tag_use_vlw = false;

lgfx::LovyanGFX* s_draw = &tft;
const Model* s_model = nullptr;  // frame being drawn (for route lookups)

const Rgb kBg{radar::kBgR, radar::kBgG, radar::kBgB};
const Rgb kSweep{70, 150, 255};

int absDiff(int a, int b) { return std::abs(a - b); }

int measureGfxHeight(const lgfx::GFXfont& font) {
  tft.setFont(&font);
  tft.setTextSize(1);
  return tft.fontHeight();
}

int measureVlwHeight(float size) {
  displayFontEnsureLoaded(tft);
  tft.setTextSize(size);
  return tft.fontHeight();
}

float findVlwSizeForHeight(int target_px) {
  float lo = 0.25f;
  float hi = 1.2f;
  for (int i = 0; i < 16; ++i) {
    const float mid = (lo + hi) * 0.5f;
    if (measureVlwHeight(mid) < target_px) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return hi;
}

const lgfx::GFXfont* pickGfxFontClosest(int target_px, const lgfx::GFXfont* const* candidates,
                                        size_t count) {
  const lgfx::GFXfont* best = candidates[0];
  int best_diff = absDiff(measureGfxHeight(*best), target_px);
  for (size_t i = 1; i < count; ++i) {
    const int diff = absDiff(measureGfxHeight(*candidates[i]), target_px);
    if (diff < best_diff) {
      best_diff = diff;
      best = candidates[i];
    }
  }
  return best;
}

void initLabelMetrics() {
  if (s_label_metrics_ready) {
    return;
  }
  const int cardinal_target = radar::kCardinalLabelHeightPx;
  if (displayFontIsSmooth()) {
    s_cardinal_use_vlw = true;
    s_cardinal_vlw_size = findVlwSizeForHeight(cardinal_target);
    const int cardinal_h = measureVlwHeight(s_cardinal_vlw_size);
    s_scale_use_vlw = true;
    s_scale_vlw_size = findVlwSizeForHeight(cardinal_h - radar::kScaleBelowCardinalPx);
  } else {
    const lgfx::GFXfont* cardinal_candidates[] = {&lgfx::fonts::FreeSansBold12pt7b,
                                                  &lgfx::fonts::FreeSansBold9pt7b};
    s_cardinal_gfx = pickGfxFontClosest(cardinal_target, cardinal_candidates, 2);
    const int cardinal_h = measureGfxHeight(*s_cardinal_gfx);
    const lgfx::GFXfont* scale_candidates[] = {&lgfx::fonts::FreeSansBold9pt7b,
                                               &lgfx::fonts::FreeSansBold12pt7b};
    s_scale_gfx = pickGfxFontClosest(cardinal_h - radar::kScaleBelowCardinalPx, scale_candidates, 2);
  }
  s_label_metrics_ready = true;
}

void initTagLabelMetrics() {
  if (s_tag_label_metrics_ready) {
    return;
  }
  const int target = radar::kAircraftTagLabelHeightPx;
  if (displayFontIsSmooth()) {
    s_tag_use_vlw = true;
    s_tag_vlw_size = findVlwSizeForHeight(target);
  } else {
    const lgfx::GFXfont* tag_candidates[] = {&lgfx::fonts::FreeSansBold12pt7b,
                                             &lgfx::fonts::FreeSansBold9pt7b};
    s_tag_gfx = pickGfxFontClosest(target, tag_candidates, 2);
  }
  s_tag_label_metrics_ready = true;
}

void initPalette() {
  radar::kColorBackground = rgb(kBg);
  radar::kColorGrid = rgb(radar::kGridR, radar::kGridG, radar::kGridB);
  radar::kColorLabel = rgb(255, 255, 255);
  radar::kColorCenter = rgb(255, 255, 255);
  radar::kColorAircraft = rgb(radar::kAircraftR, radar::kAircraftG, radar::kAircraftB);
  radar::kColorTrackVector = rgb(radar::kTrackR, radar::kTrackG, radar::kTrackB);
  radar::kColorTagType = rgb(radar::kTagTypeR, radar::kTagTypeG, radar::kTagTypeB);
  radar::kColorTagAltitude = rgb(radar::kTagAltR, radar::kTagAltG, radar::kTagAltB);
  radar::kColorRunway = rgb(radar::kRunwayR, radar::kRunwayG, radar::kRunwayB);
  radar::kColorRunwayLabel =
      rgb(radar::kRunwayLabelR, radar::kRunwayLabelG, radar::kRunwayLabelB);
}

constexpr float kKmPerDeg = 111.0f;
constexpr float kDegToRad = 3.14159265f / 180.0f;

void offsetKmFromCenter(float lat, float lon, float* dx_km, float* dy_km, float* dist_km) {
  // Longitude degrees shrink toward the poles; scale by cos(latitude) so
  // east-west distance isn't overstated away from the equator.
  const float center_lat_rad = static_cast<float>(services::location::lat()) * kDegToRad;
  *dx_km = static_cast<float>(lon - services::location::lon()) * kKmPerDeg * cosf(center_lat_rad);
  *dy_km = static_cast<float>(lat - services::location::lat()) * kKmPerDeg;
  *dist_km = sqrtf((*dx_km) * (*dx_km) + (*dy_km) * (*dy_km));
}

float innerRingMaxKm() {
  const float outer_km = radar::rangeCurrent().outer_km;
  return outer_km * (static_cast<float>(radar::kGridOuterRadius - radar::kAircraftInsideRingInsetPx) /
                     static_cast<float>(radar::kGridOuterRadius));
}

void latLonToScreen(float lat, float lon, int* out_x, int* out_y) {
  const float px_per_km = static_cast<float>(radar::kGridOuterRadius) / radar::rangeCurrent().outer_km;
  float dx_km = 0.0f;
  float dy_km = 0.0f;
  float dist_km = 0.0f;
  offsetKmFromCenter(lat, lon, &dx_km, &dy_km, &dist_km);
  *out_x = radar::kCenterX + static_cast<int>(lroundf(dx_km * px_per_km));
  *out_y = radar::kCenterY - static_cast<int>(lroundf(dy_km * px_per_km));
}

bool isInsideOuterRingKm(float dist_km) { return dist_km <= innerRingMaxKm(); }

int distSqFromCenter(int x, int y) {
  const int dx = x - radar::kCenterX;
  const int dy = y - radar::kCenterY;
  return dx * dx + dy * dy;
}

bool beyondRingEdgeDotFromLatLon(float lat, float lon, int* out_x, int* out_y) {
  float dx_km = 0.0f;
  float dy_km = 0.0f;
  float dist_km = 0.0f;
  offsetKmFromCenter(lat, lon, &dx_km, &dy_km, &dist_km);
  if (dist_km < 0.01f || isInsideOuterRingKm(dist_km)) {
    return false;
  }
  const int rim_r = radar::kCenterX - radar::kBeyondRingScreenMarginPx - 2;
  const float angle_rad = atan2f(dx_km, dy_km);
  *out_x = radar::kCenterX + static_cast<int>(lroundf(sinf(angle_rad) * rim_r));
  *out_y = radar::kCenterY - static_cast<int>(lroundf(cosf(angle_rad) * rim_r));
  return true;
}

void clipPointToOuterRing(int x0, int y0, int* x1, int* y1) {
  const int max_r_sq = radar::kGridOuterRadius * radar::kGridOuterRadius;
  if (distSqFromCenter(*x1, *y1) <= max_r_sq) {
    return;
  }
  const int dx = *x1 - x0;
  const int dy = *y1 - y0;
  float t = 1.0f;
  for (int step = 0; step < 20; ++step) {
    const int px = x0 + static_cast<int>(lroundf(dx * t));
    const int py = y0 + static_cast<int>(lroundf(dy * t));
    if (distSqFromCenter(px, py) <= max_r_sq) {
      *x1 = px;
      *y1 = py;
      return;
    }
    t -= 0.05f;
    if (t <= 0.0f) {
      *x1 = x0;
      *y1 = y0;
      return;
    }
  }
}

int speedLineLengthPx(float gs_knots) {
  if (gs_knots <= 0.0f) {
    return 0;
  }
  // Fixed screen scale: 60 s horizon at gs, not tied to current range zoom.
  constexpr float kKmPerKnotPerHorizon = 1.852f * radar::kAircraftTrackHorizonSec / 3600.0f;
  const float px = gs_knots * kKmPerKnotPerHorizon * radar::kGridOuterRadius /
                   radar::kAircraftTrackRefOuterKm * radar::kAircraftTrackLengthScale;
  const int len = static_cast<int>(px + 0.5f);
  return len < radar::kAircraftSpeedLineMinPx ? radar::kAircraftSpeedLineMinPx : len;
}

void drawSpeedVector(int cx, int cy, float heading_deg, float track_deg, float gs_knots) {
  const int len = speedLineLengthPx(gs_knots);
  if (len <= 0) {
    return;
  }
  const float h = heading_deg * kDegToRad;
  const int tip_x = cx + static_cast<int>(lroundf(sinf(h) * radar::kAircraftNoseLenPx));
  const int tip_y = cy - static_cast<int>(lroundf(cosf(h) * radar::kAircraftNoseLenPx));
  const float t = track_deg * kDegToRad;
  int ex = tip_x + static_cast<int>(lroundf(sinf(t) * len));
  int ey = tip_y - static_cast<int>(lroundf(cosf(t) * len));
  clipPointToOuterRing(tip_x, tip_y, &ex, &ey);
  if (ex == tip_x && ey == tip_y) {
    return;
  }
  // Tapered: bright at the nose, thin at the end.
  draw::thickLine(*s_draw, tip_x, tip_y, ex, ey, 1.3f, 0.5f, radar::kColorTrackVector);
}

void applyTagStyle() {
  if (s_tag_use_vlw) {
    displayFontEnsureLoaded(*s_draw);
    displayFontSetSmoothSize(*s_draw, s_tag_vlw_size);
  } else {
    displayFontSetBitmap(*s_draw, s_tag_gfx);
  }
}

/** Speed line under the altitude, a bit smaller than the rest of the tag. */
constexpr float kSpeedTagScale = 0.8f;

void applySpeedTagStyle() {
  if (s_tag_use_vlw) {
    displayFontEnsureLoaded(*s_draw);
    displayFontSetSmoothSize(*s_draw, s_tag_vlw_size * kSpeedTagScale);
  } else {
    displayFontSetBitmap(*s_draw, &lgfx::fonts::FreeSans9pt7b);
  }
}

void formatSpeed(const Aircraft& plane, char* out, size_t len) {
  if (plane.gs_knots < 1.0f) {
    out[0] = '\0';
    return;
  }
  snprintf(out, len, "%d km/h", static_cast<int>(lroundf(plane.gs_knots * 1.852f)));
}

/**
 * How much of the tag to show at the current zoom, so wide views don't turn
 * into a wall of text: 100 km = callsign + type, 50 km = + altitude,
 * 25 km and closer = + speed.
 */
struct TagDetail {
  bool altitude;
  bool speed;
  bool route;
};

TagDetail tagDetail() {
  const float ring_km = radar::rangeCurrent().ring3_km;
  return TagDetail{ring_km < 100.0f, ring_km < 50.0f, ring_km <= 25.0f};
}

/** Known route for this aircraft, if the network task has looked it up. */
const RouteCodes* routeFor(const Aircraft& plane) {
  if (s_model == nullptr || plane.callsign[0] == '\0') return nullptr;
  for (size_t i = 0; i < s_model->route_code_count; ++i) {
    if (strcmp(s_model->route_codes[i].callsign, plane.callsign) == 0) {
      return &s_model->route_codes[i];
    }
  }
  return nullptr;
}

constexpr int kRouteArrowW = 12;

int routeWidth(const RouteCodes& r) {
  return s_draw->textWidth(r.from) + kRouteArrowW + s_draw->textWidth(r.to);
}

/** "CNF → VCP" with a drawn arrow (the radar font has no arrow glyph). */
void drawRoute(const RouteCodes& r, int x, int y, int line_h, uint16_t color) {
  s_draw->setTextColor(color);
  s_draw->drawString(r.from, x, y);
  const int ax = x + s_draw->textWidth(r.from) + 2;
  const int ay = y + line_h / 2;
  s_draw->drawFastHLine(ax, ay, kRouteArrowW - 5, color);
  s_draw->fillTriangle(ax + kRouteArrowW - 4, ay, ax + kRouteArrowW - 8, ay - 3, ax + kRouteArrowW - 8,
                       ay + 3, color);
  s_draw->drawString(r.to, ax + kRouteArrowW - 2, y);
}

uint32_t s_now_ms = 0;  // page time, for pulsing emergency aircraft
const Rgb kEmergency{255, 64, 64};
const Rgb kMilitary{140, 214, 100};

bool isEmergency(const Aircraft& p) { return p.flags & services::adsb::kFlagEmergency; }
bool isMilitary(const Aircraft& p) { return p.flags & services::adsb::kFlagMilitary; }

/** Second tag line: the aircraft type, or the squawk code in an emergency. */
const char* tagTypeText(const Aircraft& p, char* buf, size_t len) {
  if (isEmergency(p)) {
    if (p.squawk) {
      snprintf(buf, len, "SQ %u", p.squawk);
    } else {
      snprintf(buf, len, "SOS");
    }
    return buf;
  }
  return p.type;
}

int measureTagBlockWidth(const Aircraft& plane, const TagDetail& detail) {
  applyTagStyle();
  int max_w = 0;
  char type[12];
  for (const char* s : {plane.callsign, tagTypeText(plane, type, sizeof(type)),
                        detail.altitude ? plane.alt : ""}) {
    if (s[0] != '\0') {
      max_w = std::max(max_w, static_cast<int>(s_draw->textWidth(s)));
    }
  }
  return max_w;
}

struct Box {
  int x0;
  int y0;
  int x1;
  int y1;
};

bool overlaps(const Box& a, const Box& b) {
  return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

/** All four corners inside the round glass (with a small margin). */
bool insideDisc(const Box& b) {
  constexpr int kR = radar::kSize / 2 - 3;
  for (int x : {b.x0, b.x1}) {
    for (int y : {b.y0, b.y1}) {
      if (distSqFromCenter(x, y) > kR * kR) return false;
    }
  }
  return true;
}

/**
 * Place a 3-line tag next to the aircraft, trying both sides and a few
 * vertical nudges so it doesn't cover other tags or aircraft.
 */
void drawAircraftTag(int x, int y, const Aircraft& plane, Box* taken, size_t* taken_n) {
  initTagLabelMetrics();
  applyTagStyle();
  const int line_h = s_draw->fontHeight();
  const TagDetail detail = tagDetail();
  int block_w = measureTagBlockWidth(plane, detail);
  const RouteCodes* route = detail.route ? routeFor(plane) : nullptr;
  if (route != nullptr) {
    block_w = std::max(block_w, routeWidth(*route));
  }
  char speed[16];
  speed[0] = '\0';
  if (detail.speed) {
    formatSpeed(plane, speed, sizeof(speed));
  }
  int speed_h = 0;
  if (speed[0] != '\0') {
    applySpeedTagStyle();
    speed_h = s_draw->fontHeight();
    block_w = std::max(block_w, static_cast<int>(s_draw->textWidth(speed)));
    applyTagStyle();
  }
  const int block_h = line_h * ((detail.altitude ? 3 : 2) + (route != nullptr ? 1 : 0)) + speed_h;
  const int gap = radar::kAircraftNoseLenPx + 1 + radar::kAircraftLabelGapPx;
  // West half: prefer the tag toward the center (right); east half: left.
  const bool prefer_right = x < radar::kCenterX;

  // Candidate boxes in order of preference: beside the plane (with vertical
  // nudges) on the preferred side, then the other side, then above / below.
  Box cands[16];
  size_t n = 0;
  const int nudges[] = {0, -block_h / 2, block_h / 2, -block_h, block_h};
  for (int side = 0; side < 2; ++side) {
    const bool right = side == 0 ? prefer_right : !prefer_right;
    for (int dy : nudges) {
      const int x0 = right ? x + gap : x - gap - block_w;
      cands[n++] = Box{x0, y - block_h / 2 + dy, x0 + block_w, y - block_h / 2 + dy + block_h};
    }
  }
  cands[n++] = Box{x - block_w / 2, y - gap - block_h, x + block_w / 2, y - gap};
  cands[n++] = Box{x - block_w / 2, y + gap, x + block_w / 2, y + gap + block_h};

  // Pick the first clear spot; otherwise the one that overlaps least.
  Box best = cands[0];
  long best_cost = -1;
  for (size_t c = 0; c < n; ++c) {
    Box b = cands[c];
    const int w = b.x1 - b.x0;
    const int h = b.y1 - b.y0;
    b.x0 = std::max(1, std::min(b.x0, radar::kSize - w - 1));
    b.y0 = std::max(1, std::min(b.y0, radar::kSize - h - 1));
    b.x1 = b.x0 + w;
    b.y1 = b.y0 + h;
    long cost = insideDisc(b) ? 0 : 100000;
    for (size_t i = 0; i < *taken_n; ++i) {
      const Box& o = taken[i];
      if (overlaps(b, o)) {
        cost += static_cast<long>(std::min(b.x1, o.x1) - std::max(b.x0, o.x0)) *
                (std::min(b.y1, o.y1) - std::max(b.y0, o.y0));
      }
    }
    if (best_cost < 0 || cost < best_cost) {
      best = b;
      best_cost = cost;
    }
    if (cost == 0) break;
  }
  taken[(*taken_n)++] = best;

  // Transparent text so the map and sweep show through between glyphs.
  s_draw->setTextDatum(textdatum_t::top_left);
  int ly = best.y0;
  const bool emergency = isEmergency(plane);
  const bool military = isMilitary(plane);
  if (plane.callsign[0] != '\0') {
    s_draw->setTextColor(emergency ? rgb(kEmergency) : radar::kColorLabel);
    s_draw->drawString(plane.callsign, best.x0, ly);
  }
  ly += line_h;
  char type[12];
  const char* type_text = tagTypeText(plane, type, sizeof(type));
  if (type_text[0] != '\0') {
    s_draw->setTextColor(emergency  ? rgb(kEmergency)
                         : military ? rgb(kMilitary)
                                    : radar::kColorTagType);
    s_draw->drawString(type_text, best.x0, ly);
  }
  ly += line_h;
  if (route != nullptr) {
    drawRoute(*route, best.x0, ly, line_h, rgb(196, 170, 255));
    ly += line_h;
  }
  if (detail.altitude && plane.alt[0] != '\0') {
    s_draw->setTextColor(radar::kColorTagAltitude);
    s_draw->drawString(plane.alt, best.x0, ly);
    ly += line_h;
  }
  if (speed[0] != '\0') {
    applySpeedTagStyle();
    s_draw->setTextColor(rgb(150, 228, 170));
    s_draw->drawString(speed, best.x0, ly);
  }
}

struct AircraftDrawItem {
  size_t index = 0;
  int x = 0;
  int y = 0;
  int dist_sq = 0;
};

/** Sweep angle (deg, 0 = north, clockwise) at time t. */
float sweepAngle(uint32_t t) {
  return fmodf(static_cast<float>(t) * 360.0f / radar::kSweepPeriodMs, 360.0f);
}

/** ms since the sweep last crossed bearing_deg. */
float msSinceSwept(float sweep_deg, float bearing_deg) {
  const float d = fmodf(sweep_deg - bearing_deg + 720.0f, 360.0f);
  return d * radar::kSweepPeriodMs / 360.0f;
}

void drawAircraft(const Model& m, float sweep_deg) {
  initLabelMetrics();
  const size_t n = m.plane_count;
  const Aircraft* planes = m.planes;

  static AircraftDrawItem items[services::adsb::kMaxAircraft];  // static: keep off the 8 KB loop stack
  size_t draw_count = 0;

  for (size_t i = 0; i < n; ++i) {
    float dx_km = 0.0f;
    float dy_km = 0.0f;
    float dist_km = 0.0f;
    offsetKmFromCenter(planes[i].lat, planes[i].lon, &dx_km, &dy_km, &dist_km);
    if (isInsideOuterRingKm(dist_km)) {
      int x = 0;
      int y = 0;
      latLonToScreen(planes[i].lat, planes[i].lon, &x, &y);
      items[draw_count++] = AircraftDrawItem{i, x, y, distSqFromCenter(x, y)};
      continue;
    }
    int dot_x = 0;
    int dot_y = 0;
    if (beyondRingEdgeDotFromLatLon(planes[i].lat, planes[i].lon, &dot_x, &dot_y)) {
      s_draw->fillSmoothCircle(dot_x, dot_y, 3, radar::kColorAircraft);
    }
  }

  std::sort(items, items + draw_count,
            [](const AircraftDrawItem& a, const AircraftDrawItem& b) { return a.dist_sq > b.dist_sq; });

  const Rgb orange{radar::kAircraftR, radar::kAircraftG, radar::kAircraftB};
  for (size_t d = 0; d < draw_count; ++d) {
    const Aircraft& p = planes[items[d].index];
    const int x = items[d].x;
    const int y = items[d].y;
    // "Ping" ring when the sweep passes over the aircraft.
    const float bearing =
        atan2f(static_cast<float>(x - radar::kCenterX), static_cast<float>(radar::kCenterY - y)) / kDegToRad;
    const float since = sweep_deg < 0.0f ? 1e9f : msSinceSwept(sweep_deg, bearing);
    if (since < 900.0f) {
      const float k = since / 900.0f;
      s_draw->drawCircle(x, y, 6 + static_cast<int>(k * 12.0f), mix(kBg, orange, (1.0f - k) * 0.9f));
    }
    uint16_t color = radar::kColorAircraft;
    if (isEmergency(p)) {
      // Two expanding red rings, so it stands out at any zoom.
      for (int k = 0; k < 2; ++k) {
        const float ph = fmodf(s_now_ms / 1100.0f + k * 0.5f, 1.0f);
        s_draw->drawCircle(x, y, 8 + static_cast<int>(ph * 14.0f), mix(kBg, kEmergency, 1.0f - ph));
      }
      color = rgb(kEmergency);
    } else if (isMilitary(p)) {
      color = rgb(kMilitary);
    }
    drawSpeedVector(x, y, p.nose_deg, p.track_deg, p.gs_knots);
    draw::airplane(*s_draw, x, y, p.nose_deg, 17.0f, color);
  }
  // Aircraft symbols are obstacles for every tag; tags claim space nearest-first.
  static Box taken[services::adsb::kMaxAircraft * 2 + 6];
  size_t taken_n = 0;
  // N / S / O / L letters and the range label on the east spoke.
  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  taken[taken_n++] = Box{cx - 9, 0, cx + 9, 18};
  taken[taken_n++] = Box{cx - 9, radar::kSize - 18, cx + 9, radar::kSize};
  taken[taken_n++] = Box{0, cy - 10, 16, cy + 10};
  taken[taken_n++] = Box{radar::kSize - 16, cy - 10, radar::kSize, cy + 10};
  taken[taken_n++] = Box{cx + 58, cy - 10, cx + radar::kGridOuterRadius, cy + 10};
  taken[taken_n++] = Box{cx - 8, cy - 8, cx + 8, cy + 8};  // home marker
  for (size_t d = 0; d < draw_count; ++d) {
    taken[taken_n++] = Box{items[d].x - 9, items[d].y - 9, items[d].x + 9, items[d].y + 9};
  }
  for (size_t d = draw_count; d-- > 0;) {
    drawAircraftTag(items[d].x, items[d].y, planes[items[d].index], taken, &taken_n);
  }
}

void applyCardinalStyle() {
  if (s_cardinal_use_vlw) {
    displayFontEnsureLoaded(*s_draw);
    displayFontSetSmoothSize(*s_draw, s_cardinal_vlw_size);
  } else {
    displayFontSetBitmap(*s_draw, s_cardinal_gfx);
  }
}

void applyScaleStyle() {
  if (s_scale_use_vlw) {
    displayFontEnsureLoaded(*s_draw);
    displayFontSetSmoothSize(*s_draw, s_scale_vlw_size);
  } else {
    displayFontSetBitmap(*s_draw, s_scale_gfx);
  }
}

void drawCardinalLabels() {
  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int edge = radar::kSize - 1;
  applyCardinalStyle();
  s_draw->setTextColor(radar::kColorLabel);
  // Portuguese compass: Norte, Sul, Leste, Oeste.
  s_draw->setTextDatum(textdatum_t::top_center);
  s_draw->drawString(i18n::cardinal(0), cx, radar::kCardinalNorthOffsetY);
  s_draw->setTextDatum(textdatum_t::bottom_center);
  s_draw->drawString(i18n::cardinal(1), cx, edge + radar::kCardinalSouthOffsetY);
  s_draw->setTextDatum(textdatum_t::middle_left);
  s_draw->drawString(i18n::cardinal(3), 1, cy);
  s_draw->setTextDatum(textdatum_t::middle_right);
  s_draw->drawString(i18n::cardinal(2), edge, cy);
}

void drawScaleLabel() {
  char label[12];
  radar::formatCurrentRing3Label(label, sizeof(label));
  applyScaleStyle();
  s_draw->setTextDatum(textdatum_t::middle_right);
  const int x = radar::kCenterX + radar::kGridOuterRadius - radar::kScaleGapFromOuterRing;
  const int y = radar::kCenterY;
  const int tw = s_draw->textWidth(label);
  const int th = s_draw->fontHeight();
  s_draw->fillRoundRect(x - tw - 4, y - th / 2 - 2, tw + 7, th + 4, 3, radar::kColorBackground);
  s_draw->setTextColor(rgb(110, 170, 240));
  s_draw->drawString(label, x, y);
}

// Author's initials set into the bezel in place of three minor degree ticks,
// each letter turned to follow the curve (lower-left, read left to right).
constexpr int kSignatureDeg[] = {235, 230, 225};
constexpr const char* kSignature[] = {"G", "F", "S"};

bool isSignatureTick(int deg) {
  for (int d : kSignatureDeg) {
    if (d == deg) return true;
  }
  return false;
}

void drawSignature() {
  static LGFX_Sprite glyph;
  static bool ready = false;
  if (!ready) {
    glyph.setColorDepth(16);
    ready = glyph.createSprite(14, 14) != nullptr;
  }
  if (!ready) return;
  constexpr float kR = 113.5f;
  // Clear the bezel strip behind the letters (the map reaches the edge).
  draw::ring(*s_draw, radar::kCenterX, radar::kCenterY, 108, 120, 221.0f, 239.0f,
             radar::kColorBackground);
  for (int i = 0; i < 3; ++i) {
    glyph.fillScreen(radar::kColorBackground);
    displayFontEnsureLoaded(glyph);
    displayFontSetSmoothSize(glyph, s_scale_vlw_size * 1.05f);
    glyph.setTextDatum(textdatum_t::middle_center);
    glyph.setTextColor(rgb(150, 195, 245), radar::kColorBackground);
    glyph.drawString(kSignature[i], 7, 7);
    const float a = kSignatureDeg[i] * kDegToRad;
    const float x = radar::kCenterX + sinf(a) * kR;
    const float y = radar::kCenterY - cosf(a) * kR;
    // Tops of the letters face the centre.
    glyph.pushRotateZoom(s_draw, x, y, kSignatureDeg[i] - 180.0f, 1.0f, 1.0f,
                         radar::kColorBackground);
  }
}

void drawSweep(float sweep_deg) {
  // Fading trail behind the sweep line, built from flat triangles (fast on C3).
  constexpr int kSegments = 12;
  const float seg = radar::kSweepTrailDeg / kSegments;
  for (int i = 0; i < kSegments; ++i) {
    const float k = static_cast<float>(i) / kSegments;
    draw::pie(*s_draw, radar::kCenterX, radar::kCenterY, radar::kGridOuterRadius,
              sweep_deg - (i + 1) * seg, sweep_deg - i * seg, mix(kBg, kSweep, 0.30f * (1.0f - k)));
  }
}

void drawSweepLine(float sweep_deg) {
  const float a = sweep_deg * kDegToRad;
  const int x = radar::kCenterX + static_cast<int>(lroundf(sinf(a) * radar::kGridOuterRadius));
  const int y = radar::kCenterY - static_cast<int>(lroundf(cosf(a) * radar::kGridOuterRadius));
  draw::thickLine(*s_draw, radar::kCenterX, radar::kCenterY, x, y, 0.5f, 1.2f, rgb(130, 200, 255));
}

void drawGrid() {
  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int r = radar::kGridOuterRadius;
  for (int i = 1; i <= radar::kRingCount; ++i) {
    const uint16_t c = i == radar::kRingCount ? rgb(60, 124, 210) : radar::kColorGrid;
    s_draw->drawCircle(cx, cy, r * i / radar::kRingCount, c);
  }
  s_draw->drawCircle(cx, cy, r - 1, rgb(40, 100, 180));
  s_draw->drawFastVLine(cx, cy - r, r * 2, radar::kColorGrid);
  s_draw->drawFastHLine(cx - r, cy, r * 2, radar::kColorGrid);

  // Bezel ticks every 5°, longer every 30°; leave room for N/S/L/O.
  for (int deg = 0; deg < 360; deg += 5) {
    if (deg % 90 == 0 || isSignatureTick(deg)) {
      continue;
    }
    const bool major = deg % 30 == 0;
    const float a = deg * kDegToRad;
    const float r0 = major ? 110.0f : 113.0f;
    const float r1 = 117.0f;
    s_draw->drawLine(cx + static_cast<int>(sinf(a) * r0), cy - static_cast<int>(cosf(a) * r0),
                     cx + static_cast<int>(sinf(a) * r1), cy - static_cast<int>(cosf(a) * r1),
                     major ? rgb(90, 160, 240) : rgb(36, 80, 140));
  }
}

uint32_t s_prof[kRadarStages] = {};
uint32_t s_prof_mark = 0;

void mark(int stage) {
  const uint32_t now = lgfx::micros();
  s_prof[stage] += now - s_prof_mark;
  s_prof_mark = now;
}

}  // namespace

const char* const kRadarStageNames[kRadarStages] = {"bg+sweep", "map", "grid", "runways",
                                                    "center", "aircraft", "labels", "-"};

void radarProfile(uint32_t out_us[kRadarStages]) {
  for (int i = 0; i < kRadarStages; ++i) {
    out_us[i] = s_prof[i];
    s_prof[i] = 0;
  }
}

uint32_t drawRadarPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t) {
  s_draw = &g;
  s_model = &m;
  s_now_ms = t;
  initPalette();
  initLabelMetrics();

  s_prof_mark = lgfx::micros();
  // Sweep is optional (portal setting); -1 means "no sweep" to drawAircraft.
  const bool sweep_on = radar::showSweep();
  const float sweep = sweep_on ? sweepAngle(t) : -1.0f;
  g.fillScreen(radar::kColorBackground);
  if (sweep_on) {
    drawSweep(sweep);
  }
  mark(0);
  radar_map::draw(g, m.radar_map, static_cast<uint16_t*>(m.frame_buffer), m.band_y0, m.band_y1);
  mark(1);
  drawGrid();
  mark(2);
  runway::drawLargeAirportRunways(g);
  mark(3);
  if (sweep_on) {
    drawSweepLine(sweep);
  }

  // Pulsing home marker.
  const float pulse = 0.5f + 0.5f * sinf(t * 0.005f);
  g.drawCircle(radar::kCenterX, radar::kCenterY, 4 + static_cast<int>(pulse * 3),
               mix(kBg, Rgb{255, 255, 255}, 0.6f * (1.0f - pulse)));
  g.fillSmoothCircle(radar::kCenterX, radar::kCenterY, radar::kCenterDotRadius, radar::kColorCenter);
  mark(4);

  drawAircraft(m, sweep);
  mark(5);
  drawCardinalLabels();
  drawScaleLabel();
  drawSignature();
  mark(6);
  g.setTextDatum(textdatum_t::top_left);
  s_draw = &tft;
  // Without the sweep only the home marker pulses, so fewer frames suffice
  // (unless an aircraft in emergency is pulsing).
  bool emergency = false;
  for (size_t i = 0; i < m.plane_count; ++i) emergency |= isEmergency(m.planes[i]);
  return sweep_on || emergency ? 40 : 100;
}

}  // namespace ui
