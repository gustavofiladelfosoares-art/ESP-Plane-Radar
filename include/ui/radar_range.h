#pragma once

#include <cstddef>
#include <cstdint>

namespace ui::radar {

/**
 * Range presets (label on ring 3 = ¾ of outer radius).
 *
 * Recommended for ADS-B on a 1.28″ display:
 *   5 km  — pattern / very local (airfield vicinity)
 *  10 km  — default; neighborhood spotting
 *  15 km  — wider local area
 *  25 km  — metro / regional picture
 *
 * Outer radius (for aircraft math) is ring-3 distance ÷ 0.75.
 */
struct RangePreset {
  /** Distance shown on ring 3 (¾ of outer radius), always stored in km. */
  float ring3_km;
  float outer_km;
};

constexpr float kRing3ToOuterKm = 4.0f / 3.0f;

constexpr RangePreset kRangePresets[] = {
    {5.0f, 5.0f * kRing3ToOuterKm},
    {10.0f, 10.0f * kRing3ToOuterKm},
    {15.0f, 15.0f * kRing3ToOuterKm},
    {25.0f, 25.0f * kRing3ToOuterKm},
    // Wider views for areas where airports are farther away.
    {50.0f, 50.0f * kRing3ToOuterKm},
    {100.0f, 100.0f * kRing3ToOuterKm},
};

constexpr size_t kRangePresetCount =
    sizeof(kRangePresets) / sizeof(kRangePresets[0]);

/** Search radius choices for the nearest-aircraft page (double tap cycles). */
constexpr float kNearestRadiiKm[] = {5.0f, 10.0f, 20.0f, 50.0f};
/** Longest Google Agenda link we keep. */
constexpr size_t kAgendaUrlMax = 320;

constexpr size_t kNearestRadiusCount = sizeof(kNearestRadiiKm) / sizeof(kNearestRadiiKm[0]);

/** Load saved range and distance units from flash. Call once after boot. */
void rangeInit();
/** Cycle preset and save to flash. */
void rangeNext();
const RangePreset& rangeCurrent();
uint8_t rangeIndex();
/** Radius (km) of the visible screen edge for the current range. */
float fetchRadiusKm();
/**
 * Radius (km) to request aircraft for the radar: the screen edge of the next
 * wider range, so the rim dots show exactly what zooming out would reveal.
 */
float adsbRadiusKm();

/** Nearest-aircraft search radius (km) and cycling it (saved to flash). */
float nearestRadiusKm();
void nearestRadiusNext();

bool useMiles();
bool showRunways();
/** Screen language index (ui::i18n::Lang); settable from the portal. */
uint8_t language();
void saveLanguageFromPortal(const char* value);
/** Rotating sweep line on the radar (off by default); settable from the portal. */
bool showSweep();
void saveSweepFromPortal(const char* checkbox_value);
/**
 * Night mode for the clock page: dimmer between start and end hour (local
 * time, may wrap past midnight). Level is the night brightness in percent.
 */
bool nightDimEnabled();
uint8_t nightStartHour();
uint8_t nightEndHour();
uint8_t nightLevelPercent();
/** True when the clock page should be dimmed at this local hour. */
bool nightDimActive(int hour);
void saveNightFromPortal(const char* checkbox_value, const char* start, const char* end,
                         const char* level);
/**
 * Overhead alert: jump to the nearest-aircraft page when a plane passes
 * within alertKm() of home. Holidays: Brazilian holidays on the calendar.
 */
bool alertEnabled();
float alertKm();
bool showHolidays();
void saveAlertFromPortal(const char* checkbox_value, const char* km);
void saveHolidaysFromPortal(const char* checkbox_value);
/**
 * Google Agenda link (a Google Apps Script web app, see tools/google-agenda).
 * Kept only in this device's flash; empty = not configured.
 */
const char* agendaUrl();
void saveAgendaFromPortal(const char* url);
/** Seconds between automatic page changes (0 = off). */
uint16_t autoPageSec();
void saveAutoPageFromPortal(const char* seconds);
/** Swap red/blue in page colors (panel quirk); settable from the portal. */
bool swapColors();
void saveSwapColorsFromPortal(const char* checkbox_value);
/** WiFi portal checkbox: "T" = miles, otherwise km. */
void saveMilesFromPortal(const char* checkbox_value);
void saveRunwaysFromPortal(const char* checkbox_value);
void formatRing3Label(char* buf, size_t len, float ring3_km, bool use_miles);
void formatCurrentRing3Label(char* buf, size_t len);
/** Reset distance units to km (e.g. with WiFi credential wipe). */
void unitsReset();

}  // namespace ui::radar
