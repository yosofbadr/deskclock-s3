#ifndef DESKCLOCK_SIM_ARDUINO_H
#define DESKCLOCK_SIM_ARDUINO_H

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#ifndef LOW
#define LOW 0
#endif
#ifndef HIGH
#define HIGH 1
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 2
#endif

class String {
public:
  String() = default;
  String(const char *value) : value_(value == nullptr ? "" : value) {}
  String(const std::string &value) : value_(value) {}
  String &operator=(const char *value)
  {
    value_ = value == nullptr ? "" : value;
    return *this;
  }
  const char *c_str() const { return value_.c_str(); }

private:
  std::string value_;
};

class SerialStub {
public:
  void begin(unsigned long) {}
  void println(const char *message) { std::printf("%s\n", message == nullptr ? "" : message); }
  void print(const char *message) { std::printf("%s", message == nullptr ? "" : message); }
  int printf(const char *format, ...)
  {
    va_list args;
    va_start(args, format);
    const int result = std::vprintf(format, args);
    va_end(args);
    return result;
  }
};

extern SerialStub Serial;

inline uint32_t millis()
{
  using clock = std::chrono::steady_clock;
  static const auto started_at = clock::now();
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - started_at).count());
}

inline void delay(uint32_t ms)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline void pinMode(int, int) {}
inline int digitalRead(int) { return HIGH; }

inline void configTzTime(const char *, const char *, const char *) {}

using std::min;
using std::max;

#endif /* DESKCLOCK_SIM_ARDUINO_H */
