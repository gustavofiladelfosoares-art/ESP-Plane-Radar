#pragma once

#include "ui/model.h"

namespace services::clock {

/** Start SNTP once Wi-Fi is up (safe to call repeatedly). */
void begin(double lon);

/** Local offset from UTC in seconds (Open-Meteo reports it for the location). */
void setUtcOffset(long seconds);

/** Local wall-clock time; valid == false until SNTP has synced. */
void now(ui::TimeModel* out);

}  // namespace services::clock
