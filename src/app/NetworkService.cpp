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
constexpr const char *kPasswordKey = "wifi_pass";
constexpr int kMaxScannedNetworks = 5;
constexpr size_t kSsidBufferLength = 33;
constexpr size_t kPasswordBufferLength = 65;

bool network_enabled = false;
bool demo_network_selected = false;
char selected_ssid[kSsidBufferLength] = "";
char selected_password[kPasswordBufferLength] = "";
char password_preview[kPasswordBufferLength] = "";
char scanned_ssids[kMaxScannedNetworks][kSsidBufferLength] = {};
int scanned_count = 0;

void update_password_preview()
{
  const size_t length = strlen(selected_password);
  const size_t visible = length < 2 ? length : 2;
  for (size_t index = 0; index < length && index + 1 < sizeof(password_preview); ++index) {
    password_preview[index] = index < visible ? selected_password[index] : '*';
  }
  password_preview[length] = '\0';
}

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
  preferences.putString(kPasswordKey, selected_password);
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
    String stored_password = preferences.getString(kPasswordKey, "");
    strlcpy(selected_ssid, stored_ssid.c_str(), sizeof(selected_ssid));
    strlcpy(selected_password, stored_password.c_str(), sizeof(selected_password));
    update_password_preview();
    preferences.end();
  }
}

void loop()
{
  // Placeholder for future NTP sync work. Core clock/alarm remains local.
}

NetworkSnapshot snapshot()
{
  NetworkSnapshot result;
  result.enabled = network_enabled;
  wifi_ap_record_t ap_info = {};
  result.connected = esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
  result.ssid = selected_ssid[0] != '\0' ? selected_ssid : (demo_network_selected ? "Demo network" : "not selected");
  result.password_preview = password_preview;
  if (result.connected) {
    result.status = "connected";
  } else if (!network_enabled) {
    result.status = "Wi-Fi skipped";
  } else if (selected_ssid[0] != '\0' && selected_password[0] != '\0') {
    result.status = "credentials saved; not connected";
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
  selected_password[0] = '\0';
  update_password_preview();
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

void appendPasswordChar(char value)
{
  const size_t length = strlen(selected_password);
  if (length + 1 >= sizeof(selected_password)) {
    return;
  }
  selected_password[length] = value;
  selected_password[length + 1] = '\0';
  update_password_preview();
  save();
}

void backspacePassword()
{
  const size_t length = strlen(selected_password);
  if (length == 0) {
    return;
  }
  selected_password[length - 1] = '\0';
  update_password_preview();
  save();
}

void clearPassword()
{
  selected_password[0] = '\0';
  update_password_preview();
  save();
}

bool connectSelected()
{
  if (selected_ssid[0] == '\0') {
    return false;
  }

  wifi_config_t config = {};
  strlcpy(reinterpret_cast<char *>(config.sta.ssid), selected_ssid, sizeof(config.sta.ssid));
  strlcpy(reinterpret_cast<char *>(config.sta.password), selected_password, sizeof(config.sta.password));
  config.sta.threshold.authmode = WIFI_AUTH_OPEN;

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err = esp_wifi_init(&cfg);
  if (err != ESP_OK && err != ESP_ERR_WIFI_INIT_STATE) {
    Serial.printf("NetworkService: wifi init failed err=%d\n", err);
    return false;
  }
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_STA));
  err = esp_wifi_set_config(WIFI_IF_STA, &config);
  if (err != ESP_OK) {
    Serial.printf("NetworkService: set config failed err=%d\n", err);
    return false;
  }
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_start());
  err = esp_wifi_connect();
  Serial.printf("NetworkService: connect to %s result=%d\n", selected_ssid, err);
  return err == ESP_OK;
}

} // namespace NetworkService
} // namespace DeskClock
