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

/** Everything a page needs to draw one frame. */
struct Model {
  bool wifi_ok = false;
  double lat = 0.0;
  double lon = 0.0;
  WeatherModel weather;
  AirModel air;
  SunModel sun;
  TimeModel time;
  RouteModel route;
  const services::adsb::Aircraft* planes = nullptr;
  size_t plane_count = 0;
  const RouteCodes* route_codes = nullptr;
  size_t route_code_count = 0;
  /** Nearest-aircraft page: search radius and ms since it was last changed. */
  float nearest_radius_km = 50.0f;
  uint32_t nearest_radius_changed_ago_ms = 0xFFFFFFFFu;
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
