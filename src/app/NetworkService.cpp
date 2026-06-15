#include "NetworkService.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <time.h>

#ifndef DESKCLOCK_SIMULATOR
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#endif

#include "BrightnessService.h"
#include "SettingsService.h"
#include "TimeService.h"
#include "lvgl_port.h"

extern "C" void clock_face_refresh_theme(void);

namespace DeskClock {
namespace {

constexpr const char *kPreferencesNamespace = "deskclock";
constexpr const char *kEnabledKey = "wifi_en";
constexpr const char *kDemoSelectedKey = "wifi_demo";
constexpr const char *kSelectedSsidKey = "wifi_ssid";
constexpr const char *kPasswordKey = "wifi_pass";
constexpr int kMaxScannedNetworks = kSetupMaxScannedNetworks;
constexpr size_t kSsidBufferLength = kSetupSsidBufferLength;
constexpr size_t kPasswordBufferLength = kSetupPasswordBufferLength;

bool network_enabled = false;
bool demo_network_selected = false;
char selected_ssid[kSsidBufferLength] = "";
char selected_password[kPasswordBufferLength] = "";
char password_preview[kPasswordBufferLength] = "";
char scanned_ssids[kMaxScannedNetworks][kSsidBufferLength] = {};
int scanned_count = 0;
SetupStatus last_setup_status;
bool ntp_started = false;
bool ntp_synced = false;
uint8_t ntp_timezone_index = 255;
uint32_t last_ntp_check_ms = 0;
uint32_t last_connect_attempt_ms = 0;

volatile bool phone_setup_active = false;
char phone_setup_name[kSetupNameBufferLength] = "DESKCLOCK";
char phone_setup_pin[kSetupPinBufferLength] = "DC000000";
char phone_setup_transport[kSetupTransportBufferLength] = "SoftAP";
char setup_url[kSetupUrlBufferLength] = "not connected";
struct PendingPortalSettings {
  bool pending = false;
  bool mark_configured = false;
  bool timezone_submitted = false;
  uint8_t timezone_index = 0;
  bool theme_submitted = false;
  uint8_t theme_index = 0;
  bool brightness_submitted = false;
  uint8_t brightness_index = 0;
};

PendingPortalSettings pending_portal_settings;
bool theme_refresh_pending = false;
bool connection_attempt_active = false;
uint32_t connection_attempt_started_ms = 0;

#ifndef DESKCLOCK_SIMULATOR
IPAddress setup_ap_ip(192, 168, 4, 1);
IPAddress setup_ap_gateway(192, 168, 4, 1);
IPAddress setup_ap_netmask(255, 255, 255, 0);
DNSServer captive_dns_server;
WebServer settings_server(80);
bool captive_dns_started = false;
bool settings_server_routes_registered = false;
bool settings_server_started = false;
#endif

void save();
bool start_wifi_connection();
bool is_wifi_connected();
void generate_phone_setup_identity(SetupIdentity &identity);
bool begin_phone_setup_transport(SetupIdentity &identity);
int perform_wifi_scan(SetupNetworkRecord *records, int max_records);
void copy_setup_url(char *destination, size_t destination_size);
void apply_pending_portal_settings();
void service_pending_theme_refresh();

class NetworkSetupSessionAdapters : public SetupSessionAdapters {
public:
  bool startPhoneSetup(SetupIdentity &identity) override;
  bool startPortal(const SetupIdentity &identity) override;
  int scanNetworks(SetupNetworkRecord *records, int max_records) override;
  bool saveCredentials(const char *ssid, const char *password, bool enabled, bool demo_selected) override;
  bool setEnabled(bool enabled) override;
  bool saveDemoNetwork() override;
  bool startConnection(const char *ssid, const char *password) override;
  bool isConnected() override;
  bool connectionFailed() override;
  void updateSetupUrl(char *url, size_t url_size) override;
};

NetworkSetupSessionAdapters setup_adapters;
SetupSession setup_session(setup_adapters);

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

void copy_setup_url(char *destination, size_t destination_size)
{
  if (destination == nullptr || destination_size == 0) {
    return;
  }
  strlcpy(destination, setup_url, destination_size);
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

void set_setup_url_from_ap()
{
  IPAddress ip = WiFi.softAPIP();
  if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) {
    ip = setup_ap_ip;
  }
  snprintf(setup_url, sizeof(setup_url), "http://%u.%u.%u.%u/", ip[0], ip[1], ip[2], ip[3]);
}

void log_http_request(const char *label)
{
  IPAddress remote = settings_server.client().remoteIP();
  Serial.printf("NetworkService: HTTP %s %s from %u.%u.%u.%u\n",
                label,
                settings_server.uri().c_str(),
                remote[0],
                remote[1],
                remote[2],
                remote[3]);
}

void send_web_settings_page()
{
  log_http_request("GET");
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
  page += F("</p><p><b>Credentials:</b> ");
  page += network.credentials_saved ? F("saved") : F("not saved");
  page += F("</p><p><b>Retry:</b> ");
  page += network.retry_available ? F("available") : F("not needed");
  page += F("</p><p><b>Setup network:</b> ");
  page += html_escape(network.phone_setup_name);
  page += F(" / ");
  page += html_escape(network.phone_setup_pin);
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
  log_http_request("POST");
  bool credentials_submitted = false;
  bool credentials_saved = false;
  bool settings_submitted = false;
  if (settings_server.hasArg("wifi_ssid") && settings_server.arg("wifi_ssid").length() > 0) {
    char submitted_ssid[kSsidBufferLength] = "";
    char submitted_password[kPasswordBufferLength] = "";
    strlcpy(submitted_ssid, settings_server.arg("wifi_ssid").c_str(), sizeof(submitted_ssid));
    if (settings_server.hasArg("wifi_pass") && settings_server.arg("wifi_pass").length() > 0) {
      strlcpy(submitted_password, settings_server.arg("wifi_pass").c_str(), sizeof(submitted_password));
    } else {
      strlcpy(submitted_password, selected_password, sizeof(submitted_password));
    }
    credentials_submitted = true;
    credentials_saved = setup_session.dispatch(SetupIntent::submitCredentials(submitted_ssid, submitted_password));
  }
  if (settings_server.hasArg("tz")) {
    pending_portal_settings.timezone_submitted = true;
    pending_portal_settings.timezone_index = static_cast<uint8_t>(settings_server.arg("tz").toInt());
    settings_submitted = true;
  }
  if (settings_server.hasArg("theme")) {
    pending_portal_settings.theme_submitted = true;
    pending_portal_settings.theme_index = static_cast<uint8_t>(settings_server.arg("theme").toInt());
    settings_submitted = true;
  }
  if (settings_server.hasArg("brightness")) {
    pending_portal_settings.brightness_submitted = true;
    pending_portal_settings.brightness_index = static_cast<uint8_t>(settings_server.arg("brightness").toInt());
    settings_submitted = true;
  }
  if (credentials_submitted || settings_submitted) {
    pending_portal_settings.pending = true;
    pending_portal_settings.mark_configured = true;
  }
  if (credentials_submitted) {
    if (credentials_saved) {
      settings_server.send(200, "text/plain", "Credentials saved; connecting...\n");
    } else {
      settings_server.send(500, "text/plain", "Credential save failed; edit credentials and retry.\n");
    }
    return;
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
  settings_server.on("/ping", HTTP_GET, []() {
    log_http_request("PING");
    settings_server.send(200, "text/plain", "deskclock setup ok\n");
  });
  settings_server.on("/status", HTTP_GET, []() {
    log_http_request("STATUS");
    NetworkSnapshot network = NetworkService::snapshot();
    String json = "{\"status\":\"" + html_escape(network.status) + "\",\"ssid\":\"" + html_escape(network.ssid) + "\",\"credentials_saved\":";
    json += network.credentials_saved ? F("true") : F("false");
    json += F(",\"retry_available\":");
    json += network.retry_available ? F("true") : F("false");
    json += F(",\"setup_url\":\"");
    json += html_escape(network.setup_url);
    json += F("\",\"setup_network\":\"");
    json += html_escape(network.phone_setup_name);
    json += F("\"}");
    settings_server.send(200, "application/json", json);
  });
  settings_server.on("/generate_204", HTTP_GET, send_web_settings_page);
  settings_server.on("/gen_204", HTTP_GET, send_web_settings_page);
  settings_server.on("/hotspot-detect.html", HTTP_GET, send_web_settings_page);
  settings_server.on("/library/test/success.html", HTTP_GET, send_web_settings_page);
  settings_server.on("/connecttest.txt", HTTP_GET, send_web_settings_page);
  settings_server.onNotFound(send_web_settings_page);
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

void start_captive_dns()
{
  if (captive_dns_started) {
    return;
  }
  if (captive_dns_server.start()) {
    captive_dns_started = true;
    Serial.println("NetworkService: captive DNS started");
  } else {
    Serial.println("NetworkService: captive DNS failed to start");
  }
}

void stop_captive_dns()
{
  if (!captive_dns_started) {
    return;
  }
  captive_dns_server.stop();
  captive_dns_started = false;
  Serial.println("NetworkService: captive DNS stopped");
}

void handle_settings_server()
{
  if (captive_dns_started) {
    captive_dns_server.processNextRequest();
  }
  if (settings_server_started) {
    settings_server.handleClient();
  }
}

void generate_phone_setup_identity(SetupIdentity &identity)
{
  const uint64_t mac = ESP.getEfuseMac();
  const uint32_t suffix = static_cast<uint32_t>(mac & 0xFFFFU);
  const uint32_t pin = static_cast<uint32_t>(mac % 1000000ULL);
  snprintf(identity.network_name, sizeof(identity.network_name), "PROV_DC%04lX", static_cast<unsigned long>(suffix));
  snprintf(identity.network_password, sizeof(identity.network_password), "DC%06lu", static_cast<unsigned long>(pin));
  strlcpy(identity.transport, "SoftAP", sizeof(identity.transport));
  strlcpy(identity.url, setup_url, sizeof(identity.url));
  strlcpy(phone_setup_name, identity.network_name, sizeof(phone_setup_name));
  strlcpy(phone_setup_pin, identity.network_password, sizeof(phone_setup_pin));
  strlcpy(phone_setup_transport, identity.transport, sizeof(phone_setup_transport));
}

bool begin_phone_setup_transport(SetupIdentity &identity)
{
  phone_setup_active = true;
  strlcpy(phone_setup_name, identity.network_name, sizeof(phone_setup_name));
  strlcpy(phone_setup_pin, identity.network_password, sizeof(phone_setup_pin));
  strlcpy(phone_setup_transport, identity.transport, sizeof(phone_setup_transport));

  Serial.printf("NetworkService: starting setup AP ssid=%s pass=%s\n", phone_setup_name, phone_setup_pin);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(setup_ap_ip, setup_ap_gateway, setup_ap_netmask);
  const bool ok = WiFi.softAP(phone_setup_name, phone_setup_pin);
  if (!ok) {
    phone_setup_active = false;
    Serial.println("NetworkService: setup access point failed to start");
    return false;
  }
  set_setup_url_from_ap();
  strlcpy(identity.url, setup_url, sizeof(identity.url));
  ensure_settings_server_routes();
  if (!settings_server_started) {
    settings_server.begin();
    settings_server_started = true;
  }
  start_captive_dns();
  Serial.printf("NetworkService: setup AP started ssid=%s pass=%s url=%s\n", phone_setup_name, phone_setup_pin, setup_url);
  return true;
}

void handle_wifi_event(arduino_event_t *event)
{
  switch (event->event_id) {
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    update_setup_url_from_wifi();
    connection_attempt_active = false;
    Serial.printf("NetworkService: Wi-Fi got IP, setup at %s\n", setup_url);
    if (phone_setup_active) {
      stop_captive_dns();
      WiFi.softAPdisconnect(true);
      phone_setup_active = false;
      Serial.println("NetworkService: setup AP stopped after Wi-Fi connection");
    }
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    if (phone_setup_active) {
      set_setup_url_from_ap();
    } else {
      set_setup_url_not_connected();
    }
    break;
  default:
    break;
  }
}
#else
void handle_settings_server() {}
void maybe_start_settings_server() {}
void generate_phone_setup_identity(SetupIdentity &identity)
{
  strlcpy(identity.network_name, "DESKCLOCK-SIM", sizeof(identity.network_name));
  strlcpy(identity.network_password, "DC000000", sizeof(identity.network_password));
  strlcpy(identity.transport, "SoftAP", sizeof(identity.transport));
  strlcpy(identity.url, setup_url, sizeof(identity.url));
  strlcpy(phone_setup_name, identity.network_name, sizeof(phone_setup_name));
  strlcpy(phone_setup_pin, identity.network_password, sizeof(phone_setup_pin));
  strlcpy(phone_setup_transport, identity.transport, sizeof(phone_setup_transport));
}

bool begin_phone_setup_transport(SetupIdentity &identity)
{
  phone_setup_active = true;
  strlcpy(phone_setup_name, identity.network_name, sizeof(phone_setup_name));
  strlcpy(phone_setup_pin, identity.network_password, sizeof(phone_setup_pin));
  strlcpy(phone_setup_transport, identity.transport, sizeof(phone_setup_transport));
  strlcpy(setup_url, "http://192.168.4.1/", sizeof(setup_url));
  strlcpy(identity.url, setup_url, sizeof(identity.url));
  Serial.printf("NetworkService: simulated phone setup name=%s pin=%s\n", phone_setup_name, phone_setup_pin);
  return true;
}
#endif

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
  connection_attempt_active = true;
  connection_attempt_started_ms = millis();
#ifndef DESKCLOCK_SIMULATOR
  WiFi.mode(phone_setup_active ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(selected_ssid, selected_password);
  Serial.printf("NetworkService: connecting to %s with Arduino WiFi%s\n", selected_ssid, phone_setup_active ? " while setup AP stays active" : "");
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

int perform_wifi_scan(SetupNetworkRecord *records, int max_records)
{
  scanned_count = 0;
  if (records == nullptr || max_records <= 0) {
    return 0;
  }
#ifndef DESKCLOCK_SIMULATOR
  WiFi.mode(WIFI_STA);
  const int found = WiFi.scanNetworks();
  if (found < 0) {
    Serial.printf("NetworkService: scan failed err=%d\n", found);
    return 0;
  }
  scanned_count = found > kMaxScannedNetworks ? kMaxScannedNetworks : found;
  if (scanned_count > max_records) {
    scanned_count = max_records;
  }
  for (int index = 0; index < scanned_count; ++index) {
    strlcpy(scanned_ssids[index], WiFi.SSID(index).c_str(), kSsidBufferLength);
    strlcpy(records[index].ssid, scanned_ssids[index], sizeof(records[index].ssid));
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
  wifi_ap_record_t wifi_records[kMaxScannedNetworks] = {};
  uint16_t requested = static_cast<uint16_t>(max_records < kMaxScannedNetworks ? max_records : kMaxScannedNetworks);
  esp_wifi_scan_get_ap_records(&requested, wifi_records);
  scanned_count = requested;
  for (int index = 0; index < scanned_count; ++index) {
    strlcpy(scanned_ssids[index], reinterpret_cast<const char *>(wifi_records[index].ssid), kSsidBufferLength);
    strlcpy(records[index].ssid, scanned_ssids[index], sizeof(records[index].ssid));
  }
  Serial.printf("NetworkService: scanned %u networks, showing %d\n", found, scanned_count);
  return scanned_count;
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

void apply_pending_portal_settings()
{
  if (!pending_portal_settings.pending) {
    return;
  }

  PendingPortalSettings update = pending_portal_settings;
  pending_portal_settings = PendingPortalSettings{};

  if (update.timezone_submitted) {
    SettingsService::setTimezoneIndex(update.timezone_index);
    ntp_started = false;
    ntp_synced = false;
  }
  if (update.theme_submitted) {
    SettingsService::setThemeIndex(update.theme_index);
    theme_refresh_pending = true;
  }
  if (update.brightness_submitted) {
    const uint8_t value = BrightnessService::presetValue(update.brightness_index);
    BrightnessSettings brightness = BrightnessService::settings();
    brightness.day_brightness = value;
    brightness.night_brightness = value;
    BrightnessService::updateSettings(brightness);
  }
  if (update.mark_configured) {
    SettingsService::setConfigured(true);
  }
}

void service_pending_theme_refresh()
{
  if (!theme_refresh_pending) {
    return;
  }
#ifndef DESKCLOCK_SIMULATOR
  if (lvgl_port_lock(500)) {
    clock_face_refresh_theme();
    lvgl_port_unlock();
    theme_refresh_pending = false;
  } else {
    Serial.println("NetworkService: LVGL lock timeout; theme refresh deferred");
  }
#else
  clock_face_refresh_theme();
  theme_refresh_pending = false;
#endif
}

bool NetworkSetupSessionAdapters::startPhoneSetup(SetupIdentity &identity)
{
  return begin_phone_setup_transport(identity);
}

bool NetworkSetupSessionAdapters::startPortal(const SetupIdentity &)
{
#ifndef DESKCLOCK_SIMULATOR
  ensure_settings_server_routes();
  if (!settings_server_started) {
    settings_server.begin();
    settings_server_started = true;
  }
#endif
  return true;
}

int NetworkSetupSessionAdapters::scanNetworks(SetupNetworkRecord *records, int max_records)
{
  return perform_wifi_scan(records, max_records);
}

bool NetworkSetupSessionAdapters::saveCredentials(const char *ssid, const char *password, bool enabled, bool demo_selected)
{
  strlcpy(selected_ssid, ssid == nullptr ? "" : ssid, sizeof(selected_ssid));
  strlcpy(selected_password, password == nullptr ? "" : password, sizeof(selected_password));
  network_enabled = enabled;
  demo_network_selected = demo_selected;
  update_password_preview();
  save();
  Serial.printf("NetworkService: saved setup credentials for %s\n", selected_ssid[0] == '\0' ? "<none>" : selected_ssid);
  return true;
}

bool NetworkSetupSessionAdapters::setEnabled(bool enabled)
{
  network_enabled = enabled;
  if (!enabled) {
    connection_attempt_active = false;
  }
  save();
  return true;
}

bool NetworkSetupSessionAdapters::saveDemoNetwork()
{
  demo_network_selected = true;
  selected_ssid[0] = '\0';
  selected_password[0] = '\0';
  network_enabled = true;
  update_password_preview();
  save();
  return true;
}

bool NetworkSetupSessionAdapters::startConnection(const char *ssid, const char *password)
{
  strlcpy(selected_ssid, ssid == nullptr ? "" : ssid, sizeof(selected_ssid));
  strlcpy(selected_password, password == nullptr ? "" : password, sizeof(selected_password));
  update_password_preview();
  last_connect_attempt_ms = millis();
  return start_wifi_connection();
}

bool NetworkSetupSessionAdapters::isConnected()
{
  return is_wifi_connected();
}

bool NetworkSetupSessionAdapters::connectionFailed()
{
  if (!connection_attempt_active || is_wifi_connected()) {
    return false;
  }
  return millis() - connection_attempt_started_ms >= 30000UL;
}

void NetworkSetupSessionAdapters::updateSetupUrl(char *url, size_t url_size)
{
#ifndef DESKCLOCK_SIMULATOR
  if (is_wifi_connected()) {
    update_setup_url_from_wifi();
  } else if (phone_setup_active) {
    set_setup_url_from_ap();
  }
#endif
  copy_setup_url(url, url_size);
}

} // namespace

namespace NetworkService {

void begin()
{
  SetupIdentity identity;
  generate_phone_setup_identity(identity);
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
  setup_session.begin(identity, selected_ssid, selected_password, network_enabled, demo_network_selected);
}

void loop()
{
  handle_settings_server();
  setup_session.loop();
  apply_pending_portal_settings();
  service_pending_theme_refresh();

  if (!network_enabled) {
    return;
  }

  if (!is_wifi_connected()) {
    if (phone_setup_active) {
      return;
    }
    const SetupStatus status = setup_session.status();
    const uint32_t now_ms = millis();
    if (!demo_network_selected && status.credentials_saved && status.state != SetupState::ConnectionFailed &&
        (last_connect_attempt_ms == 0 || now_ms - last_connect_attempt_ms >= 30000UL)) {
      last_connect_attempt_ms = now_ms;
      (void)setup_session.dispatch(SetupIntent::connectSelected());
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
  last_setup_status = setup_session.status();
  NetworkSnapshot result;
  result.enabled = network_enabled;
  result.connected = is_wifi_connected();
  result.phone_setup_active = last_setup_status.phone_setup_ready || last_setup_status.state == SetupState::PhoneSetupStarting || phone_setup_active;
  result.credentials_saved = last_setup_status.credentials_saved;
  result.retry_available = last_setup_status.retry_available;
  result.setup_state = last_setup_status.state;
  result.phone_setup_name = last_setup_status.setup_network_name;
  result.phone_setup_pin = last_setup_status.setup_network_password;
  result.phone_setup_transport = last_setup_status.setup_transport;
  result.setup_url = last_setup_status.setup_url;
  result.ssid = last_setup_status.selected_network;
  result.password_preview = last_setup_status.password_preview;
  if (result.connected && ntp_synced) {
    result.status = "connected; time synced";
  } else {
    result.status = last_setup_status.status_text;
  }
  return result;
}

void setEnabled(bool enabled)
{
  if (!enabled) {
    (void)setup_session.dispatch(SetupIntent::disableWifi());
    return;
  }
  network_enabled = true;
  save();
  SetupIdentity identity;
  generate_phone_setup_identity(identity);
  setup_session.begin(identity, selected_ssid, selected_password, network_enabled, demo_network_selected);
}

void selectDemoNetwork()
{
  (void)setup_session.dispatch(SetupIntent::selectDemoNetwork());
}

int scanNetworks()
{
  (void)setup_session.dispatch(SetupIntent::scanNetworks());
  return setup_session.scannedNetworkCount();
}

int scannedNetworkCount()
{
  return setup_session.scannedNetworkCount();
}

const char *scannedSsid(int index)
{
  if (index < 0) {
    return "";
  }
  return setup_session.scannedSsid(static_cast<uint8_t>(index));
}

void selectScannedNetwork(int index)
{
  (void)setup_session.dispatch(SetupIntent::selectNetwork(index));
}

void appendPasswordChar(char value)
{
  (void)setup_session.dispatch(SetupIntent::appendPasswordChar(value));
}

void backspacePassword()
{
  (void)setup_session.dispatch(SetupIntent::backspacePassword());
}

void clearPassword()
{
  (void)setup_session.dispatch(SetupIntent::clearPassword());
}

bool connectSelected()
{
  return setup_session.dispatch(SetupIntent::connectSelected());
}

bool startPhoneSetup()
{
  SetupStatus status = setup_session.status();
  if (status.phone_setup_ready || status.state == SetupState::PhoneSetupStarting) {
    Serial.printf("NetworkService: phone setup already active name=%s pin=%s url=%s\n", status.setup_network_name, status.setup_network_password, status.setup_url);
    return true;
  }
  set_setup_url_not_connected();
  SetupIdentity identity;
  generate_phone_setup_identity(identity);
  setup_session.begin(identity, selected_ssid, selected_password, network_enabled, demo_network_selected);
  const bool accepted = setup_session.dispatch(SetupIntent::startPhoneSetup());
  Serial.printf("NetworkService: phone setup requested name=%s pin=%s transport=%s\n", identity.network_name, identity.network_password, identity.transport);
  return accepted;
}

} // namespace NetworkService
} // namespace DeskClock
