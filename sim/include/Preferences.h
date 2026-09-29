#pragma once
// Simulator stand-ins for Arduino Preferences + Serial (settings are not persisted).
#include <cstddef>
#include <cstdint>
#include <cstdio>

struct FakeSerial {
  template <typename... A>
  void printf(const char* fmt, A... a) { std::printf(fmt, a...); }
  void println(const char* s) { std::puts(s); }
};
extern FakeSerial Serial;

class Preferences {
 public:
  bool begin(const char*, bool = false) { return true; }
  void end() {}
  bool isKey(const char*) { return false; }
  bool remove(const char*) { return true; }
  double getDouble(const char*, double d) { return d; }
  size_t putDouble(const char*, double) { return 8; }
  uint8_t getUChar(const char*, uint8_t d) { return d; }
  size_t putUChar(const char*, uint8_t) { return 1; }
  bool getBool(const char*, bool d) { return d; }
  size_t putBool(const char*, bool) { return 1; }
};
