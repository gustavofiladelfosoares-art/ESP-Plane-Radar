#include "services/clock.h"

#include <Arduino.h>
#include <sys/time.h>

#include <cmath>
#include <ctime>

namespace services::clock {

namespace {

bool s_started = false;
volatile long s_offset_s = 0;
bool s_offset_from_api = false;

// Anything before 2024 means SNTP hasn't set the clock yet.
constexpr time_t kSyncedAfter = 1704067200;

}  // namespace

void begin(double lon) {
  if (s_started) {
    return;
  }
  if (!s_offset_from_api) {
    // Rough guess from longitude until the weather API reports the real zone.
    s_offset_s = static_cast<long>(lround(lon / 15.0)) * 3600L;
  }
  configTime(0, 0, "pool.ntp.org", "time.google.com", "a.st1.ntp.br");
  s_started = true;
}

void setUtcOffset(long seconds) {
  s_offset_s = seconds;
  s_offset_from_api = true;
}

void now(ui::TimeModel* out) {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec < kSyncedAfter) {
    out->valid = false;
    return;
  }
  const time_t local = tv.tv_sec + s_offset_s;
  struct tm tm;
  gmtime_r(&local, &tm);
  out->valid = true;
  out->year = tm.tm_year + 1900;
  out->month = tm.tm_mon + 1;
  out->day = tm.tm_mday;
  out->wday = tm.tm_wday;
  out->hour = tm.tm_hour;
  out->minute = tm.tm_min;
  out->second = tm.tm_sec;
  out->millis = static_cast<int>(tv.tv_usec / 1000);
}

}  // namespace services::clock
