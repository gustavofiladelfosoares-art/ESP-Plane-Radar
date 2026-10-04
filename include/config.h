#pragma once

#include <cstdint>

#include <driver/gpio.h>

namespace config {

// --- Wi-Fi portal ---
constexpr char kPortalApName[] = "PlaneRadar-Setup";
constexpr char kPortalIp[] = "192.168.4.1";
/** mDNS host (no ".local" suffix); browser: http://plane-radar.local */
constexpr char kPortalHostname[] = "plane-radar";
constexpr char kPortalHostUrl[] = "plane-radar.local";

/** Per-attempt STA connect wait (ms); retried kWifiConnectAttempts times. */
constexpr unsigned long kWifiConnectAttemptMs = 15000;
constexpr uint8_t kWifiConnectAttempts = 3;
constexpr unsigned long kWifiPortalTimeoutSec = 0;  // 0 = no timeout while configuring
/** With Wi-Fi already saved, the portal closes after this and the saved network is retried. */
constexpr unsigned long kWifiSavedRetryPortalSec = 180;
constexpr unsigned long kWifiConnectingFrameMs = 50;
/** Wait after disconnect before reconnecting (avoids portal on brief drops). */
constexpr unsigned long kWifiDownGraceMs = 4000;
/** Minimum interval between background reconnect tries. */
constexpr unsigned long kWifiReconnectIntervalMs = 15000;

// --- BOOT button (ESP32-C3 Super Mini, active LOW) ---
constexpr gpio_num_t kBootPin = GPIO_NUM_9;
constexpr unsigned long kBootResetHoldMs = 3000UL;
/** Ignore BOOT taps shorter than this (debounce). */
constexpr unsigned long kBootTapMinMs = 40UL;
/** Two taps closer than this (release to release) count as a double tap. */
constexpr unsigned long kBootDoubleTapWindowMs = 450UL;

// --- Display: GC9A01 1.28" round 240×240 (SPI) ---
constexpr gpio_num_t kDisplayPinRst = GPIO_NUM_0;
constexpr gpio_num_t kDisplayPinCs = GPIO_NUM_1;
// DC is GPIO10 in the original ESP32-Plane-Radar wiring. Some ready-made
// units wire it to GPIO2 instead — build env "supermini-dc2" for those.
#ifndef PR_DISPLAY_PIN_DC
#define PR_DISPLAY_PIN_DC 10
#endif
constexpr gpio_num_t kDisplayPinDc = static_cast<gpio_num_t>(PR_DISPLAY_PIN_DC);
constexpr gpio_num_t kDisplayPinMosi = GPIO_NUM_3;  // display SDA
constexpr gpio_num_t kDisplayPinSclk = GPIO_NUM_4;  // display SCL

constexpr int kDisplayWidth = 240;
constexpr int kDisplayHeight = 240;

constexpr uint32_t kDisplaySpiWriteHz = 80000000;  // 80 MHz halves the frame push time
// GC9A01 modules often need invert. rgb_order = true showed yellow as cyan on
// this unit, so keep the panel in its native BGR order (colors come out right
// on both direct draws and sprite pushes).
constexpr bool kDisplayInvert = true;
constexpr bool kDisplayRgbOrder = false;

// --- Radar center defaults (overridden via WiFi setup portal) ---
constexpr double kDefaultRadarLat = -19.919100;  // Belo Horizonte - MG (set yours in the portal)
constexpr double kDefaultRadarLon = -43.938600;

/** Poll adsb.fi (API public limit: 1 req/s). */
constexpr unsigned long kAdsbFetchIntervalMs = 3000;
/** Off the plane pages: small-area checks for the overhead alert. */
constexpr unsigned long kAdsbAlertFetchIntervalMs = 10000;
/** Google Agenda refresh, and how old it may be when its page opens. */
constexpr unsigned long kAgendaFetchIntervalMs = 10UL * 60UL * 1000UL;
constexpr unsigned long kAgendaPageRefreshMs = 2UL * 60UL * 1000UL;
/** Legacy scale unused — fetch uses radar::fetchRadiusKm() to screen edge. */
constexpr float kAdsbFetchRadiusScale = 1.0f;
/** false = hide aircraft with alt_baro "ground"; true = show them too. */
constexpr bool kAdsbShowGroundAircraft = false;

// --- Weather / air quality (Open-Meteo, no API key) ---
constexpr unsigned long kWeatherFetchIntervalMs = 10UL * 60UL * 1000UL;
constexpr unsigned long kWeatherRetryIntervalMs = 60UL * 1000UL;
constexpr unsigned long kAirFetchIntervalMs = 30UL * 60UL * 1000UL;

// --- UI colors (RGB565) — status screens ---
constexpr uint16_t kColorBlack = 0x0000;
constexpr uint16_t kColorYellow = 0xFFE0;
constexpr uint16_t kTextOnYellow = kColorBlack;
constexpr uint16_t kTextOnBlack = 0xFFFF;

}  // namespace config
