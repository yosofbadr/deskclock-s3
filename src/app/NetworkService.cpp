#include "NetworkService.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_wifi.h>

namespace DeskClock {
namespace {

constexpr const char *kPreferencesNamespace = "deskclock";
constexpr const char *kEnabledKey = "wifi_en";
constexpr const char *kDemoSelectedKey = "wifi_demo";
constexpr const char *kSelectedSsidKey = "wifi_ssid";
constexpr int kMaxScannedNetworks = 5;
constexpr size_t kSsidBufferLength = 33;

bool network_enabled = false;
bool demo_network_selected = false;
char selected_ssid[kSsidBufferLength] = "";
char scanned_ssids[kMaxScannedNetworks][kSsidBufferLength] = {};
int scanned_count = 0;

void save()
{
  Preferences preferences;
  if (!preferences.begin(kPreferencesNamespace, false)) {
    Serial.println("NetworkService: failed to open preferences for write");
    return;
  }
  preferences.putBool(kEnabledKey, network_enabled);
  preferences.putBool(kDemoSelectedKey, demo_network_selected);
  preferences.putString(kSelectedSsidKey, selected_ssid);
  preferences.end();
}

} // namespace

namespace NetworkService {

void begin()
{
  Preferences preferences;
  if (preferences.begin(kPreferencesNamespace, true)) {
    network_enabled = preferences.getBool(kEnabledKey, false);
    demo_network_selected = preferences.getBool(kDemoSelectedKey, false);
    String stored_ssid = preferences.getString(kSelectedSsidKey, "");
    strlcpy(selected_ssid, stored_ssid.c_str(), sizeof(selected_ssid));
    preferences.end();
  }
}

void loop()
{
  // Placeholder for future credential entry/connect/NTP sync work. Core clock/alarm remains local.
}

NetworkSnapshot snapshot()
{
  NetworkSnapshot result;
  result.enabled = network_enabled;
  result.connected = false;
  result.ssid = selected_ssid[0] != '\0' ? selected_ssid : (demo_network_selected ? "Demo network" : "not selected");
  if (result.connected) {
    result.status = "connected";
  } else if (!network_enabled) {
    result.status = "Wi-Fi skipped";
  } else if (selected_ssid[0] != '\0') {
    result.status = "network selected; password needed";
  } else if (demo_network_selected) {
    result.status = "demo selected; not connected";
  } else if (scanned_count > 0) {
    result.status = "scan complete";
  } else {
    result.status = "select network";
  }
  return result;
}

void setEnabled(bool enabled)
{
  network_enabled = enabled;
  save();
}

void selectDemoNetwork()
{
  demo_network_selected = true;
  selected_ssid[0] = '\0';
  network_enabled = true;
  save();
}

int scanNetworks()
{
  scanned_count = 0;
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err = esp_wifi_init(&cfg);
  if (err != ESP_OK && err != ESP_ERR_WIFI_INIT_STATE) {
    Serial.printf("NetworkService: wifi init failed err=%d\n", err);
    return 0;
  }
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_start());

  wifi_scan_config_t scan_config = {};
  err = esp_wifi_scan_start(&scan_config, true);
  if (err != ESP_OK) {
    Serial.printf("NetworkService: scan failed err=%d\n", err);
    return 0;
  }

  uint16_t found = 0;
  esp_wifi_scan_get_ap_num(&found);
  wifi_ap_record_t records[kMaxScannedNetworks] = {};
  uint16_t requested = kMaxScannedNetworks;
  esp_wifi_scan_get_ap_records(&requested, records);
  scanned_count = requested;
  for (int index = 0; index < scanned_count; ++index) {
    strlcpy(scanned_ssids[index], reinterpret_cast<const char *>(records[index].ssid), kSsidBufferLength);
  }
  Serial.printf("NetworkService: scanned %u networks, showing %d\n", found, scanned_count);
  return scanned_count;
}

int scannedNetworkCount()
{
  return scanned_count;
}

const char *scannedSsid(int index)
{
  if (index < 0 || index >= scanned_count) {
    return "";
  }
  return scanned_ssids[index];
}

void selectScannedNetwork(int index)
{
  if (index < 0 || index >= scanned_count) {
    return;
  }
  strlcpy(selected_ssid, scanned_ssids[index], sizeof(selected_ssid));
  demo_network_selected = false;
  network_enabled = true;
  save();
}

} // namespace NetworkService
} // namespace DeskClock
