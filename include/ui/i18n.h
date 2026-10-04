#pragma once

#include <cstddef>
#include <cstdint>

namespace ui::i18n {

/** Screen language (chosen in the setup portal). */
enum class Lang : uint8_t { PT, EN, ES, ZH, Count };

constexpr int kLangCount = static_cast<int>(Lang::Count);

/** Active language; the firmware sets it from the saved setting every frame. */
extern Lang g_lang;

/** Every translatable text on the device screens. */
enum class S : uint16_t {
  // weather conditions
  Sunny, ClearNight, FewClouds, PartlyCloudy, Cloudy, Fog, Drizzle, FreezingDrizzle,
  LightRain, Rain, HeavyRain, FreezingRain, Snow, Showers, HeavyShowers, Storm,
  StormHail, Weather,
  // weather page
  FeelsFmt, LoadingWeather, NoWifi,
  // clock page
  Syncing,
  // nearest page
  Nearest, Aircraft, OnGround, RadiusFmt, RadiusToastFmt, NoAircraft, WithinFmt,
  TwoTapsHint, TryingToConnect,
  // air / sun page
  AqiGood, AqiModerate, AqiSensitive, AqiUnhealthy, AqiVeryUnhealthy, AqiHazardous,
  UvLow, UvModerate, UvHigh, UvVeryHigh, UvExtreme, AirCaption, UntilSunset,
  UntilSunrise, SunWaiting, LoadingAir,
  // status screens
  WifiSetup, JoinNetwork, OpenBrowser, OrIp, ConnectingTo, NotConnected,
  CheckPassword, AndSignal, HoldBoot, ToSetupAgain, WifiCleared, Restarting,
  // alerts and special aircraft
  Overhead, Emergency, RadioFailure, Hijack, Military,
  // calendar
  Today, HolidayNewYear, HolidayCarnival, HolidayGoodFriday, HolidayTiradentes,
  HolidayLabour, HolidayCorpusChristi, HolidayIndependence, HolidayAparecida,
  HolidayAllSouls, HolidayRepublic, HolidayBlackAwareness, HolidayChristmas,
  // agenda
  NextDays, NextHours, SkyToday, AircraftSeen, Highest, Fastest, Closest, MostSeen,
  FlightsFmt, NoSkyYet, AgendaLabel, FreeDay, NothingToday, LoadingAgenda, AgendaSetup, AllDay, PageFmt,
  Count
};

const char* tr(S s);

/** Short weekday (0 = Sunday) and month (1..12) names. */
const char* weekday(int wday);
const char* month(int month);

/** Weekday initial for the calendar header (0 = Sunday). */
const char* weekdayInitial(int wday);

/** "OUTUBRO 2026" / "OCTOBER 2026" / "2026年10月". */
void formatMonthYear(char* out, size_t len, int month, int year);

/** "TER, 29 SET" / "TUE, 29 SEP" / "9月29日 周二". */
void formatDate(char* out, size_t len, int wday, int day, int month);

/** 8-point compass label for a bearing in degrees (0 = north). */
const char* compass8(float deg);

/** "4,0 km a NE" / "4.0 km NE" / "4.0 公里 东北"-style distance + direction. */
void formatDistanceDir(char* out, size_t len, float km, const char* dir);

/** Decimal and thousands separators for the active language. */
char decimalSep();
char thousandsSep();

/** Radar cardinal letters: north, south, east, west. */
const char* cardinal(int idx);

}  // namespace ui::i18n
