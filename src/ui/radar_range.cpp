#include "ui/radar_range.h"

#include "config.h"
#include "ui/radar_theme.h"

#include <Preferences.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace ui::radar {

namespace {

constexpr char kPrefsNamespace[] = "planeradar";
constexpr char kPrefsRangeKey[] = "rangeIdx";
constexpr char kPrefsMilesKey[] = "useMiles";
constexpr char kPrefsRunwaysKey[] = "showRwys";
constexpr char kPrefsSwapKey[] = "swapRB";
constexpr char kPrefsNearKey[] = "nearIdx";
constexpr char kPrefsSweepKey[] = "sweep";
constexpr char kPrefsLangKey[] = "lang";
constexpr char kPrefsNightKey[] = "nightOn";
constexpr char kPrefsNightStartKey[] = "nightFrom";
constexpr char kPrefsNightEndKey[] = "nightTo";
constexpr char kPrefsNightLevelKey[] = "nightLvl";
constexpr uint8_t kDefaultNightStart = 23;
constexpr uint8_t kDefaultNightEnd = 7;
constexpr uint8_t kDefaultNightLevel = 20;  // percent
constexpr char kPrefsAlertKey[] = "alertOn";
constexpr char kPrefsAlertKmKey[] = "alertKm";
constexpr char kPrefsHolidaysKey[] = "holidays";
constexpr uint8_t kDefaultAlertKm = 3;
constexpr char kPrefsAgendaKey[] = "agendaUrl";
constexpr char kPrefsAutoPageKey[] = "autoPage";
constexpr uint8_t kDefaultNearIndex = 3;  // 50 km
constexpr uint8_t kDefaultRangeIndex = 1;  // 10 km ring
constexpr float kKmPerMile = 1.609344f;

Preferences s_prefs;
uint8_t s_range_index = kDefaultRangeIndex;
bool s_use_miles = false;
bool s_show_runways = true;
bool s_swap_colors = config::kDisplayRgbOrder;
uint8_t s_near_index = kDefaultNearIndex;
bool s_show_sweep = false;
uint8_t s_language = 0;  // Português
bool s_night_on = true;
uint8_t s_night_start = kDefaultNightStart;
uint8_t s_night_end = kDefaultNightEnd;
uint8_t s_night_level = kDefaultNightLevel;
bool s_alert_on = true;
uint8_t s_alert_km = kDefaultAlertKm;
bool s_holidays = true;
uint16_t s_auto_page_sec = 0;
char s_agenda_url[kAgendaUrlMax + 1] = "";

/** Parse an integer portal field, falling back when empty or out of range. */
uint8_t parseRange(const char* value, int lo, int hi, uint8_t fallback) {
  if (value == nullptr || value[0] == '\0') {
    return fallback;
  }
  char* end = nullptr;
  const long v = strtol(value, &end, 10);
  if (end == value || v < lo || v > hi) {
    return fallback;
  }
  return static_cast<uint8_t>(v);
}

void saveRangeIndex() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putUChar(kPrefsRangeKey, s_range_index);
  s_prefs.end();
}

void saveUseMiles() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(kPrefsMilesKey, s_use_miles);
  s_prefs.end();
}

void saveSwapColors() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(kPrefsSwapKey, s_swap_colors);
  s_prefs.end();
}

void saveShowRunways() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(kPrefsRunwaysKey, s_show_runways);
  s_prefs.end();
}

bool portalCheckboxChecked(const char* value) {
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  // WiFiManager checkbox submits its value= attribute ("T", or "F" if we prefilled F).
  if ((value[0] == 'T' || value[0] == 't' || value[0] == 'F' || value[0] == 'f') &&
      value[1] == '\0') {
    return true;
  }
  return strcmp(value, "on") == 0;
}

}  // namespace

void rangeInit() {
  if (!s_prefs.begin(kPrefsNamespace, true)) {
    return;
  }
  const uint8_t saved = s_prefs.getUChar(kPrefsRangeKey, kDefaultRangeIndex);
  s_range_index =
      (saved < kRangePresetCount) ? saved : kDefaultRangeIndex;
  s_use_miles = s_prefs.getBool(kPrefsMilesKey, false);
  s_show_runways = s_prefs.getBool(kPrefsRunwaysKey, true);
  s_swap_colors = s_prefs.getBool(kPrefsSwapKey, config::kDisplayRgbOrder);
  s_show_sweep = s_prefs.getBool(kPrefsSweepKey, false);
  s_language = s_prefs.getUChar(kPrefsLangKey, 0);
  if (s_language > 3) s_language = 0;
  s_night_on = s_prefs.getBool(kPrefsNightKey, true);
  s_night_start = s_prefs.getUChar(kPrefsNightStartKey, kDefaultNightStart) % 24;
  s_night_end = s_prefs.getUChar(kPrefsNightEndKey, kDefaultNightEnd) % 24;
  s_night_level = s_prefs.getUChar(kPrefsNightLevelKey, kDefaultNightLevel);
  if (s_night_level < 5 || s_night_level > 100) s_night_level = kDefaultNightLevel;
  s_alert_on = s_prefs.getBool(kPrefsAlertKey, true);
  s_alert_km = s_prefs.getUChar(kPrefsAlertKmKey, kDefaultAlertKm);
  if (s_alert_km < 1 || s_alert_km > 20) s_alert_km = kDefaultAlertKm;
  s_holidays = s_prefs.getBool(kPrefsHolidaysKey, true);
  s_auto_page_sec = s_prefs.getUChar(kPrefsAutoPageKey, 0);
  if (s_prefs.isKey(kPrefsAgendaKey)) {
    s_prefs.getString(kPrefsAgendaKey, s_agenda_url, sizeof(s_agenda_url));
  }
  const uint8_t near = s_prefs.getUChar(kPrefsNearKey, kDefaultNearIndex);
  s_near_index = near < kNearestRadiusCount ? near : kDefaultNearIndex;
  s_prefs.end();
}

void rangeNext() {
  s_range_index = static_cast<uint8_t>((s_range_index + 1) % kRangePresetCount);
  saveRangeIndex();
}

const RangePreset& rangeCurrent() { return kRangePresets[s_range_index]; }

uint8_t rangeIndex() { return s_range_index; }

float fetchRadiusKm() {
  const float outer_km = rangeCurrent().outer_km;
  const float screen_r_px =
      static_cast<float>(kCenterX - kBeyondRingScreenMarginPx);
  return outer_km * (screen_r_px / static_cast<float>(kGridOuterRadius));
}

float adsbRadiusKm() {
  const size_t next = s_range_index + 1 < kRangePresetCount ? s_range_index + 1 : s_range_index;
  const float screen_r_px = static_cast<float>(kCenterX - kBeyondRingScreenMarginPx);
  return kRangePresets[next].outer_km * (screen_r_px / static_cast<float>(kGridOuterRadius));
}

bool useMiles() { return s_use_miles; }

bool showRunways() { return s_show_runways; }

bool swapColors() { return s_swap_colors; }

bool showSweep() { return s_show_sweep; }

uint8_t language() { return s_language; }

void saveLanguageFromPortal(const char* value) {
  const int v = value != nullptr ? atoi(value) : 0;
  s_language = static_cast<uint8_t>(v >= 0 && v <= 3 ? v : 0);
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putUChar(kPrefsLangKey, s_language);
    s_prefs.end();
  }
  Serial.printf("Language: %u\n", s_language);
}

void saveSweepFromPortal(const char* checkbox_value) {
  s_show_sweep = portalCheckboxChecked(checkbox_value);
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putBool(kPrefsSweepKey, s_show_sweep);
    s_prefs.end();
  }
  Serial.printf("Radar sweep: %s\n", s_show_sweep ? "on" : "off");
}

bool nightDimEnabled() { return s_night_on; }

uint8_t nightStartHour() { return s_night_start; }

uint8_t nightEndHour() { return s_night_end; }

uint8_t nightLevelPercent() { return s_night_level; }

bool nightDimActive(int hour) {
  if (!s_night_on || s_night_start == s_night_end) {
    return false;
  }
  if (s_night_start < s_night_end) {
    return hour >= s_night_start && hour < s_night_end;
  }
  return hour >= s_night_start || hour < s_night_end;  // wraps past midnight
}

void saveNightFromPortal(const char* checkbox_value, const char* start, const char* end,
                         const char* level) {
  s_night_on = portalCheckboxChecked(checkbox_value);
  s_night_start = parseRange(start, 0, 23, s_night_start);
  s_night_end = parseRange(end, 0, 23, s_night_end);
  s_night_level = parseRange(level, 5, 100, s_night_level);
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putBool(kPrefsNightKey, s_night_on);
    s_prefs.putUChar(kPrefsNightStartKey, s_night_start);
    s_prefs.putUChar(kPrefsNightEndKey, s_night_end);
    s_prefs.putUChar(kPrefsNightLevelKey, s_night_level);
    s_prefs.end();
  }
  Serial.printf("Night clock: %s, %02u:00-%02u:00, %u%%\n", s_night_on ? "on" : "off",
                s_night_start, s_night_end, s_night_level);
}

bool alertEnabled() { return s_alert_on; }

float alertKm() { return s_alert_km; }

bool showHolidays() { return s_holidays; }

void saveAlertFromPortal(const char* checkbox_value, const char* km) {
  s_alert_on = portalCheckboxChecked(checkbox_value);
  s_alert_km = parseRange(km, 1, 20, s_alert_km);
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putBool(kPrefsAlertKey, s_alert_on);
    s_prefs.putUChar(kPrefsAlertKmKey, s_alert_km);
    s_prefs.end();
  }
  Serial.printf("Overhead alert: %s, %u km\n", s_alert_on ? "on" : "off", s_alert_km);
}

void saveHolidaysFromPortal(const char* checkbox_value) {
  s_holidays = portalCheckboxChecked(checkbox_value);
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putBool(kPrefsHolidaysKey, s_holidays);
    s_prefs.end();
  }
}

uint16_t autoPageSec() { return s_auto_page_sec; }

void saveAutoPageFromPortal(const char* seconds) {
  // 0 = off, otherwise 5..255 s.
  uint8_t v = parseRange(seconds, 0, 255, static_cast<uint8_t>(s_auto_page_sec));
  if (v > 0 && v < 5) v = 5;
  s_auto_page_sec = v;
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putUChar(kPrefsAutoPageKey, v);
    s_prefs.end();
  }
  Serial.printf("Auto page: %u s\n", v);
}

const char* agendaUrl() { return s_agenda_url; }

void saveAgendaFromPortal(const char* url) {
  char clean[kAgendaUrlMax + 1] = "";
  if (url != nullptr) {
    // Trim spaces a phone keyboard may add around a pasted link.
    while (*url == ' ') ++url;
    snprintf(clean, sizeof(clean), "%s", url);
    for (size_t n = strlen(clean); n > 0 && clean[n - 1] == ' '; --n) clean[n - 1] = '\0';
  }
  if (clean[0] != '\0' && strncmp(clean, "https://", 8) != 0) {
    Serial.println("Agenda link ignored (must start with https://)");
    return;
  }
  snprintf(s_agenda_url, sizeof(s_agenda_url), "%s", clean);
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putString(kPrefsAgendaKey, s_agenda_url);
    s_prefs.end();
  }
  Serial.printf("Agenda link: %s\n", s_agenda_url[0] ? "set" : "none");
}

float nearestRadiusKm() { return kNearestRadiiKm[s_near_index]; }

void nearestRadiusNext() {
  s_near_index = static_cast<uint8_t>((s_near_index + 1) % kNearestRadiusCount);
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putUChar(kPrefsNearKey, s_near_index);
    s_prefs.end();
  }
}

void saveSwapColorsFromPortal(const char* checkbox_value) {
  s_swap_colors = portalCheckboxChecked(checkbox_value);
  saveSwapColors();
  Serial.printf("Swap red/blue: %s\n", s_swap_colors ? "on" : "off");
}

void saveMilesFromPortal(const char* checkbox_value) {
  s_use_miles = portalCheckboxChecked(checkbox_value);
  saveUseMiles();
  Serial.printf("Distance units: %s\n", s_use_miles ? "miles" : "km");
}

void saveRunwaysFromPortal(const char* checkbox_value) {
  s_show_runways = portalCheckboxChecked(checkbox_value);
  saveShowRunways();
  Serial.printf("Runway overlay: %s\n", s_show_runways ? "on" : "off");
}

void formatRing3Label(char* buf, size_t len, float ring3_km, bool use_miles) {
  if (use_miles) {
    const int mi = static_cast<int>(lroundf(ring3_km / kKmPerMile));
    snprintf(buf, len, "%dmi", mi);
  } else {
    const int km = static_cast<int>(lroundf(ring3_km));
    snprintf(buf, len, "%dkm", km);
  }
}

void formatCurrentRing3Label(char* buf, size_t len) {
  formatRing3Label(buf, len, rangeCurrent().ring3_km, s_use_miles);
}

void unitsReset() {
  s_use_miles = false;
  s_show_runways = true;
  s_agenda_url[0] = '\0';  // private link: goes with the Wi-Fi reset
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.remove(kPrefsMilesKey);
    s_prefs.remove(kPrefsRunwaysKey);
    s_prefs.remove(kPrefsAgendaKey);
    s_prefs.end();
  }
}

}  // namespace ui::radar
