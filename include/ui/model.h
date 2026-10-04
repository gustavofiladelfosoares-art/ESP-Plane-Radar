#pragma once

#include <cstddef>
#include <cstdint>

#include "services/adsb_client.h"

namespace ui {

/** Current conditions + today's forecast (Open-Meteo). */
struct WeatherModel {
  bool valid = false;
  float temp_c = 0.0f;
  float feels_c = 0.0f;
  int humidity = 0;
  float wind_kmh = 0.0f;
  int code = 0;  // WMO weather interpretation code
  bool is_day = true;
  float t_min = 0.0f;
  float t_max = 0.0f;
  int rain_prob = 0;
};

/** One day of the multi-day forecast. */
struct ForecastDay {
  int wday = 0;  // 0 = Sunday
  int code = 0;  // WMO weather code
  float t_min = 0.0f;
  float t_max = 0.0f;
  int rain_prob = 0;
};

/** The next five days (tomorrow onward). */
struct ForecastModel {
  bool valid = false;
  int count = 0;
  ForecastDay days[5];
};

/** The next 12 hours (from the current hour). */
struct HourlyModel {
  bool valid = false;
  int count = 0;
  int hour[12] = {};
  float temp_c[12] = {};
  int rain_prob[12] = {};
};

/** Air quality and UV (Open-Meteo air-quality API). */
struct AirModel {
  bool valid = false;
  int aqi = 0;  // US AQI
  float uv = 0.0f;
};

/** Today's sunrise/sunset as minutes after local midnight. */
struct SunModel {
  bool valid = false;
  int sunrise_min = 0;
  int sunset_min = 0;
};

/** Local wall-clock time. */
struct TimeModel {
  bool valid = false;
  int year = 0;
  int month = 1;  // 1..12
  int day = 1;
  int wday = 0;   // 0 = Sunday
  int hour = 0;
  int minute = 0;
  int second = 0;
  int millis = 0;
};

/** Flight route looked up by callsign (adsbdb). */
struct RouteModel {
  bool valid = false;
  char callsign[9] = "";
  char airline[32] = "";
  char from_iata[4] = "";
  char to_iata[4] = "";
  char from_city[28] = "";
  char to_city[28] = "";
};

/** Airport codes of a known route, for radar tags ("CNF → VCP"). */
struct RouteCodes {
  char callsign[9] = "";
  char from[4] = "";
  char to[4] = "";
};

/** Today's Google Calendar events and Google Tasks (agenda page). */
struct AgendaItem {
  bool task = false;  // false = calendar event
  bool done = false;  // tasks only
  char time[6] = "";  // "09:30", "" = all day / no time
  char title[44] = "";
};

struct AgendaModel {
  bool configured = false;  // a link was saved in the setup portal
  bool valid = false;       // a fetch succeeded
  int count = 0;
  AgendaItem items[12];
};

/** Today's sky so far: what the device has seen since midnight. */
struct SummaryModel {
  int seen = 0;  // distinct aircraft
  char highest_cs[9] = "";
  int highest_ft = 0;
  char fastest_cs[9] = "";
  int fastest_kmh = 0;
  char closest_cs[9] = "";
  float closest_km = 0.0f;
  char airline[20] = "";  // most seen airline (name or ICAO code)
  int airline_count = 0;
};

/** Everything a page needs to draw one frame. */
struct Model {
  bool wifi_ok = false;
  double lat = 0.0;
  double lon = 0.0;
  WeatherModel weather;
  ForecastModel forecast;
  HourlyModel hourly;
  /** Next-days page: showing the 12-hour chart, and ms since the view flipped. */
  bool forecast_hours = false;
  uint32_t forecast_view_ago_ms = 0xFFFFFFFFu;
  AirModel air;
  SunModel sun;
  TimeModel time;
  RouteModel route;
  AgendaModel agenda;
  SummaryModel summary;
  const services::adsb::Aircraft* planes = nullptr;
  size_t plane_count = 0;
  const RouteCodes* route_codes = nullptr;
  size_t route_code_count = 0;
  /** Nearest-aircraft page: search radius and ms since it was last changed. */
  float nearest_radius_km = 50.0f;
  uint32_t nearest_radius_changed_ago_ms = 0xFFFFFFFFu;
  /**
   * Overhead alert: ms since an aircraft passing close overhead made the
   * nearest page pop up (0xFFFFFFFF = no alert).
   */
  uint32_t alert_ago_ms = 0xFFFFFFFFu;
  /** Calendar: months ahead of today being shown, and ms since that changed. */
  int calendar_month_offset = 0;
  uint32_t calendar_changed_ago_ms = 0xFFFFFFFFu;
  bool show_holidays = true;
  /** 2-bit radar map for the active range (nullptr = no map yet). */
  const uint8_t* radar_map = nullptr;
  /**
   * Raw 16-bit buffer being drawn into (for fast pixel blits), if any. It is
   * addressed as a full 240-row frame, but only rows band_y0..band_y1-1 are
   * backed by memory (the firmware renders the screen in two halves).
   */
  void* frame_buffer = nullptr;
  int band_y0 = 0;
  int band_y1 = 240;
};

}  // namespace ui
