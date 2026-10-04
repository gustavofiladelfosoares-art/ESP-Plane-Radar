/**
 * Plane Radar — Wi-Fi setup, then animated pages on the round GC9A01 display:
 * radar, weather, next days, clock, calendar, agenda, nearest aircraft, air / UV / sun, about.
 *
 * BOOT button: tap = next page, double tap = radar range (nearest radius /
 * next month / reload agenda on those pages), hold 3 s = reset Wi-Fi.
 * All HTTP work runs in a background task (services/net_task) so animations
 * keep running while data loads.
 *
 * USB serial debug keys: n = next page, r = range, 0-8 = page, s = screenshot,
 * h = heap, w = refresh weather.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "config.h"
#include "hardware/display.h"
#include "services/adsb_client.h"
#include "services/agenda.h"
#include "services/clock.h"
#include "services/map_service.h"
#include "services/net_task.h"
#include "services/radar_location.h"
#include "services/route.h"
#include "services/shared.h"
#include "services/sky_stats.h"
#include "services/weather.h"
#include "services/wifi_setup.h"
#include "ui/color.h"
#include "ui/draw_util.h"
#include "ui/i18n.h"
#include "ui/model.h"
#include "ui/pages.h"
#include "ui/radar_range.h"
#include "ui/status_screens.h"

namespace {

// The screen is drawn in two 120-row halves through one 57.6 KB buffer (a full
// 240×240 frame would take 115 KB and leave too little RAM for HTTPS).
constexpr int kBandRows = 120;
uint16_t* s_band = nullptr;
LGFX_Sprite s_frame(&tft);
bool s_frame_ok = false;

ui::Page s_page = ui::Page::Radar;
unsigned long s_page_since = 0;
unsigned long s_next_frame = 0;

unsigned long s_near_changed_at = 0;  // 0 = never

// Next-days page: days or 12-hour chart, and when that last flipped.
constexpr unsigned long kForecastFlipMs = 10000;
bool s_fc_hours = false;
unsigned long s_fc_view_at = 0;

// Calendar: months ahead of today and when that last changed.
int s_cal_offset = 0;
unsigned long s_cal_changed_at = 0;

// Overhead alert: when it fired, and the page to go back to afterwards.
constexpr unsigned long kAlertShowMs = 30000;
constexpr unsigned long kAlertCooldownMs = 15UL * 60UL * 1000UL;  // per aircraft
unsigned long s_alert_at = 0;  // 0 = none
int s_alert_return = -1;       // page index, -1 = stay
struct AlertSeen {
  char callsign[9];
  unsigned long at;
};
AlertSeen s_alert_seen[8] = {};
ui::RouteCodes s_route_codes[12];

unsigned long g_wifi_down_since = 0;
unsigned long g_last_reconnect_ms = 0;

ui::Model s_model;

// Night mode on the clock page: current brightness (0..256), eased toward the
// target so 23:00 fades in gently; entering the page at night starts dim.
float s_dim_level = 256.0f;
unsigned long s_dim_at = 0;

// Render timing, logged every 10 s (render = draw + push to the panel).
unsigned long s_stat_since = 0;
unsigned long s_stat_ms = 0;
unsigned s_stat_frames = 0;

uint16_t nightDimTarget() {
  if (s_page != ui::Page::Clock || !s_model.time.valid ||
      !ui::radar::nightDimActive(s_model.time.hour)) {
    return 256;
  }
  return static_cast<uint16_t>(ui::radar::nightLevelPercent() * 256 / 100);
}

void updateNightDim(unsigned long now) {
  constexpr float kFadeMs = 3000.0f;  // full-range fade time
  const float target = nightDimTarget();
  const float step = 256.0f * (now - s_dim_at) / kFadeMs;
  s_dim_at = now;
  if (s_dim_level < target) {
    s_dim_level = s_dim_level + step < target ? s_dim_level + step : target;
  } else {
    s_dim_level = s_dim_level - step > target ? s_dim_level - step : target;
  }
}

void showPage(ui::Page page) {
  if (page != s_page) {
    s_cal_offset = 0;
    s_fc_hours = false;
    s_fc_view_at = millis();
    s_cal_changed_at = 0;
    if (page == ui::Page::Radar || page == ui::Page::Nearest) {
      services::net::refreshAircraft();  // other pages keep only a small area
    }
  }
  s_page = page;
  s_dim_level = 256.0f;
  s_dim_at = millis();
  s_page_since = millis();
  s_next_frame = 0;
  services::net::setActivePage(page);
  Serial.printf("Page %d\n", static_cast<int>(page));
}

void nextPage() {
  auto page = static_cast<ui::Page>((static_cast<int>(s_page) + 1) % ui::kPageCount);
  // The agenda page only joins the cycle once a Google Agenda link is set.
  if (page == ui::Page::Agenda && ui::radar::agendaUrl()[0] == '\0') {
    page = static_cast<ui::Page>((static_cast<int>(page) + 1) % ui::kPageCount);
  }
  showPage(page);
}

void cycleRange() {
  ui::radar::rangeNext();
  char label[12];
  ui::radar::formatCurrentRing3Label(label, sizeof(label));
  Serial.printf("Range: %s (outer ~%.0f km)\n", label, ui::radar::rangeCurrent().outer_km);
  if (s_page != ui::Page::Radar) {
    showPage(ui::Page::Radar);
  }
  s_next_frame = 0;
}

void cycleNearestRadius() {
  ui::radar::nearestRadiusNext();
  s_near_changed_at = millis();
  services::net::refreshAircraft();
  Serial.printf("Nearest radius: %.0f km\n", ui::radar::nearestRadiusKm());
  s_next_frame = 0;
}

void nextCalendarMonth() {
  s_cal_offset = (s_cal_offset + 1) % 12;
  s_cal_changed_at = millis();
  s_next_frame = 0;
}

/** Double tap: nearest radius, calendar month, or radar range. */
void doubleTapAction() {
  if (s_page == ui::Page::Nearest) {
    cycleNearestRadius();
  } else if (s_page == ui::Page::Calendar) {
    nextCalendarMonth();
  } else if (s_page == ui::Page::Forecast) {
    s_fc_hours = !s_fc_hours;
    s_fc_view_at = millis();
    s_next_frame = 0;
  } else if (s_page == ui::Page::Agenda) {
    services::net::refreshAgenda();
  } else {
    cycleRange();
  }
}

/**
 * Pop up the nearest page when a plane passes within the alert distance (each
 * aircraft at most once per 15 min). Not from the radar, where it is already
 * visible, and not at night while the clock is dimmed.
 */
void checkOverheadAlert() {
  const unsigned long now = millis();
  if (s_alert_at != 0 && now - s_alert_at >= kAlertShowMs) {
    s_alert_at = 0;
    if (s_alert_return >= 0 && s_page == ui::Page::Nearest) {
      showPage(static_cast<ui::Page>(s_alert_return));
    }
    s_alert_return = -1;
  }
  if (!ui::radar::alertEnabled() || s_page == ui::Page::Radar || s_alert_at != 0) {
    return;
  }
  if (s_model.time.valid && ui::radar::nightDimActive(s_model.time.hour)) {
    return;
  }
  services::SharedLock lock;
  const float cos_lat = cosf(static_cast<float>(services::location::lat()) * 0.01745329f);
  const float lim = ui::radar::alertKm();
  const auto* planes = services::adsb::aircraftList();
  for (size_t i = 0; i < services::adsb::aircraftCount(); ++i) {
    const auto& p = planes[i];
    if (p.on_ground || p.callsign[0] == '\0') continue;
    const float dx = static_cast<float>(p.lon - services::location::lon()) * 111.0f * cos_lat;
    const float dy = static_cast<float>(p.lat - services::location::lat()) * 111.0f;
    if (dx * dx + dy * dy > lim * lim) continue;
    AlertSeen* slot = &s_alert_seen[0];
    bool recent = false;
    for (auto& e : s_alert_seen) {
      if (strcmp(e.callsign, p.callsign) == 0 && e.at != 0 && now - e.at < kAlertCooldownMs) {
        recent = true;
        break;
      }
      if (e.at < slot->at) slot = &e;  // oldest entry gets replaced
    }
    if (recent) continue;
    snprintf(slot->callsign, sizeof(slot->callsign), "%s", p.callsign);
    slot->at = now;
    Serial.printf("Overhead alert: %s\n", p.callsign);
    if (s_page != ui::Page::Nearest) {
      s_alert_return = static_cast<int>(s_page);
      showPage(ui::Page::Nearest);
    }
    s_alert_at = now;
    return;
  }
}

/**
 * Optional slideshow: move to the next page every N seconds (setup portal).
 * Skips the about page and never interrupts an overhead alert.
 */
void autoAdvance() {
  const uint16_t sec = ui::radar::autoPageSec();
  if (sec == 0 || s_alert_at != 0 || millis() - s_page_since < sec * 1000UL) {
    return;
  }
  nextPage();
  if (s_page == ui::Page::About) {
    nextPage();
  }
}

void handleGestures() {
  switch (bootButtonConsumeGesture()) {
    case BootGesture::Tap:
      s_alert_return = -1;  // the user took over
      nextPage();
      break;
    case BootGesture::DoubleTap:
      s_alert_return = -1;
      doubleTapAction();
      break;
    default:
      break;
  }
}

bool s_dump_next = false;

/** Send one rendered band over serial (screenshot is assembled from both). */
void dumpBand(int y0) {
  if (y0 == 0) {
    Serial.print("\n@@SHOT 240 240\n");
  }
  // Raw RGB565, big-endian as LovyanGFX stores it (scripts/device_tool.py).
  Serial.write(reinterpret_cast<const uint8_t*>(s_band), 240 * kBandRows * 2);
  if (y0 + kBandRows >= 240) {
    Serial.print("\n@@END\n");
  }
}

void handleSerial() {
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c == 'n') {
      nextPage();
    } else if (c == 'r') {
      doubleTapAction();
    } else if (c >= '0' && c < '0' + ui::kPageCount) {
      showPage(static_cast<ui::Page>(c - '0'));
    } else if (c == 's') {
      s_dump_next = true;
      s_next_frame = 0;
    } else if (c == 'h') {
      Serial.printf("heap free %u, min %u, largest block %u\n", ESP.getFreeHeap(),
                    ESP.getMinFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    } else if (c == 'w') {
      services::net::refreshWeather();
    }
  }
}

void buildModel() {
  ui::Model& m = s_model;
  m.wifi_ok = WiFi.status() == WL_CONNECTED;
  m.lat = services::location::lat();
  m.lon = services::location::lon();
  services::weather::snapshot(&m.weather, &m.air, &m.sun);
  services::weather::forecastSnapshot(&m.forecast, &m.hourly);
  // Next-days page flips between days and hours every 10 s (double tap: now).
  if (s_page == ui::Page::Forecast && millis() - s_fc_view_at >= kForecastFlipMs) {
    s_fc_hours = !s_fc_hours;
    s_fc_view_at = millis();
  }
  m.forecast_hours = s_fc_hours;
  m.forecast_view_ago_ms = static_cast<uint32_t>(millis() - s_fc_view_at);
  services::clock::now(&m.time);
  services::route::snapshot(&m.route);
  services::agenda::snapshot(&m.agenda);
  services::stats::snapshot(&m.summary);  // SharedLock is held by renderFrame()
  // Aircraft are read in place; renderFrame() holds SharedLock meanwhile.
  m.plane_count = services::adsb::aircraftCount();
  m.planes = services::adsb::aircraftList();
  m.radar_map = nullptr;
  m.route_code_count = services::route::copyCodes(s_route_codes, 12);
  m.route_codes = s_route_codes;
  m.nearest_radius_km = ui::radar::nearestRadiusKm();
  m.nearest_radius_changed_ago_ms =
      s_near_changed_at == 0 ? 0xFFFFFFFFu : static_cast<uint32_t>(millis() - s_near_changed_at);
  m.alert_ago_ms = s_alert_at == 0 ? 0xFFFFFFFFu : static_cast<uint32_t>(millis() - s_alert_at);
  if (s_alert_at != 0 && ui::radar::alertKm() > m.nearest_radius_km) {
    m.nearest_radius_km = ui::radar::alertKm();  // keep the alerted plane in reach
  }
  m.calendar_month_offset = s_cal_offset;
  m.calendar_changed_ago_ms =
      s_cal_changed_at == 0 ? 0xFFFFFFFFu : static_cast<uint32_t>(millis() - s_cal_changed_at);
  m.show_holidays = ui::radar::showHolidays();

  if (s_page == ui::Page::Nearest) {
    float dist = 0.0f;
    float bearing = 0.0f;
    const int i = ui::nearestPlane(m, &dist, &bearing);
    if (i >= 0) {
      services::route::request(m.planes[i].callsign);
    }
  }
}

void renderFrame() {
  const unsigned long start = millis();
  ui::g_swap_rb = ui::radar::swapColors();
  ui::i18n::g_lang = static_cast<ui::i18n::Lang>(ui::radar::language());
  services::SharedLock lock;  // aircraft list is used in place while drawing
  buildModel();
  if (start - s_page_since < 60) {
    s_dim_level = nightDimTarget();  // first frame of a page: no bright flash
  }
  updateNightDim(start);
  const uint16_t dim = static_cast<uint16_t>(s_dim_level + 0.5f);

  const uint32_t t = start - s_page_since;
  uint32_t wait = 0;
  // The map lives in flash shared with the network task; hold it while drawing.
  if (s_page == ui::Page::Radar) {
    s_model.radar_map =
        services::map::lockCurrent(s_model.lat, s_model.lon, ui::radar::rangeIndex());
  }
  if (s_frame_ok) {
    for (int y0 = 0; y0 < config::kDisplayHeight; y0 += kBandRows) {
      // Address the band buffer as if it were a full frame starting at row 0,
      // and clip to the rows it actually holds.
      uint16_t* base = s_band - y0 * config::kDisplayWidth;
      s_frame.setBuffer(base, config::kDisplayWidth, config::kDisplayHeight, 16);
      s_frame.setClipRect(0, y0, config::kDisplayWidth, kBandRows);
      s_model.frame_buffer = base;
      s_model.band_y0 = y0;
      s_model.band_y1 = y0 + kBandRows;
      wait = ui::drawPage(s_page, s_frame, s_model, t);
      ui::draw::dimPixels(s_band, config::kDisplayWidth, kBandRows, y0, dim);
      tft.pushImage(0, y0, config::kDisplayWidth, kBandRows,
                    reinterpret_cast<const lgfx::swap565_t*>(s_band));
      if (s_dump_next) {
        dumpBand(y0);
      }
    }
    s_dump_next = false;
  } else {
    s_model.frame_buffer = nullptr;
    wait = ui::drawPage(s_page, tft, s_model, t);
  }
  if (s_page == ui::Page::Radar) {
    services::map::unlock();
  }
  s_next_frame = start + wait;

  s_stat_ms += millis() - start;
  ++s_stat_frames;
  if (millis() - s_stat_since >= 10000) {
    Serial.printf("ui: page %d, %u frames, %lu ms/frame, heap %u\n", static_cast<int>(s_page),
                  s_stat_frames, s_stat_ms / (s_stat_frames ? s_stat_frames : 1), ESP.getFreeHeap());
    if (s_page == ui::Page::Radar && s_stat_frames > 0) {
      uint32_t us[ui::kRadarStages];
      ui::radarProfile(us);
      Serial.print("radar ms/frame:");
      for (int i = 0; i < ui::kRadarStages - 1; ++i) {
        Serial.printf(" %s=%lu", ui::kRadarStageNames[i], us[i] / 1000 / s_stat_frames);
      }
      Serial.println();
    }
    s_stat_since = millis();
    s_stat_ms = 0;
    s_stat_frames = 0;
  }
}

void handleWifiDown() {
  if (g_wifi_down_since == 0) {
    g_wifi_down_since = millis();
    Serial.println("WiFi lost — will reconnect");
  }
  const unsigned long down_ms = millis() - g_wifi_down_since;
  if (down_ms >= config::kWifiDownGraceMs &&
      millis() - g_last_reconnect_ms >= config::kWifiReconnectIntervalMs) {
    g_last_reconnect_ms = millis();
    if (wifiReconnect()) {
      g_wifi_down_since = 0;
    }
    s_next_frame = 0;  // status screen drew over the panel; repaint
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("ESP Plane Radar v2.2.0 — by Gustavo Soares");

  services::sharedInit();
  bootButtonInit();
  displayInit();

  // Grab the half-screen buffer before Wi-Fi fragments the heap.
  s_band = static_cast<uint16_t*>(heap_caps_malloc(config::kDisplayWidth * kBandRows * 2,
                                                   MALLOC_CAP_8BIT | MALLOC_CAP_DMA));
  s_frame_ok = s_band != nullptr;
  if (!s_frame_ok) {
    Serial.println("frame buffer alloc failed — drawing directly (may flicker)");
  }

  services::location::init();
  ui::radar::rangeInit();
  ui::g_swap_rb = ui::radar::swapColors();
  ui::i18n::g_lang = static_cast<ui::i18n::Lang>(ui::radar::language());
  if (wifiShowsSetupScreenOnBoot()) {
    statusScreenPortal();
  }
  services::map::init();

  wifiSetupConnect();
  services::net::start();
  showPage(ui::Page::Radar);
}

void loop() {
  handleGestures();
  bootButtonPollLongPress();
  wifiLoop();
  handleSerial();

  if (WiFi.status() != WL_CONNECTED) {
    handleWifiDown();
  } else {
    g_wifi_down_since = 0;
  }

  if (WiFi.status() == WL_CONNECTED) {
    checkOverheadAlert();
  }
  autoAdvance();
  if (millis() >= s_next_frame) {
    renderFrame();
  }
  delay(1);
}
