#include "services/net_task.h"

#include <Arduino.h>
#include <WiFi.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>

#include "config.h"
#include "services/adsb_client.h"
#include <cstdio>
#include <cstring>

#include "services/agenda.h"
#include "services/clock.h"
#include "services/map_service.h"
#include "services/radar_location.h"
#include "services/route.h"
#include "services/shared.h"
#include "services/sky_stats.h"
#include "services/weather.h"
#include "ui/radar_range.h"

namespace services::net {

namespace {

volatile ui::Page s_page = ui::Page::Radar;
volatile bool s_force_weather = false;
volatile bool s_force_agenda = false;

/** Feed the fresh aircraft list into today's sky summary. */
void countForStats(double lat, double lon) {
  ui::TimeModel tm;
  clock::now(&tm);
  if (!tm.valid) return;
  SharedLock lock;
  stats::record(adsb::aircraftList(), adsb::aircraftCount(), lat, lon,
                (tm.year * 13 + tm.month) * 32 + tm.day);
}
volatile bool s_force_adsb = false;

bool due(unsigned long last, unsigned long interval, bool ever) {
  return !ever || millis() - last >= interval;
}

void taskMain(void*) {
  unsigned long last_adsb = 0;
  unsigned long last_weather = 0;
  unsigned long last_air = 0;
  bool weather_ok = false;
  bool weather_tried = false;
  bool air_ok = false;
  bool air_tried = false;
  double weather_lat = NAN;
  double weather_lon = NAN;

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    const double lat = location::lat();
    const double lon = location::lon();
    clock::begin(lon);

    const ui::Page page = s_page;
    const bool wants_planes = page == ui::Page::Radar || page == ui::Page::Nearest;
    if (wants_planes && (s_force_adsb || millis() - last_adsb >= config::kAdsbFetchIntervalMs)) {
      s_force_adsb = false;
      last_adsb = millis();
      // The nearest page searches its own radius; the radar fills its screen.
      const float radius_km = page == ui::Page::Nearest ? ui::radar::nearestRadiusKm()
                                                        : ui::radar::adsbRadiusKm();
      if (adsb::fetchUpdate(lat, lon, radius_km)) countForStats(lat, lon);
    } else if (!wants_planes && ui::radar::alertEnabled() &&
               millis() - last_adsb >= config::kAdsbAlertFetchIntervalMs) {
      // Other pages: just a small area around home, for the overhead alert.
      last_adsb = millis();
      if (adsb::fetchUpdate(lat, lon, ui::radar::alertKm() + 2.0f)) countForStats(lat, lon);
    }
    // Routes: the nearest page's aircraft, and radar tags on close zooms.
    const bool radar_routes = page == ui::Page::Radar && ui::radar::rangeCurrent().ring3_km <= 25.0f;
    if (page == ui::Page::Nearest || radar_routes) {
      route::service(radar_routes);
    }

    // A new location from the portal refreshes everything right away.
    if (lat != weather_lat || lon != weather_lon || s_force_weather) {
      s_force_weather = false;
      weather_tried = air_tried = false;
      weather_lat = lat;
      weather_lon = lon;
    }
    if (due(last_weather, weather_ok ? config::kWeatherFetchIntervalMs : config::kWeatherRetryIntervalMs,
            weather_tried)) {
      weather_tried = true;
      last_weather = millis();
      weather_ok = weather::fetchForecast(lat, lon);
    }
    if (due(last_air, air_ok ? config::kAirFetchIntervalMs : config::kWeatherRetryIntervalMs, air_tried)) {
      air_tried = true;
      last_air = millis();
      air_ok = weather::fetchAir(lat, lon);
    }

    // Google Agenda: every 10 min (2 min after a failure), right away when the
    // link changes or the agenda page opens with old data.
    {
      static unsigned long last_agenda = 0;
      static bool agenda_ok = false;
      static char agenda_url[ui::radar::kAgendaUrlMax + 1] = "";
      static char url[ui::radar::kAgendaUrlMax + 1];
      {
        SharedLock lock;  // the setup portal may be writing it
        snprintf(url, sizeof(url), "%s", ui::radar::agendaUrl());
      }
      const bool changed = strcmp(url, agenda_url) != 0;
      const unsigned long every = agenda_ok ? config::kAgendaFetchIntervalMs : config::kWeatherRetryIntervalMs;
      const bool stale = page == ui::Page::Agenda && millis() - last_agenda > config::kAgendaPageRefreshMs;
      if (changed || s_force_agenda ||
          (url[0] != '\0' && (last_agenda == 0 || millis() - last_agenda >= every || stale))) {
        s_force_agenda = false;
        snprintf(agenda_url, sizeof(agenda_url), "%s", url);
        last_agenda = millis();
        agenda_ok = agenda::fetch(url);
      }
    }

    // Maps only matter on the radar; build them when nothing else is pending.
    if (!wants_planes || millis() - last_adsb < config::kAdsbFetchIntervalMs - 1000) {
      map::service(lat, lon, ui::radar::rangeIndex());
    }

    static unsigned long last_report = 0;
    if (millis() - last_report > 60000) {
      last_report = millis();
      Serial.printf("net: stack headroom %u bytes, heap %u, largest %u\n",
                    static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)), ESP.getFreeHeap(),
                    static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

}  // namespace

void start() {
  // TLS needs a roomy stack; same priority as loop() so both get time slices.
  xTaskCreate(taskMain, "net", 9216, nullptr, 1, nullptr);
}

void setActivePage(ui::Page page) { s_page = page; }

void refreshWeather() { s_force_weather = true; }

void refreshAircraft() { s_force_adsb = true; }

void refreshAgenda() { s_force_agenda = true; }

}  // namespace services::net
