#include "services/adsb_client.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <ArduinoJson.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "services/shared.h"

namespace services::adsb {

namespace {

constexpr char kApiBase[] = "https://opendata.adsb.fi/api/v3/lat/";
constexpr float kKmPerNm = 1.852f;
constexpr int kConnectTimeoutMs = 5000;  // TLS handshake needs room
constexpr int kConnectAttempts = 1;  // a stalled TLS connect blocks the UI; retry next poll instead
constexpr unsigned long kRequestTimeoutMs = 6000;

// s_aircraft is what the UI sees; s_parse is filled by the network task and
// copied over under the shared lock once a fetch completes.
Aircraft s_aircraft[kMaxAircraft];
size_t s_aircraft_count = 0;
Aircraft s_parse[kMaxAircraft];
PollFn s_poll_fn = nullptr;

void pollNetwork() {
  if (s_poll_fn != nullptr) {
    s_poll_fn();
  }
}

float kmToNauticalMiles(float km) { return km / kKmPerNm; }

bool readJsonFloat(const JsonObject& obj, const char* key, float* out) {
  if (obj[key].is<float>() || obj[key].is<double>() || obj[key].is<int>()) {
    *out = obj[key].as<float>();
    return true;
  }
  return false;
}

float pickNoseHeading(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "true_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "mag_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "track", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "dir", &v)) {
    return v;
  }
  return 0.0f;
}

float pickTrackHeading(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "track", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "true_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "mag_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "dir", &v)) {
    return v;
  }
  return 0.0f;
}

float pickGroundSpeed(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "gs", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "tas", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "ias", &v)) {
    return v;
  }
  return 0.0f;
}

bool isOnGround(const JsonObject& plane) {
  if (!plane["alt_baro"].is<const char*>()) {
    return false;
  }
  return strcmp(plane["alt_baro"].as<const char*>(), "ground") == 0;
}

void copyJsonStringTrimmed(const JsonObject& obj, const char* key, char* out,
                           size_t out_len) {
  out[0] = '\0';
  if (out_len == 0 || !obj[key].is<const char*>()) {
    return;
  }
  const char* s = obj[key].as<const char*>();
  size_t n = strnlen(s, out_len - 1);
  while (n > 0 && s[n - 1] == ' ') {
    --n;
  }
  memcpy(out, s, n);
  out[n] = '\0';
}

void formatAltitudeTag(const JsonObject& plane, char* out, size_t out_len) {
  out[0] = '\0';
  if (out_len == 0) {
    return;
  }

  if (plane["alt_baro"].is<const char*>()) {
    const char* s = plane["alt_baro"].as<const char*>();
    if (strcmp(s, "ground") == 0) {
      strncpy(out, "GND", out_len - 1);
      out[out_len - 1] = '\0';
      return;
    }
  }

  float alt = 0.0f;
  if (readJsonFloat(plane, "alt_baro", &alt) ||
      readJsonFloat(plane, "alt_geom", &alt)) {
    snprintf(out, out_len, "%d ft", static_cast<int>(lroundf(alt)));
  }
}

bool httpGetJson(const String& url, const char* tag, JsonDocument& doc,
                 const JsonDocument& filter) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.printf("%s: http.begin failed\n", tag);
    return false;
  }

  http.useHTTP10(true);
  http.setTimeout(kRequestTimeoutMs);
  http.setConnectTimeout(kConnectTimeoutMs);

  int code = 0;
  for (int attempt = 0; attempt < kConnectAttempts; ++attempt) {
    pollNetwork();
    code = http.GET();
    if (code > 0) {
      break;
    }
  }
  if (code != HTTP_CODE_OK) {
    Serial.printf("%s: HTTP %d\n", tag, code);
    http.end();
    return false;
  }

  const DeserializationError err = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("%s: JSON parse error: %s\n", tag, err.c_str());
    return false;
  }
  return true;
}

void fillTagFields(Aircraft* ac, const JsonObject& plane) {
  copyJsonStringTrimmed(plane, "flight", ac->callsign, sizeof(ac->callsign));
  if (ac->callsign[0] == '\0') {
    copyJsonStringTrimmed(plane, "hex", ac->callsign, sizeof(ac->callsign));
  }

  copyJsonStringTrimmed(plane, "t", ac->type, sizeof(ac->type));
  copyJsonStringTrimmed(plane, "desc", ac->desc, sizeof(ac->desc));
  copyJsonStringTrimmed(plane, "r", ac->reg, sizeof(ac->reg));
  formatAltitudeTag(plane, ac->alt, sizeof(ac->alt));

  ac->on_ground = isOnGround(plane);
  float alt = 0.0f;
  ac->has_alt = !ac->on_ground && (readJsonFloat(plane, "alt_baro", &alt) ||
                                   readJsonFloat(plane, "alt_geom", &alt));
  ac->alt_ft = ac->has_alt ? static_cast<int32_t>(lroundf(alt)) : 0;

  ac->squawk = 0;
  ac->flags = 0;
  if (plane["squawk"].is<const char*>()) {
    ac->squawk = static_cast<uint16_t>(atoi(plane["squawk"].as<const char*>()));
  }
  const char* emergency = plane["emergency"].is<const char*>() ? plane["emergency"].as<const char*>() : "";
  if (ac->squawk == 7500 || ac->squawk == 7600 || ac->squawk == 7700 ||
      (emergency[0] != '\0' && strcmp(emergency, "none") != 0)) {
    ac->flags |= kFlagEmergency;
  }
  if ((plane["dbFlags"] | 0) & 1) {
    ac->flags |= kFlagMilitary;
  }
}

}  // namespace

void setPollFn(PollFn fn) { s_poll_fn = fn; }

size_t aircraftCount() { return s_aircraft_count; }

const Aircraft* aircraftList() { return s_aircraft; }



bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km) {
  const float dist_nm = kmToNauticalMiles(fetch_radius_km);

  String url = kApiBase;
  url += String(center_lat, 6);
  url += "/lon/";
  url += String(center_lon, 6);
  url += "/dist/";
  url += String(dist_nm, 1);

  // Keep only the fields we render; the rest never reaches RAM.
  JsonDocument filter;
  JsonObject f = filter["ac"].add<JsonObject>();
  for (const char* key :
       {"lat", "lon", "true_heading", "mag_heading", "track", "dir", "gs",
        "tas", "ias", "alt_baro", "alt_geom", "flight", "hex", "t",
        "category", "desc", "r", "squawk", "emergency", "dbFlags"}) {
    f[key] = true;
  }

  JsonDocument doc;
  if (!httpGetJson(url, "adsb", doc, filter)) {
    return false;
  }

  JsonArray ac = doc["ac"].as<JsonArray>();
  if (ac.isNull()) {
    SharedLock lock;
    s_aircraft_count = 0;
    return true;
  }

  // Keep the closest kMaxAircraft when the area is busy (distance² in
  // degrees, longitude scaled by cos(latitude) — only used for ranking).
  const float cos_lat = cosf(static_cast<float>(center_lat) * 0.01745329f);
  auto dist2 = [&](float lat, float lon) {
    const float dy = lat - static_cast<float>(center_lat);
    const float dx = (lon - static_cast<float>(center_lon)) * cos_lat;
    return dx * dx + dy * dy;
  };
  size_t n = 0;
  for (JsonObject plane : ac) {
    if (!plane["lat"].is<float>() || !plane["lon"].is<float>()) {
      continue;
    }
    if (isOnGround(plane) && !config::kAdsbShowGroundAircraft) {
      continue;
    }
    const float lat = plane["lat"].as<float>();
    const float lon = plane["lon"].as<float>();
    size_t slot = n;
    if (n >= kMaxAircraft) {
      size_t far = 0;
      for (size_t k = 1; k < n; ++k) {
        if (dist2(s_parse[k].lat, s_parse[k].lon) > dist2(s_parse[far].lat, s_parse[far].lon)) far = k;
      }
      if (dist2(lat, lon) >= dist2(s_parse[far].lat, s_parse[far].lon)) {
        continue;
      }
      slot = far;
    }

    s_parse[slot].lat = lat;
    s_parse[slot].lon = lon;
    s_parse[slot].nose_deg = pickNoseHeading(plane);
    s_parse[slot].track_deg = pickTrackHeading(plane);
    s_parse[slot].gs_knots = pickGroundSpeed(plane);
    fillTagFields(&s_parse[slot], plane);
    if (slot == n) ++n;
  }

  {
    SharedLock lock;
    memcpy(s_aircraft, s_parse, n * sizeof(Aircraft));
    s_aircraft_count = n;
  }
  Serial.printf("adsb: %u aircraft\n", static_cast<unsigned>(n));
  return true;
}

}  // namespace services::adsb
