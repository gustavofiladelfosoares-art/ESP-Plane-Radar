#include "services/weather.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>

#include "services/clock.h"
#include "services/http_util.h"
#include "services/shared.h"

namespace services::weather {

namespace {

ui::WeatherModel s_weather;
ui::AirModel s_air;
ui::SunModel s_sun;

/** "2026-09-29T05:48" -> minutes after midnight (or -1). */
int isoMinutes(const char* iso) {
  if (iso == nullptr) return -1;
  const char* t = strchr(iso, 'T');
  int h = 0;
  int m = 0;
  if (t == nullptr || sscanf(t + 1, "%d:%d", &h, &m) != 2) return -1;
  return h * 60 + m;
}

}  // namespace

bool fetchForecast(double lat, double lon) {
  char url[400];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f"
           "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,"
           "is_day,wind_speed_10m"
           "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max,"
           "sunrise,sunset&timezone=auto&forecast_days=1",
           lat, lon);
  JsonDocument doc;
  if (!http::getJson(url, "weather", doc)) {
    return false;
  }
  JsonObject cur = doc["current"];
  JsonObject day = doc["daily"];
  if (cur.isNull() || day.isNull()) {
    Serial.println("weather: missing fields");
    return false;
  }

  ui::WeatherModel w;
  w.valid = true;
  w.temp_c = cur["temperature_2m"] | 0.0f;
  w.feels_c = cur["apparent_temperature"] | w.temp_c;
  w.humidity = cur["relative_humidity_2m"] | 0;
  w.wind_kmh = cur["wind_speed_10m"] | 0.0f;
  w.code = cur["weather_code"] | 0;
  w.is_day = (cur["is_day"] | 1) != 0;
  w.t_max = day["temperature_2m_max"][0] | w.temp_c;
  w.t_min = day["temperature_2m_min"][0] | w.temp_c;
  w.rain_prob = day["precipitation_probability_max"][0] | 0;

  ui::SunModel sun;
  sun.sunrise_min = isoMinutes(day["sunrise"][0]);
  sun.sunset_min = isoMinutes(day["sunset"][0]);
  sun.valid = sun.sunrise_min >= 0 && sun.sunset_min > sun.sunrise_min;

  {
    SharedLock lock;
    s_weather = w;
    s_sun = sun;
  }
  if (doc["utc_offset_seconds"].is<long>()) {
    clock::setUtcOffset(doc["utc_offset_seconds"].as<long>());
  }
  Serial.printf("weather: %.1fC feels %.1fC code %d, %s\n", w.temp_c, w.feels_c, w.code,
                w.is_day ? "day" : "night");
  return true;
}

bool fetchAir(double lat, double lon) {
  char url[256];
  snprintf(url, sizeof(url),
           "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.5f"
           "&longitude=%.5f&current=us_aqi,uv_index&timezone=auto",
           lat, lon);
  JsonDocument doc;
  if (!http::getJson(url, "air", doc)) {
    return false;
  }
  JsonObject cur = doc["current"];
  if (cur.isNull() || cur["us_aqi"].isNull()) {
    Serial.println("air: missing fields");
    return false;
  }
  ui::AirModel a;
  a.valid = true;
  a.aqi = cur["us_aqi"] | 0;
  a.uv = cur["uv_index"] | 0.0f;
  {
    SharedLock lock;
    s_air = a;
  }
  Serial.printf("air: AQI %d, UV %.1f\n", a.aqi, a.uv);
  return true;
}

void snapshot(ui::WeatherModel* weather, ui::AirModel* air, ui::SunModel* sun) {
  SharedLock lock;
  *weather = s_weather;
  *air = s_air;
  *sun = s_sun;
}

}  // namespace services::weather
