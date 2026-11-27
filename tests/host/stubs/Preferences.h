// Host stand-in for the ESP32 Preferences (NVS) API (tests only). Storage is
// static, so it survives a simulated reboot within the test program.
#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

class Preferences {
 public:
  static std::map<std::string, std::vector<uint8_t>>& storage() {
    static std::map<std::string, std::vector<uint8_t>> s;
    return s;
  }
  bool begin(const char* ns, bool /*readOnly*/ = false) {
    ns_ = ns;
    return true;
  }
  size_t putBytes(const char* key, const void* value, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(value);
    storage()[ns_ + "/" + key] = std::vector<uint8_t>(p, p + len);
    return len;
  }
  // Like the ESP32 version: copies only if the buffer is big enough.
  size_t getBytes(const char* key, void* buf, size_t maxLen) {
    auto it = storage().find(ns_ + "/" + key);
    if (it == storage().end() || it->second.size() > maxLen) return 0;
    memcpy(buf, it->second.data(), it->second.size());
    return it->second.size();
  }
  size_t putInt(const char* key, int32_t value) { return putBytes(key, &value, sizeof value); }
  int32_t getInt(const char* key, int32_t def = 0) {
    int32_t v = def;
    return getBytes(key, &v, sizeof v) == sizeof v ? v : def;
  }

 private:
  std::string ns_;
};
