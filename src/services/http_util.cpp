#include "services/http_util.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <cstdlib>

namespace services::http {

namespace {

constexpr char kUserAgent[] = "PlaneRadar-ESP32/1.0 (hobby radar display)";

bool open(WiFiClientSecure& client, HTTPClient& http, const char* url, const char* tag,
          uint32_t timeout_ms) {
  client.setInsecure();
  if (!http.begin(client, url)) {
    Serial.printf("%s: http.begin failed\n", tag);
    return false;
  }
  http.useHTTP10(true);  // no chunked encoding: the stream is the raw body
  http.setUserAgent(kUserAgent);
  http.setTimeout(timeout_ms);
  http.setConnectTimeout(5000);
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("%s: HTTP %d\n", tag, code);
    return false;
  }
  return true;
}

}  // namespace

bool getJson(const char* url, const char* tag, JsonDocument& doc, const JsonDocument* filter,
             uint32_t timeout_ms) {
  WiFiClientSecure client;
  HTTPClient http;
  if (!open(client, http, url, tag, timeout_ms)) {
    http.end();
    return false;
  }
  const DeserializationError err =
      filter != nullptr
          ? deserializeJson(doc, http.getStream(), DeserializationOption::Filter(*filter))
          : deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    Serial.printf("%s: JSON error %s\n", tag, err.c_str());
    return false;
  }
  return true;
}

bool getBytes(const char* url, const char* tag, uint8_t** out, size_t* out_len, size_t max_len,
              uint32_t timeout_ms) {
  *out = nullptr;
  *out_len = 0;
  WiFiClientSecure client;
  HTTPClient http;
  if (!open(client, http, url, tag, timeout_ms)) {
    http.end();
    return false;
  }
  const int declared = http.getSize();
  if (declared > static_cast<int>(max_len)) {
    Serial.printf("%s: body too large (%d)\n", tag, declared);
    http.end();
    return false;
  }
  const size_t cap = declared > 0 ? static_cast<size_t>(declared) : max_len;
  auto* buf = static_cast<uint8_t*>(malloc(cap));
  if (buf == nullptr) {
    Serial.printf("%s: no memory for %u bytes\n", tag, static_cast<unsigned>(cap));
    http.end();
    return false;
  }
  WiFiClient* stream = http.getStreamPtr();
  size_t n = 0;
  uint32_t last = millis();
  while (n < cap && millis() - last < timeout_ms) {
    const int avail = stream->available();
    if (avail > 0) {
      n += stream->readBytes(buf + n, std::min(cap - n, static_cast<size_t>(avail)));
      last = millis();
    } else if (!stream->connected()) {
      break;
    } else {
      delay(2);
    }
  }
  http.end();
  if (n == 0 || (declared > 0 && n != cap)) {
    Serial.printf("%s: short body (%u)\n", tag, static_cast<unsigned>(n));
    free(buf);
    return false;
  }
  *out = buf;
  *out_len = n;
  return true;
}

}  // namespace services::http
