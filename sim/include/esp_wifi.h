#ifndef DESKCLOCK_SIM_ESP_WIFI_H
#define DESKCLOCK_SIM_ESP_WIFI_H

#include <cstdint>
#include <cstring>

#include "esp_err.h"

#define WIFI_AUTH_OPEN 0
#define WIFI_MODE_STA 1
#define WIFI_IF_STA 0

typedef struct {
  struct {
    uint8_t ssid[32];
    uint8_t password[64];
    struct {
      int authmode;
    } threshold;
  } sta;
} wifi_config_t;

typedef struct {
  int placeholder;
} wifi_init_config_t;

#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}

typedef struct {
  uint8_t ssid[33];
} wifi_ap_record_t;

typedef struct {
  int placeholder;
} wifi_scan_config_t;

inline esp_err_t esp_wifi_init(const wifi_init_config_t *) { return ESP_OK; }
inline esp_err_t esp_wifi_set_mode(int) { return ESP_OK; }
inline esp_err_t esp_wifi_set_config(int, const wifi_config_t *) { return ESP_OK; }
inline esp_err_t esp_wifi_start() { return ESP_OK; }
inline esp_err_t esp_wifi_connect() { return ESP_FAIL; }
inline esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *) { return ESP_FAIL; }
inline esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *, bool) { return ESP_OK; }
inline esp_err_t esp_wifi_scan_get_ap_num(uint16_t *number)
{
  if (number != nullptr) {
    *number = 3;
  }
  return ESP_OK;
}
inline esp_err_t esp_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *records)
{
  static const char *names[] = {"DeskClock Lab", "Home Wi-Fi", "Guest"};
  const uint16_t count = number == nullptr ? 0 : ((*number < 3) ? *number : 3);
  for (uint16_t index = 0; index < count; ++index) {
    std::strncpy(reinterpret_cast<char *>(records[index].ssid), names[index], sizeof(records[index].ssid) - 1);
  }
  if (number != nullptr) {
    *number = count;
  }
  return ESP_OK;
}

#endif /* DESKCLOCK_SIM_ESP_WIFI_H */
