#pragma once

#include "ui/model.h"

namespace services::weather {

/** Current conditions + today's and the next 5 days' forecast from Open-Meteo (updates sun + UTC offset too). */
bool fetchForecast(double lat, double lon);

/** US AQI and UV index from the Open-Meteo air-quality API. */
bool fetchAir(double lat, double lon);

/** Copy the latest values (thread-safe). */
void snapshot(ui::WeatherModel* weather, ui::AirModel* air, ui::SunModel* sun);

/** Copy of the next-five-days forecast (fetched together with the weather). */
void forecastSnapshot(ui::ForecastModel* out, ui::HourlyModel* hourly);

}  // namespace services::weather
