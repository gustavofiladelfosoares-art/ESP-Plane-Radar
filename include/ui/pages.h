#pragma once

#include <cstdint>

#include <LovyanGFX.hpp>

#include "ui/model.h"

namespace ui {

enum class Page : uint8_t { Radar, Weather, Clock, Nearest, AirSun, About, Count };

constexpr int kPageCount = static_cast<int>(Page::Count);

/**
 * Draw one frame of a page into g. t_ms counts from the moment the page was
 * opened (entry animations key off it). Returns how many ms until the next
 * frame is worth drawing.
 */
uint32_t drawPage(Page page, lgfx::LovyanGFX& g, const Model& m, uint32_t t_ms);

uint32_t drawRadarPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t_ms);
uint32_t drawWeatherPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t_ms);
uint32_t drawClockPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t_ms);
uint32_t drawNearestPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t_ms);
uint32_t drawAirSunPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t_ms);
uint32_t drawAboutPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t_ms);

/** Radar render profile: accumulated µs per stage since the last call (reset on read). */
constexpr int kRadarStages = 8;
extern const char* const kRadarStageNames[kRadarStages];
void radarProfile(uint32_t out_us[kRadarStages]);

/** Portuguese label for a WMO weather code. */
const char* weatherLabel(int code, bool is_day);

/**
 * Closest airborne aircraft to the radar center. Returns its index in
 * m.planes (or -1), with distance (km) and bearing from home (deg, 0 = N).
 */
int nearestPlane(const Model& m, float* dist_km, float* bearing_deg);

}  // namespace ui
