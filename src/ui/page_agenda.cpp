// Agenda page: today's Google Calendar events and Google Tasks to-dos,
// fetched through the user's own Apps Script link (tools/google-agenda).

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "ui/draw_util.h"
#include "ui/i18n.h"
#include "ui/pages.h"

namespace ui {

namespace {

using draw::kCx;
using fonts::Id;
using i18n::S;

const Rgb kTop{18, 18, 44};
const Rgb kBottom{6, 6, 18};
const Rgb kAccent{150, 130, 255};
const Rgb kWhite{238, 240, 252};
const Rgb kMuted{122, 126, 160};
const Rgb kTask{110, 210, 160};

constexpr int kRowsPerPage = 5;
constexpr int kRowH = 24;
constexpr int kListY = 86;
constexpr uint32_t kPageMs = 6000;

Rgb bgAt(int y) { return lerp(kTop, kBottom, y / 240.0f); }

float appear(uint32_t t, uint32_t start, uint32_t dur = 420) {
  return draw::easeOutCubic(draw::progress(t, start, dur));
}

/** Half-width of the round glass at row y. */
int halfWidth(int y) {
  const float dy = y - 120.0f;
  return static_cast<int>(sqrtf(120.0f * 120.0f - dy * dy));
}

int minutesOf(const char* hhmm) {
  if (hhmm[0] == '\0') return -1;
  return atoi(hhmm) * 60 + atoi(hhmm + 3);
}

void header(lgfx::LovyanGFX& g, const Model& m, uint32_t t) {
  const float p = appear(t, 0);
  draw::text(g, Id::S14, i18n::tr(S::AgendaLabel), kCx, 30, mix(bgAt(30), kAccent, p));
  if (m.time.valid) {
    char date[32];
    i18n::formatDate(date, sizeof(date), m.time.wday, m.time.day, m.time.month);
    const float pd = appear(t, 100);
    const int y = 53 + static_cast<int>((1.0f - pd) * 6.0f);
    draw::text(g, Id::S22, date, kCx, y, mix(bgAt(y), kWhite, pd));
  }
  g.drawFastHLine(kCx - 70, 70, 140, mix(bgAt(70), kAccent, 0.35f * p));
}

/** Centered message with a small calendar glyph above (empty / setup states). */
void message(lgfx::LovyanGFX& g, uint32_t t, const char* big, const char* small,
             const char* small2 = nullptr) {
  const float p = appear(t, 200);
  const int y = 112;
  const Rgb bg = bgAt(y);
  // Little calendar sheet that bobs gently.
  const int by = y + static_cast<int>(sinf(t * 0.003f) * 2.0f);
  g.fillRoundRect(kCx - 15, by - 14, 30, 28, 5, mix(bg, Rgb{40, 40, 80}, p));
  g.fillRoundRect(kCx - 15, by - 14, 30, 9, 4, mix(bg, kAccent, p));
  g.fillRect(kCx - 15, by - 9, 30, 4, mix(bg, kAccent, p));
  g.fillSmoothCircle(kCx - 7, by - 16, 2, mix(bg, kWhite, p));
  g.fillSmoothCircle(kCx + 7, by - 16, 2, mix(bg, kWhite, p));
  for (int i = 0; i < 3; ++i) {
    g.drawFastHLine(kCx - 9, by + 1 + i * 4, 18, mix(bg, kMuted, p));
  }
  draw::text(g, Id::S17, big, kCx, 152, mix(bgAt(152), kWhite, p));
  if (small != nullptr) {
    draw::text(g, Id::S14, small, kCx, 175, mix(bgAt(175), kMuted, p));
  }
  if (small2 != nullptr) {
    draw::text(g, Id::S14, small2, kCx, 194, mix(bgAt(194), kAccent, p));
  }
}

void row(lgfx::LovyanGFX& g, const AgendaItem& it, int y, float p, bool past, bool next,
         uint32_t t) {
  if (p <= 0.0f) return;
  const Rgb bg = bgAt(y);
  const int x0 = kCx - 86;  // one column for every row (fits the lowest one)
  const int x_right = kCx + halfWidth(y) - 16;
  const int slide = static_cast<int>((1.0f - p) * -16.0f);
  const float fade = past || it.done ? 0.5f : 1.0f;
  char buf[48];
  int tx = 0;
  if (it.task) {
    // Round checkbox; ticked for done tasks.
    const int cx = x0 + 7 + slide;
    g.drawCircle(cx, y, 6, mix(bg, kTask, p * fade));
    if (it.done) {
      g.fillSmoothCircle(cx, y, 6, mix(bg, kTask, p * 0.6f));
      draw::thickLine(g, cx - 3, y, cx - 1, y + 3, 1.2f, 1.2f, mix(bg, Rgb{10, 30, 20}, p));
      draw::thickLine(g, cx - 1, y + 3, cx + 4, y - 3, 1.2f, 1.2f, mix(bg, Rgb{10, 30, 20}, p));
    }
    tx = x0 + 20 + slide;
  } else {
    const char* when = it.time[0] ? it.time : "•";
    Rgb c = next ? Rgb{255, 214, 120} : kAccent;
    if (next) {
      // Pulsing marker on the next event of the day.
      const float pulse = 0.5f + 0.5f * sinf(t * 0.005f);
      g.fillSmoothCircle(x0 - 8 + slide, y, 2 + static_cast<int>(pulse), mix(bg, c, p));
    }
    draw::text(g, Id::S14, when, x0 + slide, y, mix(bg, c, p * fade), textdatum_t::middle_left);
    tx = x0 + (it.time[0] ? 46 : 14) + slide;
  }
  draw::fitText(g, Id::S14, it.title, x_right - tx, buf, sizeof(buf));
  const uint16_t tc = mix(bg, it.task ? Rgb{214, 240, 226} : kWhite, p * fade);
  draw::text(g, Id::S14, buf, tx, y, tc, textdatum_t::middle_left);
  if (it.done) {
    g.drawFastHLine(tx, y + 1, draw::textWidth(g, Id::S14, buf), tc);
  }
}

}  // namespace

uint32_t drawAgendaPage(lgfx::LovyanGFX& g, const Model& m, uint32_t t) {
  draw::verticalGradient(g, kTop, kBottom);
  header(g, m, t);
  const AgendaModel& a = m.agenda;

  if (!a.configured) {
    message(g, t, "Google Agenda", i18n::tr(S::AgendaSetup), "plane-radar.local");
  } else if (!a.valid) {
    message(g, t, i18n::tr(S::LoadingAgenda), m.wifi_ok ? nullptr : i18n::tr(S::NoWifi));
  } else if (a.count == 0) {
    message(g, t, i18n::tr(S::FreeDay), i18n::tr(S::NothingToday));
  } else {
    // Which event comes next (first not yet started).
    const int now = m.time.valid ? m.time.hour * 60 + m.time.minute : -1;
    int next = -1;
    for (int i = 0; i < a.count && next < 0; ++i) {
      if (!a.items[i].task && minutesOf(a.items[i].time) >= now && now >= 0) next = i;
    }
    // More than five items: show them five at a time, flipping every 6 s.
    const int pages = (a.count + kRowsPerPage - 1) / kRowsPerPage;
    const int page = pages > 1 ? static_cast<int>((t / kPageMs) % pages) : 0;
    const uint32_t tp = pages > 1 ? t % kPageMs : t;
    const float out = pages > 1 ? 1.0f - draw::clamp01((tp - (kPageMs - 300.0f)) / 300.0f) : 1.0f;
    for (int r = 0; r < kRowsPerPage; ++r) {
      const int i = page * kRowsPerPage + r;
      if (i >= a.count) break;
      const AgendaItem& it = a.items[i];
      const int mins = minutesOf(it.time);
      const bool past = !it.task && mins >= 0 && now >= 0 && mins < now && i != next;
      const float p = appear(tp, 180 + r * 70) * out;
      row(g, it, kListY + r * kRowH, p, past, i == next, t);
    }
    if (pages > 1) {
      char buf[16];
      snprintf(buf, sizeof(buf), i18n::tr(S::PageFmt), page + 1, pages);
      draw::text(g, Id::S14, buf, kCx, 207, mix(bgAt(207), kMuted, 0.9f));
    }
  }

  draw::pageDots(g, static_cast<int>(Page::Agenda), kPageCount);
  return 40;
}

}  // namespace ui
