// Desktop preview: renders the real page code into a 240×240 sprite with
// sample data and dumps raw RGB frames for make_preview.py.
//
//   make -C sim && sim/build/sim sim/out

#include <sys/stat.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <Preferences.h>  // simulator stand-in: FakeSerial

#include "hardware/display.h"
#include "hardware/display_font.h"
#include "services/radar_location.h"
#include "ui/draw_util.h"
#include "ui/fonts.h"
#include "ui/i18n.h"
#include "ui/pages.h"
#include "ui/radar_map.h"
#include "ui/radar_range.h"

FakeSerial Serial;
LGFX tft;

#ifndef PR_ROOT
#define PR_ROOT "."
#endif

namespace ui::fonts {

const uint8_t* data(Id id) {
  static const char* kFiles[] = {
      "data/ui_font.vlw",           "data/fonts/noto_bold_14.vlw",
      "data/fonts/noto_bold_17.vlw", "data/fonts/noto_bold_22.vlw",
      "data/fonts/noto_bold_28.vlw", "data/fonts/noto_bold_54_digits.vlw",
  };
  static std::vector<uint8_t> cache[static_cast<int>(Id::Count)];
  const int i = static_cast<int>(id);
  if (i < 0 || i >= static_cast<int>(Id::Count)) return nullptr;
  if (cache[i].empty()) {
    const std::string path = std::string(PR_ROOT) + "/" + kFiles[i];
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
      fprintf(stderr, "missing font %s\n", path.c_str());
      return nullptr;
    }
    uint8_t buf[4096];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) cache[i].insert(cache[i].end(), buf, buf + n);
    fclose(f);
  }
  return cache[i].data();
}

}  // namespace ui::fonts

namespace {

using services::adsb::Aircraft;

std::vector<uint8_t> readFile(const std::string& path) {
  std::vector<uint8_t> out;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return out;
  uint8_t buf[4096];
  size_t n = 0;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.insert(out.end(), buf, buf + n);
  fclose(f);
  return out;
}

/** Download a tile once into sim/cache (curl), then reuse it. */
std::vector<uint8_t> tile(int z, int x, int y) {
  const std::string dir = std::string(PR_ROOT) + "/sim/cache";
  mkdir(dir.c_str(), 0755);
  char path[256];
  snprintf(path, sizeof(path), "%s/tile_%d_%d_%d.jpg", dir.c_str(), z, y, x);
  auto data = readFile(path);
  if (!data.empty()) return data;
  char url[256];
  snprintf(url, sizeof(url), ui::radar_map::kTileUrlFormat, z, y, x);
  const std::string cmd = std::string("curl -sf -A 'PlaneRadar-sim/1.0' -o '") + path + "' '" + url + "'";
  if (system(cmd.c_str()) != 0) fprintf(stderr, "tile download failed: %s\n", url);
  return readFile(path);
}

void buildMap(uint8_t* map, double lat, double lon, float outer_km) {
  memset(map, 0, ui::radar_map::kBytes);
  const auto p = ui::radar_map::plan(lat, lon, outer_km);
  // Same as the firmware: the centre tile calibrates the classes.
  int cx = 0;
  int cy = 0;
  ui::radar_map::centerTile(p, &cx, &cy);
  ui::radar_map::Histogram hist;
  const auto center = tile(p.z, cx, cy);
  ui::radar_map::accumulate(center.data(), center.size(), &hist);
  const auto th = ui::radar_map::calibrate(hist);
  int tiles = 0;
  for (int ty = p.ty0; ty <= p.ty1; ++ty) {
    for (int tx = p.tx0; tx <= p.tx1; ++tx) {
      const auto jpg = tile(p.z, tx, ty);
      if (!jpg.empty() && ui::radar_map::paintTile(p, tx, ty, jpg.data(), jpg.size(), th, map)) ++tiles;
    }
  }
  ui::radar_map::despeckle(map);
  printf("map: outer %.1f km -> zoom %d scale %.2f, %d tiles, water<%d urban>=%d road>=%d\n", outer_km,
         p.z, p.scale, tiles, th.water_max, th.urban_min, th.road_min);
}

Aircraft plane(const char* cs, const char* type, const char* desc, const char* reg, double lat0,
               double lon0, float dx_km, float dy_km, int alt_ft, float gs, float hdg) {
  Aircraft a{};
  a.lat = static_cast<float>(lat0 + dy_km / 111.0);
  a.lon = static_cast<float>(lon0 + dx_km / (111.0 * cos(lat0 * M_PI / 180.0)));
  a.nose_deg = hdg;
  a.track_deg = hdg;
  a.gs_knots = gs;
  snprintf(a.callsign, sizeof(a.callsign), "%s", cs);
  snprintf(a.type, sizeof(a.type), "%s", type);
  snprintf(a.alt, sizeof(a.alt), "%d ft", alt_ft);
  a.alt_ft = alt_ft;
  a.has_alt = true;
  a.on_ground = false;
  snprintf(a.desc, sizeof(a.desc), "%s", desc);
  snprintf(a.reg, sizeof(a.reg), "%s", reg);
  return a;
}

struct Scenario {
  std::string name;
  ui::Page page;
  ui::Model model;
  uint32_t duration_ms;
  bool tick_clock;
  bool radius_toast = false;  // pretend the search radius was just changed
  bool alert = false;         // overhead alert fires at t = 0
  int month_change_ms = -1;   // calendar: show the next month from this time
  ui::i18n::Lang lang = ui::i18n::Lang::PT;
  uint16_t dim = 256;  // night-mode brightness applied after drawing (256 = off)
};

void setTime(ui::TimeModel* t, int h, int m, int s, int ms) {
  t->valid = true;
  t->year = 2026;
  t->month = 9;
  t->day = 29;
  t->wday = 2;
  t->hour = h;
  t->minute = m;
  t->second = s;
  t->millis = ms;
}

void advance(ui::TimeModel* t, const ui::TimeModel& start, uint32_t ms) {
  const long total = ((start.hour * 60L + start.minute) * 60L + start.second) * 1000L + start.millis + ms;
  *t = start;
  t->millis = total % 1000;
  t->second = (total / 1000) % 60;
  t->minute = (total / 60000) % 60;
  t->hour = (total / 3600000) % 24;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string out = argc > 1 ? argv[1] : "sim/out";
  mkdir(out.c_str(), 0755);

  displayFontInit();
  services::location::init();
  ui::radar::rangeInit();
  const double lat = services::location::lat();
  const double lon = services::location::lon();

  LGFX_Sprite frame;
  frame.setColorDepth(16);
  if (!frame.createSprite(240, 240)) {
    fprintf(stderr, "sprite alloc failed\n");
    return 1;
  }

  static uint8_t map[ui::radar_map::kBytes];
  // Map preview for every range (sim/out/maps_all.rgb, one 240x240 per range).
  {
    FILE* mf = fopen((out + "/maps_all.rgb").c_str(), "wb");
    std::vector<lgfx::bgr888_t> px(240 * 240);
    for (size_t r = 0; r < ui::radar::kRangePresetCount; ++r) {
      buildMap(map, lat, lon, ui::radar::kRangePresets[r].outer_km);
      frame.fillScreen(0);
      ui::radar_map::draw(frame, map, static_cast<uint16_t*>(frame.getBuffer()));
      frame.readRect(0, 0, 240, 240, px.data());
      fwrite(px.data(), 3, px.size(), mf);
    }
    fclose(mf);
  }
  buildMap(map, lat, lon, ui::radar::rangeCurrent().outer_km);

  std::vector<Aircraft> planes = {
      plane("AZU4521", "A20N", "AIRBUS A-320neo", "PR-YRA", lat, lon, 3.2f, 2.4f, 9800, 260, 20),
      plane("GLO1690", "B38M", "BOEING 737 MAX 8", "PR-XMG", lat, lon, -6.5f, -1.5f, 14000, 330, 335),
      plane("TAM3302", "A321", "AIRBUS A-321", "PT-MXC", lat, lon, 1.0f, 8.6f, 22000, 410, 190),
      plane("PRHBS", "R44", "ROBINSON R-44 Raven", "PR-HBS", lat, lon, -2.0f, -4.0f, 2600, 90, 110),
      plane("AZU2710", "E195", "EMBRAER 195", "PR-AXH", lat, lon, 18.0f, -12.0f, 18000, 300, 300),
  };

  static ui::RouteCodes codes[2];
  snprintf(codes[0].callsign, sizeof(codes[0].callsign), "AZU4521");
  snprintf(codes[0].from, sizeof(codes[0].from), "CNF");
  snprintf(codes[0].to, sizeof(codes[0].to), "VCP");
  snprintf(codes[1].callsign, sizeof(codes[1].callsign), "TAM3302");
  snprintf(codes[1].from, sizeof(codes[1].from), "GRU");
  snprintf(codes[1].to, sizeof(codes[1].to), "CNF");

  ui::Model base;
  base.route_codes = codes;
  base.route_code_count = 2;
  base.wifi_ok = true;
  base.lat = lat;
  base.lon = lon;
  base.planes = planes.data();
  base.plane_count = planes.size();
  base.radar_map = map;
  setTime(&base.time, 9, 41, 12, 0);
  base.sun = {true, 5 * 60 + 48, 18 * 60 + 4};
  base.air = {true, 42, 7.2f};
  base.weather = {true, 28.3f, 31.3f, 50, 3.7f, 0, true, 18.5f, 32.8f, 10};
  {
    ui::SummaryModel& r = base.summary;
    r.seen = 147;
    snprintf(r.highest_cs, sizeof(r.highest_cs), "TAM8084");
    r.highest_ft = 41000;
    snprintf(r.fastest_cs, sizeof(r.fastest_cs), "UAL861");
    r.fastest_kmh = 934;
    snprintf(r.closest_cs, sizeof(r.closest_cs), "AZU4521");
    r.closest_km = 1.2f;
    snprintf(r.airline, sizeof(r.airline), "Azul");
    r.airline_count = 38;
  }
  base.hourly.valid = true;
  {
    const float temps[] = {24, 26, 27.5f, 28.3f, 28, 27, 25, 23, 21.5f, 20, 19, 18.5f};
    const int rain[] = {0, 0, 5, 10, 30, 60, 80, 40, 20, 10, 0, 0};
    for (int i = 0; i < 12; ++i) {
      base.hourly.hour[i] = (10 + i) % 24;
      base.hourly.temp_c[i] = temps[i];
      base.hourly.rain_prob[i] = rain[i];
    }
    base.hourly.count = 12;
  }
  base.forecast.valid = true;
  {
    const ui::ForecastDay days[] = {{3, 1, 17.0f, 30.0f, 0}, {4, 2, 18.0f, 29.0f, 20},
                                    {5, 80, 16.0f, 24.0f, 70}, {6, 95, 15.0f, 22.0f, 90},
                                    {0, 3, 14.0f, 25.0f, 30}};
    for (const auto& d : days) base.forecast.days[base.forecast.count++] = d;
  }
  base.route.valid = true;
  snprintf(base.route.callsign, sizeof(base.route.callsign), "AZU4521");
  snprintf(base.route.airline, sizeof(base.route.airline), "Azul Linhas Aéreas");
  snprintf(base.route.from_iata, sizeof(base.route.from_iata), "CNF");
  snprintf(base.route.to_iata, sizeof(base.route.to_iata), "VCP");
  snprintf(base.route.from_city, sizeof(base.route.from_city), "Belo Horizonte");
  snprintf(base.route.to_city, sizeof(base.route.to_city), "Campinas");

  // Overhead alert: a jet 1.2 km from home.
  std::vector<Aircraft> alert_planes = planes;
  alert_planes[0] = plane("AZU4521", "A20N", "AIRBUS A-320neo", "PR-YRA", lat, lon, 0.8f, 0.9f, 4200,
                          190, 160);
  ui::Model alert_base = base;
  alert_base.planes = alert_planes.data();
  // Emergency (squawk 7700) and a military aircraft.
  std::vector<Aircraft> emerg_planes = planes;
  emerg_planes[1].squawk = 7700;
  emerg_planes[1].flags = services::adsb::kFlagEmergency;
  emerg_planes[3] = plane("FAB2101", "C390", "EMBRAER KC-390", "FAB2853", lat, lon, -3.0f, 3.5f, 9000,
                          280, 70);
  emerg_planes[3].flags = services::adsb::kFlagMilitary;
  ui::Model emerg_base = base;
  emerg_base.planes = emerg_planes.data();

  // Sample agenda (made-up items) for the agenda page.
  ui::Model agenda_base = base;
  agenda_base.time.hour = 10;
  agenda_base.time.minute = 5;
  {
    ui::AgendaModel& a = agenda_base.agenda;
    a.configured = a.valid = true;
    auto ev = [&](const char* time, const char* title) {
      ui::AgendaItem& it = a.items[a.count++];
      snprintf(it.time, sizeof(it.time), "%s", time);
      snprintf(it.title, sizeof(it.title), "%s", title);
    };
    auto task = [&](const char* title, bool done) {
      ui::AgendaItem& it = a.items[a.count++];
      it.task = true;
      it.done = done;
      snprintf(it.title, sizeof(it.title), "%s", title);
    };
    ev("", "Aniversário da Ana");
    ev("08:30", "Academia");
    ev("10:30", "Reunião com a equipe");
    ev("14:00", "Dentista");
    ev("19:30", "Jantar com a família");
    task("Comprar pão", true);
    task("Ligar para o banco", false);
    task("Pagar a conta de luz", false);
  }

  auto weather = [&](const char* name, int code, bool day, float temp, float feels, float lo,
                     float hi, int rain) {
    Scenario s{name, ui::Page::Weather, base, 4000, false};
    s.model.weather = {true, temp, feels, 60, 8.0f, code, day, lo, hi, rain};
    return s;
  };

  std::vector<Scenario> scenarios = {
      {"radar", ui::Page::Radar, base, 5000, true},
      weather("clima_sol", 0, true, 28.3f, 31.3f, 18.5f, 32.8f, 10),
      weather("clima_nuvens", 2, true, 24.0f, 25.0f, 17.0f, 27.0f, 20),
      weather("clima_nublado", 3, true, 21.0f, 21.0f, 16.0f, 23.0f, 30),
      weather("clima_chuva", 63, true, 19.0f, 18.0f, 16.0f, 22.0f, 90),
      weather("clima_tempestade", 95, true, 21.0f, 22.0f, 17.0f, 26.0f, 80),
      weather("clima_noite", 0, false, 17.0f, 16.0f, 14.0f, 27.0f, 0),
      {"relogio", ui::Page::Clock, base, 4000, true},
      [&] {
        Scenario s{"relogio_noite", ui::Page::Clock, base, 4000, true};
        setTime(&s.model.time, 23, 41, 12, 0);
        s.model.weather.is_day = false;  // clear night: real moon phase
        s.dim = 20 * 256 / 100;
        return s;
      }(),
      {"aviao", ui::Page::Nearest, base, 7000, false},
      [&] {
        Scenario s{"aviao_raio", ui::Page::Nearest, base, 3000, false, true};
        s.model.nearest_radius_km = 5.0f;
        s.model.plane_count = 0;  // nothing within reach: empty state + hint
        return s;
      }(),
      {"ar_sol", ui::Page::AirSun, base, 4000, false},
      {"sobre", ui::Page::About, base, 6000, false},
      [&] {
        Scenario s{"calendario", ui::Page::Calendar, base, 5000, false};
        setTime(&s.model.time, 9, 41, 12, 0);
        s.model.time.month = 10;
        s.model.time.day = 3;
        s.model.time.wday = 6;
        s.month_change_ms = 3000;  // double tap: November slides in
        return s;
      }(),
      [&] {
        Scenario s{"calendario_feriado", ui::Page::Calendar, base, 2500, false};
        s.model.time.month = 10;
        s.model.time.day = 12;
        s.model.time.wday = 1;
        return s;
      }(),
      [&] {
        Scenario s{"aviao_alerta", ui::Page::Nearest, alert_base, 4000, false};
        s.alert = true;
        return s;
      }(),
      {"aviao_emergencia", ui::Page::Nearest, emerg_base, 4000, false},
      {"agenda", ui::Page::Agenda, agenda_base, 13000, false},
      {"previsao", ui::Page::Forecast, base, 4000, false},
      {"resumo", ui::Page::Summary, base, 4000, false},
      [&] {
        Scenario s{"previsao_horas", ui::Page::Forecast, base, 4000, false};
        s.model.forecast_hours = true;
        return s;
      }(),
      [&] {
        Scenario s{"agenda_livre", ui::Page::Agenda, base, 2500, false};
        s.model.agenda.configured = s.model.agenda.valid = true;
        return s;
      }(),
      {"agenda_configurar", ui::Page::Agenda, base, 2500, false},
      {"radar_emergencia", ui::Page::Radar, emerg_base, 4000, true},
  };
  // Same pages in the other languages (short clips, for checking layouts).
  const std::pair<const char*, ui::i18n::Lang> langs[] = {
      {"en", ui::i18n::Lang::EN}, {"es", ui::i18n::Lang::ES}, {"zh", ui::i18n::Lang::ZH}};
  for (const auto& [suffix, lang] : langs) {
    for (const auto& [name, page] : {std::pair<const char*, ui::Page>{"radar", ui::Page::Radar},
                                     {"clima", ui::Page::Weather}, {"relogio", ui::Page::Clock}, {"calendario", ui::Page::Calendar},
                                     {"agenda", ui::Page::Agenda}, {"previsao", ui::Page::Forecast}, {"resumo", ui::Page::Summary},
                                     {"aviao", ui::Page::Nearest}, {"ar_sol", ui::Page::AirSun}}) {
      Scenario s{std::string(name) + "_" + suffix, page, page == ui::Page::Agenda ? agenda_base : base,
                 1600, page == ui::Page::Clock};
      if (page == ui::Page::Weather) s.model.weather.code = 2;
      s.lang = lang;
      scenarios.push_back(s);
    }
  }

  // Same two-band rendering the firmware uses, to check it matches full frames.
  constexpr int kBand = 120;
  // Band buffer with 120 guard rows above and below: any write outside the
  // band (clip rect ignored) shows up as a changed guard pixel.
  constexpr uint16_t kGuard = 0xA5A5;
  std::vector<uint16_t> guarded(240 * (kBand * 3), kGuard);
  uint16_t* const band_ptr = guarded.data() + 240 * kBand;
  int guard_hits = 0;
  std::vector<uint16_t> banded(240 * 240);
  LGFX_Sprite band_sprite;
  auto renderBanded = [&](Scenario& s, uint32_t t) {
    for (int y0 = 0; y0 < 240; y0 += kBand) {
      uint16_t* base = band_ptr - y0 * 240;
      band_sprite.setBuffer(base, 240, 240, 16);
      band_sprite.setClipRect(0, y0, 240, kBand);
      s.model.frame_buffer = base;
      s.model.band_y0 = y0;
      s.model.band_y1 = y0 + kBand;
      ui::drawPage(s.page, band_sprite, s.model, t);
      memcpy(banded.data() + y0 * 240, band_ptr, 240 * kBand * 2);
      for (int i = 0; i < 240 * kBand; ++i) {
        if (guarded[i] != kGuard || guarded[240 * kBand * 2 + i] != kGuard) {
          ++guard_hits;
          break;
        }
      }
      std::fill(guarded.begin(), guarded.begin() + 240 * kBand, kGuard);
      std::fill(guarded.begin() + 240 * kBand * 2, guarded.end(), kGuard);
    }
    s.model.band_y0 = 0;
    s.model.band_y1 = 240;
  };

  constexpr uint32_t kStep = 40;  // 25 fps
  std::vector<lgfx::bgr888_t> rgb(240 * 240);  // bytes in R,G,B order
  FILE* manifest = fopen((out + "/manifest.txt").c_str(), "w");
  for (auto& s : scenarios) {
    const std::string path = out + "/" + s.name + ".rgb";
    FILE* f = fopen(path.c_str(), "wb");
    int frames = 0;
    int mismatched = 0;
    double total_ms = 0.0;
    const ui::TimeModel start = s.model.time;
    for (uint32_t t = 0; t < s.duration_ms; t += kStep) {
      if (s.tick_clock) advance(&s.model.time, start, t);
      if (s.radius_toast) s.model.nearest_radius_changed_ago_ms = t;
      if (s.alert) s.model.alert_ago_ms = t;
      if (s.month_change_ms >= 0 && t >= static_cast<uint32_t>(s.month_change_ms)) {
        s.model.calendar_month_offset = 1;
        s.model.calendar_changed_ago_ms = t - s.month_change_ms;
      }
      ui::i18n::g_lang = s.lang;
      const auto a = std::chrono::steady_clock::now();
      s.model.frame_buffer = frame.getBuffer();
      ui::drawPage(s.page, frame, s.model, t);
      total_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count();
      if (frames % 10 == 0) {
        renderBanded(s, t);
        const auto* full = static_cast<const uint16_t*>(frame.getBuffer());
        int diff = 0, rmin = 999, rmax = -1;
        for (int i = 0; i < 240 * 240; ++i) {
          if (banded[i] != full[i]) {
            ++diff;
            rmin = std::min(rmin, i / 240);
            rmax = std::max(rmax, i / 240);
          }
        }
        if (diff) {
          ++mismatched;
          if (mismatched <= 2) printf("   t=%u: %d px differ, rows %d..%d\n", t, diff, rmin, rmax);
        }
      }
      ui::draw::dimPixels(static_cast<uint16_t*>(frame.getBuffer()), 240, 240, 0, s.dim);
      frame.readRect(0, 0, 240, 240, rgb.data());
      fwrite(rgb.data(), 3, rgb.size(), f);
      ++frames;
    }
    fclose(f);
    fprintf(manifest, "%s %d %u\n", s.name.c_str(), frames, kStep);
    printf("%-18s %3d frames, %.2f ms/frame on this Mac, banded mismatches: %d, guard hits: %d\n",
           s.name.c_str(), frames, total_ms / frames, mismatched, guard_hits);
    guard_hits = 0;
  }
  fclose(manifest);
  return 0;
}
