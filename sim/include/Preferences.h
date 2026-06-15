#ifndef DESKCLOCK_SIM_PREFERENCES_H
#define DESKCLOCK_SIM_PREFERENCES_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "Arduino.h"

class Preferences {
public:
  static void clear()
  {
    store().clear();
    writeCountRef() = 0;
  }

  static size_t writeCount()
  {
    return writeCountRef();
  }

  bool begin(const char *name, bool readOnly = false)
  {
    namespace_ = name == nullptr ? "default" : name;
    read_only_ = readOnly;
    return true;
  }

  void end() {}

  bool isKey(const char *key) const
  {
    return store().find(fullKey(key)) != store().end();
  }

  bool putBool(const char *key, bool value)
  {
    if (read_only_) {
      return false;
    }
    store()[fullKey(key)] = std::vector<uint8_t>{static_cast<uint8_t>(value ? 1 : 0)};
    writeCountRef()++;
    return true;
  }

  bool getBool(const char *key, bool defaultValue = false) const
  {
    auto it = store().find(fullKey(key));
    return it == store().end() || it->second.empty() ? defaultValue : it->second[0] != 0;
  }

  uint8_t putUChar(const char *key, uint8_t value)
  {
    if (read_only_) {
      return 0;
    }
    store()[fullKey(key)] = std::vector<uint8_t>{value};
    writeCountRef()++;
    return value;
  }

  uint8_t getUChar(const char *key, uint8_t defaultValue = 0) const
  {
    auto it = store().find(fullKey(key));
    return it == store().end() || it->second.empty() ? defaultValue : it->second[0];
  }

  size_t putString(const char *key, const char *value)
  {
    if (read_only_) {
      return 0;
    }
    const char *safe_value = value == nullptr ? "" : value;
    store()[fullKey(key)] = std::vector<uint8_t>(safe_value, safe_value + std::strlen(safe_value) + 1);
    writeCountRef()++;
    return std::strlen(safe_value);
  }

  String getString(const char *key, const char *defaultValue = "") const
  {
    auto it = store().find(fullKey(key));
    if (it == store().end() || it->second.empty()) {
      return String(defaultValue);
    }
    return String(reinterpret_cast<const char *>(it->second.data()));
  }

  size_t putBytes(const char *key, const void *value, size_t length)
  {
    if (read_only_ || value == nullptr) {
      return 0;
    }
    const uint8_t *bytes = static_cast<const uint8_t *>(value);
    store()[fullKey(key)] = std::vector<uint8_t>(bytes, bytes + length);
    writeCountRef()++;
    return length;
  }

  size_t getBytesLength(const char *key) const
  {
    auto it = store().find(fullKey(key));
    return it == store().end() ? 0 : it->second.size();
  }

  size_t getBytes(const char *key, void *value, size_t maxLen) const
  {
    auto it = store().find(fullKey(key));
    if (it == store().end() || value == nullptr) {
      return 0;
    }
    const size_t length = std::min(maxLen, it->second.size());
    std::memcpy(value, it->second.data(), length);
    return length;
  }

private:
  std::string fullKey(const char *key) const
  {
    return namespace_ + "/" + (key == nullptr ? "" : key);
  }

  static std::map<std::string, std::vector<uint8_t>> &store()
  {
    static std::map<std::string, std::vector<uint8_t>> values;
    return values;
  }

  static size_t &writeCountRef()
  {
    static size_t count = 0;
    return count;
  }

  std::string namespace_ = "default";
  bool read_only_ = false;
};

#endif /* DESKCLOCK_SIM_PREFERENCES_H */
