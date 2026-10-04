#include "ui/pages.h"

#include "ui/i18n.h"

namespace ui {

uint32_t drawPage(Page page, lgfx::LovyanGFX& g, const Model& m, uint32_t t_ms) {
  switch (page) {
    case Page::Radar:
      return drawRadarPage(g, m, t_ms);
    case Page::Weather:
      return drawWeatherPage(g, m, t_ms);
    case Page::Forecast:
      return drawForecastPage(g, m, t_ms);
    case Page::Clock:
      return drawClockPage(g, m, t_ms);
    case Page::Calendar:
      return drawCalendarPage(g, m, t_ms);
    case Page::Agenda:
      return drawAgendaPage(g, m, t_ms);
    case Page::Nearest:
      return drawNearestPage(g, m, t_ms);
    case Page::Summary:
      return drawSummaryPage(g, m, t_ms);
    case Page::AirSun:
      return drawAirSunPage(g, m, t_ms);
    case Page::About:
      return drawAboutPage(g, m, t_ms);
    default:
      return 1000;
  }
}

const char* emergencyLabel(const services::adsb::Aircraft& a) {
  switch (a.squawk) {
    case 7500: return i18n::tr(i18n::S::Hijack);
    case 7600: return i18n::tr(i18n::S::RadioFailure);
    default: return i18n::tr(i18n::S::Emergency);
  }
}

const char* weatherLabel(int code, bool is_day) {
  using i18n::S;
  S s = S::Weather;
  switch (code) {
    case 0: s = is_day ? S::Sunny : S::ClearNight; break;
    case 1: s = S::FewClouds; break;
    case 2: s = S::PartlyCloudy; break;
    case 3: s = S::Cloudy; break;
    case 45: case 48: s = S::Fog; break;
    case 51: case 53: case 55: s = S::Drizzle; break;
    case 56: case 57: s = S::FreezingDrizzle; break;
    case 61: s = S::LightRain; break;
    case 63: s = S::Rain; break;
    case 65: s = S::HeavyRain; break;
    case 66: case 67: s = S::FreezingRain; break;
    case 71: case 73: case 75: case 77: case 85: case 86: s = S::Snow; break;
    case 80: case 81: s = S::Showers; break;
    case 82: s = S::HeavyShowers; break;
    case 95: s = S::Storm; break;
    case 96: case 99: s = S::StormHail; break;
    default: break;
  }
  return i18n::tr(s);
}

}  // namespace ui
