/**
 * Plane Radar — Wi-Fi setup, then five animated pages on the round GC9A01
 * display: radar, weather, clock, nearest aircraft, air / UV / sun.
 *
 * BOOT button: tap = next page, double tap = radar range, hold 3 s = reset Wi-Fi.
 * All HTTP work runs in a background task (services/net_task) so animations
 * keep running while data loads.
 *
 * USB serial debug keys: n = next page, r = range, 0-4 = page, s = screenshot,
 * h = heap, w = refresh weather.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "config.h"
#include "hardware/display.h"
#include "services/adsb_client.h"
#include "services/clock.h"
#include "services/map_service.h"
#include "services/net_task.h"
#include "services/radar_location.h"
#include "services/route.h"
#include "services/shared.h"
#include "services/weather.h"
#include "services/wifi_setup.h"
#include "ui/color.h"
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
ui::RouteCodes s_route_codes[12];

unsigned long g_wifi_down_since = 0;
unsigned long g_last_reconnect_ms = 0;

ui::Model s_model;

// Render timing, logged every 10 s (render = draw + push to the panel).
unsigned long s_stat_since = 0;
unsigned long s_stat_ms = 0;
unsigned s_stat_frames = 0;

void showPage(ui::Page page) {
  s_page = page;
  s_page_since = millis();
  s_next_frame = 0;
  services::net::setActivePage(page);
  Serial.printf("Page %d\n", static_cast<int>(page));
}

void nextPage() {
  showPage(static_cast<ui::Page>((static_cast<int>(s_page) + 1) % ui::kPageCount));
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

void handleGestures() {
  switch (bootButtonConsumeGesture()) {
    case BootGesture::Tap:
      nextPage();
      break;
    case BootGesture::DoubleTap:
      // On the nearest-aircraft page a double tap widens/narrows the search;
      // everywhere else it cycles the radar range.
      if (s_page == ui::Page::Nearest) {
        cycleNearestRadius();
      } else {
        cycleRange();
      }
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
      if (s_page == ui::Page::Nearest) {
        cycleNearestRadius();
      } else {
        cycleRange();
      }
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
  services::clock::now(&m.time);
  services::route::snapshot(&m.route);
  // Aircraft are read in place; renderFrame() holds SharedLock meanwhile.
  m.plane_count = services::adsb::aircraftCount();
  m.planes = services::adsb::aircraftList();
  m.radar_map = nullptr;
  m.route_code_count = services::route::copyCodes(s_route_codes, 12);
  m.route_codes = s_route_codes;
  m.nearest_radius_km = ui::radar::nearestRadiusKm();
  m.nearest_radius_changed_ago_ms =
      s_near_changed_at == 0 ? 0xFFFFFFFFu : static_cast<uint32_t>(millis() - s_near_changed_at);

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
  Serial.println("Plane Radar");

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

  if (millis() >= s_next_frame) {
    renderFrame();
  }
  delay(1);
}
