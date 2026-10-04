#include "services/agenda.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstdio>
#include <cstring>

#include "services/http_util.h"
#include "services/shared.h"
#include "ui/radar_range.h"

namespace services::agenda {

namespace {

ui::AgendaModel s_agenda;

/** Copy UTF-8 text, never cutting a multi-byte character in half. */
void copyText(char* out, size_t len, const char* in) {
  snprintf(out, len, "%s", in != nullptr ? in : "");
  size_t n = strlen(out);
  if (in != nullptr && n < strlen(in)) {
    while (n > 0 && (static_cast<uint8_t>(out[n]) & 0xC0) == 0x80) --n;  // continuation byte
    out[n] = '\0';
  }
}

}  // namespace

bool fetch(const char* url) {
  if (url == nullptr || url[0] == '\0') {
    SharedLock lock;
    s_agenda = ui::AgendaModel{};
    return false;
  }
  JsonDocument doc;
  if (!http::getJson(url, "agenda", doc, nullptr, 15000)) {
    return false;
  }
  ui::AgendaModel next;
  next.configured = true;
  next.valid = true;
  // {"events":[{"time":"09:30","title":"…"}], "tasks":[{"title":"…","done":false}]}
  constexpr int kMax = sizeof(next.items) / sizeof(next.items[0]);
  for (JsonObject e : doc["events"].as<JsonArray>()) {
    if (next.count >= kMax) break;
    ui::AgendaItem& it = next.items[next.count++];
    copyText(it.time, sizeof(it.time), e["time"] | "");
    copyText(it.title, sizeof(it.title), e["title"] | "");
  }
  for (JsonObject t : doc["tasks"].as<JsonArray>()) {
    if (next.count >= kMax) break;
    ui::AgendaItem& it = next.items[next.count++];
    it.task = true;
    it.done = t["done"] | false;
    copyText(it.title, sizeof(it.title), t["title"] | "");
  }
  {
    SharedLock lock;
    s_agenda = next;
  }
  Serial.printf("agenda: %d items\n", next.count);
  return true;
}

void snapshot(ui::AgendaModel* out) {
  SharedLock lock;
  *out = s_agenda;
  out->configured = ui::radar::agendaUrl()[0] != '\0';
}

}  // namespace services::agenda
