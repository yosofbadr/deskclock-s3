#include "NetworkService.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <time.h>

#ifndef DESKCLOCK_SIMULATOR
#include <WebServer.h>
#include <WiFi.h>
#endif

#include "BrightnessService.h"
#include "SettingsService.h"
#include "TimeService.h"

extern "C" void clock_face_refresh_theme(void);

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
bool ntp_started = false;
bool ntp_synced = false;
uint8_t ntp_timezone_index = 255;
uint32_t last_ntp_check_ms = 0;
uint32_t last_connect_attempt_ms = 0;

volatile bool phone_setup_requested = false;
volatile bool phone_setup_active = false;
bool phone_setup_credentials_pending = false;
char phone_setup_name[32] = "DESKCLOCK";
char phone_setup_pin[12] = "DC000000";
char phone_setup_transport[8] = "SoftAP";
char setup_url[40] = "not connected";
char pending_ssid[kSsidBufferLength] = "";
char pending_password[kPasswordBufferLength] = "";

#ifndef DESKCLOCK_SIMULATOR
WebServer settings_server(80);
bool settings_server_routes_registered = false;
bool settings_server_started = false;
#endif

void save();

void update_password_preview()
{
  const size_t length = strlen(selected_password);
  const size_t visible = length < 2 ? length : 2;
  for (size_t index = 0; index < length && index + 1 < sizeof(password_preview); ++index) {
    password_preview[index] = index < visible ? selected_password[index] : '*';
  }
  password_preview[length] = '\0';
}

void set_setup_url_not_connected()
{
  strlcpy(setup_url, "not connected", sizeof(setup_url));
}

#ifndef DESKCLOCK_SIMULATOR
String html_escape(const char *value)
{
  String escaped;
  if (value == nullptr) {
    return escaped;
  }
  for (const char *cursor = value; *cursor != '\0'; ++cursor) {
    switch (*cursor) {
    case '&':
      escaped += F("&amp;");
      break;
    case '<':
      escaped += F("&lt;");
      break;
    case '>':
      escaped += F("&gt;");
      break;
    case '"':
      escaped += F("&quot;");
      break;
    default:
      escaped += *cursor;
      break;
    }
  }
  return escaped;
}

void update_setup_url_from_wifi()
{
  IPAddress ip = WiFi.localIP();
  if ((ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) ||
      (ip[0] == 255 && ip[1] == 255 && ip[2] == 255 && ip[3] == 255)) {
    set_setup_url_not_connected();
    return;
  }
  snprintf(setup_url, sizeof(setup_url), "http://%u.%u.%u.%u/", ip[0], ip[1], ip[2], ip[3]);
}

void send_web_settings_page()
{
  SettingsSnapshot settings = SettingsService::snapshot();
  BrightnessSettings brightness = BrightnessService::settings();
  NetworkSnapshot network = NetworkService::snapshot();

  String page;
  page.reserve(4600);
  page += F("<!doctype html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>");
  page += F("<title>DeskClock setup</title><style>body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif;margin:24px;max-width:620px;background:#020617;color:#e2e8f0}label{display:block;margin:18px 0 6px;color:#94a3b8}select,button{font:inherit;width:100%;padding:12px;border-radius:10px;border:1px solid #475569;background:#0f172a;color:#f8fafc}button{margin-top:22px;background:#2563eb;border-color:#2563eb}.card{padding:16px;border:1px solid #334155;border-radius:14px;background:#0f172a}a{color:#93c5fd}</style></head><body>");
  page += F("<h1>DeskClock setup</h1><div class=card><p><b>Status:</b> ");
  page += html_escape(network.status);
  page += F("</p><p><b>Network:</b> ");
  page += html_escape(network.ssid);
  page += F("</p><p><b>This page:</b> <a href='/'>");
  page += html_escape(network.setup_url);
  page += F("</a></p></div><form method=post action='/settings'>");

  page += F("<label for=wifi_ssid>Wi-Fi network</label><input id=wifi_ssid name=wifi_ssid value='");
  page += html_escape(selected_ssid);
  page += F("' placeholder='Home Wi-Fi' style='font:inherit;width:100%;box-sizing:border-box;padding:12px;border-radius:10px;border:1px solid #475569;background:#0f172a;color:#f8fafc'>");
  page += F("<label for=wifi_pass>Wi-Fi password</label><input id=wifi_pass name=wifi_pass type=password placeholder='Leave blank to keep saved password' style='font:inherit;width:100%;box-sizing:border-box;padding:12px;border-radius:10px;border:1px solid #475569;background:#0f172a;color:#f8fafc'>");

  page += F("<label for=tz>Timezone</label><select id=tz name=tz>");
  for (uint8_t index = 0; index < SettingsService::timezoneCount(); ++index) {
    page += F("<option value='");
    page += index;
    page += index == settings.timezone_index ? F("' selected>") : F("'>");
    page += html_escape(SettingsService::timezoneLabel(index));
    page += F("</option>");
  }
  page += F("</select>");

  page += F("<label for=theme>Theme</label><select id=theme name=theme>");
  for (uint8_t index = 0; index < SettingsService::themeCount(); ++index) {
    page += F("<option value='");
    page += index;
    page += index == settings.theme_index ? F("' selected>") : F("'>Theme ");
    page += static_cast<unsigned>(index + 1U);
    page += F("</option>");
  }
  page += F("</select>");

  page += F("<label for=brightness>Brightness</label><select id=brightness name=brightness>");
  for (uint8_t index = 0; index < BrightnessService::presetCount(); ++index) {
    const uint8_t value = BrightnessService::presetValue(index);
    page += F("<option value='");
    page += index;
    page += value == brightness.day_brightness ? F("' selected>") : F("'>");
    page += value;
    page += F("</option>");
  }
  page += F("</select><button type=submit>Save settings</button></form></body></html>");
  settings_server.send(200, "text/html", page);
}

void handle_web_settings_post()
{
  bool theme_changed = false;
  bool timezone_changed = false;
  if (settings_server.hasArg("wifi_ssid") && settings_server.arg("wifi_ssid").length() > 0) {
    strlcpy(pending_ssid, settings_server.arg("wifi_ssid").c_str(), sizeof(pending_ssid));
    if (settings_server.hasArg("wifi_pass") && settings_server.arg("wifi_pass").length() > 0) {
      strlcpy(pending_password, settings_server.arg("wifi_pass").c_str(), sizeof(pending_password));
    } else {
      strlcpy(pending_password, selected_password, sizeof(pending_password));
    }
    phone_setup_credentials_pending = true;
  }
  if (settings_server.hasArg("tz")) {
    const uint8_t index = static_cast<uint8_t>(settings_server.arg("tz").toInt());
    SettingsService::setTimezoneIndex(index);
    timezone_changed = true;
  }
  if (settings_server.hasArg("theme")) {
    const uint8_t index = static_cast<uint8_t>(settings_server.arg("theme").toInt());
    SettingsService::setThemeIndex(index);
    theme_changed = true;
  }
  if (settings_server.hasArg("brightness")) {
    const uint8_t index = static_cast<uint8_t>(settings_server.arg("brightness").toInt());
    const uint8_t value = BrightnessService::presetValue(index);
    BrightnessSettings brightness = BrightnessService::settings();
    brightness.day_brightness = value;
    brightness.night_brightness = value;
    BrightnessService::updateSettings(brightness);
  }
  if (timezone_changed) {
    ntp_started = false;
    ntp_synced = false;
  }
  SettingsService::setConfigured(true);
  if (theme_changed) {
    clock_face_refresh_theme();
  }
  settings_server.sendHeader("Location", "/");
  settings_server.send(303, "text/plain", "Saved");
}

void ensure_settings_server_routes()
{
  if (settings_server_routes_registered) {
    return;
  }
  settings_server.on("/", HTTP_GET, send_web_settings_page);
  settings_server.on("/settings", HTTP_POST, handle_web_settings_post);
  settings_server.on("/status", HTTP_GET, []() {
    NetworkSnapshot network = NetworkService::snapshot();
    String json = "{\"status\":\"" + html_escape(network.status) + "\",\"ssid\":\"" + html_escape(network.ssid) + "\",\"setup_url\":\"" + html_escape(network.setup_url) + "\"}";
    settings_server.send(200, "application/json", json);
  });
  settings_server_routes_registered = true;
}

void maybe_start_settings_server()
{
  update_setup_url_from_wifi();
  if (settings_server_started || strcmp(setup_url, "not connected") == 0) {
    return;
  }
  ensure_settings_server_routes();
  settings_server.begin();
  settings_server_started = true;
  Serial.printf("NetworkService: web setup available at %s\n", setup_url);
}

void handle_settings_server()
{
  if (settings_server_started) {
    settings_server.handleClient();
  }
}

void generate_phone_setup_identity()
{
  const uint64_t mac = ESP.getEfuseMac();
  const uint32_t suffix = static_cast<uint32_t>(mac & 0xFFFFU);
  const uint32_t pin = static_cast<uint32_t>(mac % 1000000ULL);
  snprintf(phone_setup_name, sizeof(phone_setup_name), "PROV_DC%04lX", static_cast<unsigned long>(suffix));
  snprintf(phone_setup_pin, sizeof(phone_setup_pin), "DC%06lu", static_cast<unsigned long>(pin));
  strlcpy(phone_setup_transport, "SoftAP", sizeof(phone_setup_transport));
}

bool begin_phone_setup_transport()
{
  phone_setup_requested = false;
  phone_setup_active = true;
  phone_setup_credentials_pending = false;
  generate_phone_setup_identity();

  Serial.printf("NetworkService: starting setup AP ssid=%s pass=%s\n", phone_setup_name, phone_setup_pin);
  WiFi.mode(WIFI_AP_STA);
  const bool ok = WiFi.softAP(phone_setup_name, phone_setup_pin);
  if (!ok) {
    phone_setup_active = false;
    Serial.println("NetworkService: setup access point failed to start");
    return false;
  }
  IPAddress ip = WiFi.softAPIP();
  snprintf(setup_url, sizeof(setup_url), "http://%u.%u.%u.%u/", ip[0], ip[1], ip[2], ip[3]);
  ensure_settings_server_routes();
  if (!settings_server_started) {
    settings_server.begin();
    settings_server_started = true;
  }
  Serial.printf("NetworkService: setup AP started ssid=%s pass=%s url=%s\n", phone_setup_name, phone_setup_pin, setup_url);
  return true;
}

void handle_wifi_event(arduino_event_t *event)
{
  switch (event->event_id) {
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    update_setup_url_from_wifi();
    Serial.printf("NetworkService: Wi-Fi got IP, setup at %s\n", setup_url);
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    set_setup_url_not_connected();
    break;
  default:
    break;
  }
}
#else
void handle_settings_server() {}
void maybe_start_settings_server() {}
void generate_phone_setup_identity()
{
  strlcpy(phone_setup_name, "DESKCLOCK-SIM", sizeof(phone_setup_name));
  strlcpy(phone_setup_pin, "DC000000", sizeof(phone_setup_pin));
  strlcpy(phone_setup_transport, "SoftAP", sizeof(phone_setup_transport));
}

bool begin_phone_setup_transport()
{
  phone_setup_requested = false;
  phone_setup_active = true;
  Serial.printf("NetworkService: simulated phone setup name=%s pin=%s\n", phone_setup_name, phone_setup_pin);
  return true;
}
#endif

void apply_pending_phone_credentials()
{
  if (!phone_setup_credentials_pending) {
    return;
  }
  strlcpy(selected_ssid, pending_ssid, sizeof(selected_ssid));
  strlcpy(selected_password, pending_password, sizeof(selected_password));
  demo_network_selected = false;
  network_enabled = true;
  update_password_preview();
  save();
  phone_setup_credentials_pending = false;
  Serial.printf("NetworkService: saved phone credentials for %s\n", selected_ssid);
}

bool is_wifi_connected()
{
#ifndef DESKCLOCK_SIMULATOR
  return WiFi.status() == WL_CONNECTED;
#else
  wifi_ap_record_t ap_info = {};
  return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
#endif
}

bool start_wifi_connection()
{
  if (selected_ssid[0] == '\0') {
    return false;
  }

  ntp_started = false;
  ntp_synced = false;
#ifndef DESKCLOCK_SIMULATOR
  WiFi.mode(WIFI_STA);
  WiFi.begin(selected_ssid, selected_password);
  Serial.printf("NetworkService: connecting to %s with Arduino WiFi\n", selected_ssid);
  return true;
#else
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
#endif
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
  generate_phone_setup_identity();
#ifndef DESKCLOCK_SIMULATOR
  WiFi.onEvent(handle_wifi_event);
#endif

  Preferences preferences;
  if (preferences.begin(kPreferencesNamespace, true)) {
    network_enabled = preferences.getBool(kEnabledKey, false);
    demo_network_selected = preferences.getBool(kDemoSelectedKey, false);

    String stored_ssid;
    String stored_password;
    if (preferences.isKey(kSelectedSsidKey)) {
      stored_ssid = preferences.getString(kSelectedSsidKey, "");
    }
    if (preferences.isKey(kPasswordKey)) {
      stored_password = preferences.getString(kPasswordKey, "");
    }

    strlcpy(selected_ssid, stored_ssid.c_str(), sizeof(selected_ssid));
    strlcpy(selected_password, stored_password.c_str(), sizeof(selected_password));
    update_password_preview();
    preferences.end();
  }
}

void loop()
{
  handle_settings_server();
  apply_pending_phone_credentials();
  if (phone_setup_requested) {
    (void)begin_phone_setup_transport();
    return;
  }

  if (!network_enabled) {
    return;
  }

  if (!is_wifi_connected()) {
    if (phone_setup_active) {
      return;
    }
    const uint32_t now_ms = millis();
    if (!demo_network_selected && selected_ssid[0] != '\0' &&
        (last_connect_attempt_ms == 0 || now_ms - last_connect_attempt_ms >= 30000UL)) {
      last_connect_attempt_ms = now_ms;
      (void)start_wifi_connection();
    }
    return;
  }

  maybe_start_settings_server();

  SettingsSnapshot settings = SettingsService::snapshot();
  if (ntp_timezone_index != settings.timezone_index) {
    ntp_started = false;
    ntp_synced = false;
  }

  if (!ntp_started) {
    configTzTime(settings.timezone_posix, "pool.ntp.org", "time.nist.gov");
    ntp_started = true;
    ntp_timezone_index = settings.timezone_index;
    last_ntp_check_ms = 0;
    Serial.printf("NetworkService: started NTP sync for %s\n", settings.timezone_label);
  }

  const uint32_t now_ms = millis();
  if (ntp_synced || (last_ntp_check_ms != 0 && now_ms - last_ntp_check_ms < 5000)) {
    return;
  }
  last_ntp_check_ms = now_ms;

  time_t epoch = time(nullptr);
  if (epoch < 1704067200) { // 2024-01-01 UTC
    return;
  }

  struct tm timeinfo = {};
  localtime_r(&epoch, &timeinfo);
  DateTime synced;
  synced.year = static_cast<uint16_t>(timeinfo.tm_year + 1900);
  synced.month = static_cast<uint8_t>(timeinfo.tm_mon + 1);
  synced.day = static_cast<uint8_t>(timeinfo.tm_mday);
  synced.hour = static_cast<uint8_t>(timeinfo.tm_hour);
  synced.minute = static_cast<uint8_t>(timeinfo.tm_min);
  synced.second = static_cast<uint8_t>(timeinfo.tm_sec);
  synced.valid = true;
  if (TimeService::setNetworkTime(synced)) {
    ntp_synced = true;
    Serial.printf("NetworkService: RTC updated from NTP (%s)\n", settings.timezone_label);
  }
}

NetworkSnapshot snapshot()
{
  NetworkSnapshot result;
  result.enabled = network_enabled;
  result.phone_setup_active = phone_setup_active;
  result.phone_setup_name = phone_setup_name;
  result.phone_setup_pin = phone_setup_pin;
  result.phone_setup_transport = phone_setup_transport;
  result.setup_url = setup_url;
  result.connected = is_wifi_connected();
  result.ssid = selected_ssid[0] != '\0' ? selected_ssid : (demo_network_selected ? "Demo network" : "not selected");
  result.password_preview = password_preview;
  if (phone_setup_active) {
    result.status = "phone setup active";
  } else if (phone_setup_credentials_pending) {
    result.status = "phone credentials received";
  } else if (result.connected && ntp_synced) {
    result.status = "connected; time synced";
  } else if (result.connected) {
    result.status = strcmp(setup_url, "not connected") == 0 ? "connected; setup starting" : "connected; setup web ready";
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
#ifndef DESKCLOCK_SIMULATOR
  WiFi.mode(WIFI_STA);
  const int found = WiFi.scanNetworks();
  if (found < 0) {
    Serial.printf("NetworkService: scan failed err=%d\n", found);
    return 0;
  }
  scanned_count = found > kMaxScannedNetworks ? kMaxScannedNetworks : found;
  for (int index = 0; index < scanned_count; ++index) {
    strlcpy(scanned_ssids[index], WiFi.SSID(index).c_str(), kSsidBufferLength);
  }
  Serial.printf("NetworkService: scanned %d networks, showing %d\n", found, scanned_count);
  return scanned_count;
#else
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
#endif
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

  last_connect_attempt_ms = millis();
  return start_wifi_connection();
}

bool startPhoneSetup()
{
  if (phone_setup_active) {
    Serial.printf("NetworkService: phone setup already active name=%s pin=%s url=%s\n", phone_setup_name, phone_setup_pin, setup_url);
    return true;
  }
  Serial.println("NetworkService: phone setup requested");
  return begin_phone_setup_transport();
}

} // namespace NetworkService
} // namespace DeskClock
