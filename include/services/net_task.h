#pragma once

#include "ui/pages.h"

namespace services::net {

/** Start the background task that does all HTTP work (ADS-B, weather, maps…). */
void start();

/** Tell the task which page is showing (aircraft are only polled when needed). */
void setActivePage(ui::Page page);

/** Force a weather + air refresh on the next pass (e.g. after settings change). */
void refreshWeather();

/** Fetch aircraft again right away (e.g. the search radius just changed). */
void refreshAircraft();

/** Fetch the Google Agenda again right away. */
void refreshAgenda();

}  // namespace services::net
