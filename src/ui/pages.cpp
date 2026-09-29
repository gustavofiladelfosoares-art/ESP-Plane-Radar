#include "ui/pages.h"

namespace ui {

uint32_t drawPage(Page page, lgfx::LovyanGFX& g, const Model& m, uint32_t t_ms) {
  switch (page) {
    case Page::Radar:
      return drawRadarPage(g, m, t_ms);
    case Page::Weather:
      return drawWeatherPage(g, m, t_ms);
    case Page::Clock:
      return drawClockPage(g, m, t_ms);
    case Page::Nearest:
      return drawNearestPage(g, m, t_ms);
    case Page::AirSun:
      return drawAirSunPage(g, m, t_ms);
    case Page::About:
      return drawAboutPage(g, m, t_ms);
    default:
      return 1000;
  }
}

const char* weatherLabel(int code, bool is_day) {
  switch (code) {
    case 0:
      return is_day ? "Ensolarado" : "Céu limpo";
    case 1:
      return "Poucas nuvens";
    case 2:
      return "Parcialmente nublado";
    case 3:
      return "Nublado";
    case 45:
    case 48:
      return "Neblina";
    case 51:
    case 53:
    case 55:
      return "Garoa";
    case 56:
    case 57:
      return "Garoa gelada";
    case 61:
      return "Chuva fraca";
    case 63:
      return "Chuva";
    case 65:
      return "Chuva forte";
    case 66:
    case 67:
      return "Chuva gelada";
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86:
      return "Neve";
    case 80:
    case 81:
      return "Pancadas de chuva";
    case 82:
      return "Pancadas fortes";
    case 95:
      return "Tempestade";
    case 96:
    case 99:
      return "Tempestade c/ granizo";
    default:
      return "Tempo";
  }
}

}  // namespace ui
