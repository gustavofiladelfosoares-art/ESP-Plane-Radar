// Calendar page: the month at a glance with today highlighted, Brazilian
// national holidays and the moon phase. Double tap shows the next months.

#include <cmath>
#include <cstdio>

#include "ui/draw_util.h"
#include "ui/i18n.h"
#include "ui/pages.h"

namespace ui {

namespace {

using draw::kCx;
using fonts::Id;
using i18n::S;

const Rgb kTop{14, 22, 46};
const Rgb kBottom{4, 7, 18};
const Rgb kAccent{70, 160, 255};
const Rgb kWhite{236, 242, 252};
const Rgb kMuted{120, 134, 160};
const Rgb kSunday{255, 132, 132};
const Rgb kHoliday{255, 196, 90};

constexpr int kColW = 26;
constexpr int kRowH = 19;
constexpr int kGridY = 88;

Rgb bgAt(int y) { return lerp(kTop, kBottom, y / 240.0f); }

struct Holiday {
  int month;
  int day;
  S name;
};

/** Day of week, 0 = Sunday (Sakamoto). */
int dayOfWeek(int y, int m, int d) {
  static const int kT[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + kT[m - 1] + d) % 7;
}

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysInMonth(int y, int m) {
  static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return m == 2 && isLeap(y) ? 29 : kDays[m - 1];
}

/** Day number since 1970-01-01 (proleptic Gregorian). */
long dayNumber(int y, int m, int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const long yoe = y - era * 400;
  const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

void fromDayNumber(long z, int* y, int* m, int* d) {
  z += 719468;
  const long era = (z >= 0 ? z : z - 146096) / 146097;
  const long doe = z - era * 146097;
  const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const long mp = (5 * doy + 2) / 153;
  *d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  *m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  *y = static_cast<int>(yoe + era * 400 + (*m <= 2));
}

/** Brazilian national holidays (and Carnival / Corpus Christi) of a year. */
int holidays(int year, Holiday* out) {
  // Easter Sunday (anonymous Gregorian algorithm).
  const int a = year % 19;
  const int b = year / 100;
  const int c = year % 100;
  const int d = b / 4;
  const int e = b % 4;
  const int f = (b + 8) / 25;
  const int g = (b - f + 1) / 3;
  const int h = (19 * a + b - d - g + 15) % 30;
  const int i = c / 4;
  const int k = c % 4;
  const int l = (32 + 2 * e + 2 * i - h - k) % 7;
  const int mm = (a + 11 * h + 22 * l) / 451;
  const int em = (h + l - 7 * mm + 114) / 31;
  const int ed = (h + l - 7 * mm + 114) % 31 + 1;
  const long easter = dayNumber(year, em, ed);

  int n = 0;
  auto add = [&](int m, int dd, S s) { out[n++] = Holiday{m, dd, s}; };
  auto addRel = [&](long offset, S s) {
    int y = 0;
    int m = 0;
    int dd = 0;
    fromDayNumber(easter + offset, &y, &m, &dd);
    add(m, dd, s);
  };
  add(1, 1, S::HolidayNewYear);
  addRel(-48, S::HolidayCarnival);
  addRel(-47, S::HolidayCarnival);
  addRel(-2, S::HolidayGoodFriday);
  add(4, 21, S::HolidayTiradentes);
  add(5, 1, S::HolidayLabour);
  addRel(60, S::HolidayCorpusChristi);
  add(9, 7, S::HolidayIndependence);
  add(10, 12, S::HolidayAparecida);
  add(11, 2, S::HolidayAllSouls);
  add(11, 15, S::HolidayRepublic);
  add(11, 20, S::HolidayBlackAwareness);
  add(12, 25, S::HolidayChristmas);
  return n;
}

const Holiday* holidayOn(const Holiday* list, int n, int m, int d) {
  for (int i = 0; i < n; ++i) {
    if (list[i].month == m && list[i].day == d) return &list[i];
  }
  return nullptr;
}

float appear(uint32_t t, uint32_t start, uint32_t dur = 420) {
  return draw::easeOutCubic(draw::progress(t, start, dur));
}

/** "12 OUT" / "OCT 12" / "10月12日" for the holiday line. */
void shortDate(char* out, size_t len, int m, int d) {
  if (i18n::g_lang == i18n::Lang::ZH) {
    snprintf(out, len, "%s%d日", i18n::month(m), d);
  } else if (i18n::g_lang == i18n::Lang::EN) {
    snprintf(out, len, "%s %d", i18n::month(m), d);
  } else {
    snprintf(out, len, "%d %s", d, i18n::month(m));
  }
}

}  // namespace

uint32_t drawCalendarPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t) {
  draw::verticalGradient(g, kTop, kBottom);
  const TimeModel& tm = m.time;
  if (!tm.valid) {
    draw::text(g, Id::S17, i18n::tr(S::Syncing), kCx, 120, rgb(kMuted));
    draw::pageDots(g, static_cast<int>(Page::Calendar), kPageCount);
    return 200;
  }

  // Month shown: today's, or a later one after double taps.
  int year = tm.year;
  int month = tm.month + m.calendar_month_offset;
  while (month > 12) {
    month -= 12;
    ++year;
  }
  const bool this_month = m.calendar_month_offset == 0;
  // Grid animations restart when the month changes.
  const uint32_t tg = m.calendar_changed_ago_ms < t ? m.calendar_changed_ago_ms : t;
  const float slide = 1.0f - appear(tg, 0, 380);

  Holiday hol[16];
  const int hol_n = m.show_holidays ? holidays(year, hol) : 0;

  // Moon of the day (today, or the 1st of a later month) above the title.
  const float pm = appear(t, 0, 600);
  const int moon_day = this_month ? tm.day : 1;
  draw::moonPhaseIcon(g, kCx, 22, 8, draw::moonPhase(year, month, moon_day), m.lat < 0.0, bgAt(22), pm);

  char buf[48];
  i18n::formatMonthYear(buf, sizeof(buf), month, year);
  {
    const float p = appear(tg, 60);
    const int y = 46 + static_cast<int>((1.0f - p) * 6.0f);
    draw::text(g, Id::S17, buf, kCx + static_cast<int>(slide * 24.0f), y,
               mix(bgAt(y), this_month ? kWhite : Rgb{170, 200, 240}, p));
  }

  // Weekday initials.
  for (int c = 0; c < 7; ++c) {
    const float p = appear(t, 120 + c * 25);
    const int x = kCx + (c - 3) * kColW;
    draw::text(g, Id::S14, i18n::weekdayInitial(c), x, 68,
               mix(bgAt(68), c == 0 ? kSunday : kMuted, p * 0.9f));
  }
  g.drawFastHLine(kCx - 3 * kColW - 10, 78, 6 * kColW + 20, mix(bgAt(78), kAccent, 0.35f * pm));

  // Day grid, rippling in diagonally.
  const int first = dayOfWeek(year, month, 1);
  const int days = daysInMonth(year, month);
  const int shift = static_cast<int>(slide * 24.0f);
  for (int d = 1; d <= days; ++d) {
    const int cell = first + d - 1;
    const int col = cell % 7;
    const int row = cell / 7;
    const float p = appear(tg, 140 + (row + col) * 28, 360);
    if (p <= 0.0f) continue;
    const int x = kCx + (col - 3) * kColW + shift;
    const int y = kGridY + row * kRowH;
    const Rgb bg = bgAt(y);
    const bool today = this_month && d == tm.day;
    const Holiday* h = hol_n ? holidayOn(hol, hol_n, month, d) : nullptr;
    snprintf(buf, sizeof(buf), "%d", d);
    if (today) {
      // Breathing halo + filled disc.
      const float pulse = 0.5f + 0.5f * sinf(t * 0.004f);
      const int rr = 11 + static_cast<int>(pulse * 2.0f);
      g.drawCircle(x, y, rr + 1, mix(bg, kAccent, 0.35f * p * (1.0f - pulse * 0.5f)));
      g.fillSmoothCircle(x, y, static_cast<int>(10 * draw::easeOutBack(p)), mix(bg, kAccent, p));
      draw::text(g, Id::S14, buf, x, y + 1, mix(kAccent, Rgb{255, 255, 255}, p));
      continue;
    }
    Rgb c = col == 0 ? kSunday : kWhite;
    if (h != nullptr) c = kHoliday;
    const bool past = this_month && d < tm.day;
    const float fade = past ? 0.45f : 1.0f;
    draw::text(g, Id::S14, buf, x, y, mix(bg, c, p * fade));
    if (h != nullptr) {
      g.fillSmoothCircle(x, y + 9, 1, mix(bg, kHoliday, p * fade));
    }
  }

  // Bottom line: today's holiday, or the next one.
  const Holiday* show = nullptr;
  if (hol_n) {
    if (this_month) {
      for (int i = 0; i < hol_n && show == nullptr; ++i) {
        const Holiday& h = hol[i];
        if (dayNumber(year, h.month, h.day) >= dayNumber(year, month, tm.day)) show = &h;
      }
      if (show == nullptr) {  // past Christmas: next year's New Year
        hol[0] = Holiday{1, 1, S::HolidayNewYear};
        show = &hol[0];
      }
    } else {
      for (int i = 0; i < hol_n && show == nullptr; ++i) {
        if (hol[i].month == month) show = &hol[i];
      }
    }
  }
  if (show != nullptr) {
    const float p = appear(t, 700);
    const bool is_today = this_month && show->month == month && show->day == tm.day;
    char date[20];
    if (is_today) {
      snprintf(date, sizeof(date), "%s", i18n::tr(S::Today));
    } else {
      shortDate(date, sizeof(date), show->month, show->day);
    }
    char line[64];
    snprintf(line, sizeof(line), "%s • %s", date, i18n::tr(show->name));
    // Last grid row is at y 183 (six-week months); otherwise sit a bit higher.
    const int rows = (first + days + 6) / 7;
    const int base_y = rows > 5 ? 205 : 198;
    const float half = sqrtf(120.0f * 120.0f - (base_y - 120.0f) * (base_y - 120.0f));
    draw::fitText(g, Id::S14, line, static_cast<int>(2.0f * half) - 18, buf, sizeof(buf));
    const int y = base_y + static_cast<int>((1.0f - p) * 6.0f);
    const int tw = draw::textWidth(g, Id::S14, buf);
    g.fillSmoothCircle(kCx - tw / 2 - 8, y, 3, mix(bgAt(y), kHoliday, p));
    draw::text(g, Id::S14, buf, kCx + 4, y, mix(bgAt(y), Rgb{255, 214, 140}, p));
  }

  draw::pageDots(g, static_cast<int>(Page::Calendar), kPageCount);
  return 40;
}

}  // namespace ui
