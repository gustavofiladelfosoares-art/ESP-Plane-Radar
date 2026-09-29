#pragma once

#include <cstddef>
#include <cstdint>

#include <ArduinoJson.h>

namespace services::http {

/** HTTPS GET + streamed JSON parse (optionally through a filter). */
bool getJson(const char* url, const char* tag, JsonDocument& doc,
             const JsonDocument* filter = nullptr, uint32_t timeout_ms = 8000);

/**
 * HTTPS GET into a malloc'd buffer (caller frees *out). Fails if the body is
 * larger than max_len.
 */
bool getBytes(const char* url, const char* tag, uint8_t** out, size_t* out_len,
              size_t max_len, uint32_t timeout_ms = 8000);

}  // namespace services::http
