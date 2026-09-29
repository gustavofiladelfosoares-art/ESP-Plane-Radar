// Screen wiring finder: tries common ESP32-C3 + GC9A01 pinouts one per boot,
// flashing red / green / blue and a big number. Watch the screen and note the
// number that appears; the serial log says which pins that number used.
//
//   pio run -e screentest -t upload

#include <Arduino.h>
#include <esp_attr.h>
#include <esp_system.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

namespace {

struct Pins {
  const char* name;
  int sclk;
  int mosi;
  int dc;
  int cs;
  int rst;
  int bl;  // -1 = none (backlight wired to power)
  uint32_t hz;
};

// No candidate touches GPIO9 (BOOT button) or 18/19 (USB).
constexpr Pins kCandidates[] = {
    {"ESP32-2424S012 (SCL6 SDA7 DC2 CS10 BL3)", 6, 7, 2, 10, -1, 3, 20000000},
    {"projeto original (SCL4 SDA3 DC10 CS1 RST0) 20MHz", 4, 3, 10, 1, 0, -1, 20000000},
    {"projeto original, fios lentos 8MHz", 4, 3, 10, 1, 0, -1, 8000000},
    {"original com SCL/SDA trocados (SCL3 SDA4)", 3, 4, 10, 1, 0, -1, 8000000},
    {"original com DC/CS trocados (DC1 CS10)", 4, 3, 1, 10, 0, -1, 8000000},
    {"XIAO ESP32C3 Round Display (SCL8 SDA10 DC5 CS3 BL21)", 8, 10, 5, 3, -1, 21, 20000000},
    {"alternativa (SCL6 SDA7 DC2 CS10 RST3)", 6, 7, 2, 10, 3, -1, 8000000},
    {"alternativa (SCL4 SDA6 DC2 CS7 RST10)", 4, 6, 2, 7, 10, -1, 8000000},
    {"alternativa (SCL2 SDA3 DC4 CS5 RST1)", 2, 3, 4, 5, 1, -1, 8000000},
};
constexpr int kCount = sizeof(kCandidates) / sizeof(kCandidates[0]);

RTC_NOINIT_ATTR uint32_t s_magic;
RTC_NOINIT_ATTR uint32_t s_index;

class Screen : public lgfx::LGFX_Device {
 public:
  explicit Screen(const Pins& p) {
    auto b = bus_.config();
    b.spi_host = SPI2_HOST;
    b.freq_write = p.hz;
    b.pin_sclk = p.sclk;
    b.pin_mosi = p.mosi;
    b.pin_miso = -1;
    b.pin_dc = p.dc;
    bus_.config(b);
    panel_.setBus(&bus_);
    auto c = panel_.config();
    c.pin_cs = p.cs;
    c.pin_rst = p.rst;
    c.invert = true;
    c.rgb_order = false;
    panel_.config(c);
    setPanel(&panel_);
  }

 private:
  lgfx::Bus_SPI bus_;
  lgfx::Panel_GC9A01 panel_;
};

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(600);
  if (s_magic != 0x5C12EE17 || s_index >= kCount) {
    s_magic = 0x5C12EE17;
    s_index = 0;
  }
  const int i = static_cast<int>(s_index);
  const Pins& p = kCandidates[i];
  Serial.printf("\n=== TESTE %d/%d: %s ===\n", i + 1, kCount, p.name);

  if (p.bl >= 0) {
    pinMode(p.bl, OUTPUT);
    digitalWrite(p.bl, HIGH);
  }
  Screen tft(p);
  tft.init();
  tft.setRotation(0);

  const uint32_t colors[] = {0xFF0000, 0x00FF00, 0x0000FF};
  for (int rep = 0; rep < 2; ++rep) {
    for (uint32_t c : colors) {
      tft.fillScreen(tft.color888(c >> 16, (c >> 8) & 0xFF, c & 0xFF));
      delay(350);
    }
  }
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setFont(&fonts::Font7);
  tft.setTextSize(3);
  tft.drawNumber(i + 1, 120, 120);
  delay(4000);

  s_index = (i + 1) % kCount;
  Serial.println("proximo teste...");
  delay(200);
  esp_restart();
}

void loop() {}
