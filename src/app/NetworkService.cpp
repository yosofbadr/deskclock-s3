#include "NetworkService.h"

#include <Arduino.h>
#include <Preferences.h>

namespace DeskClock {
namespace {

constexpr const char *kPreferencesNamespace = "deskclock";
constexpr const char *kEnabledKey = "wifi_en";
constexpr const char *kDemoSelectedKey = "wifi_demo";

bool network_enabled = false;
bool demo_network_selected = false;

void save()
{
  Preferences preferences;
  if (!preferences.begin(kPreferencesNamespace, false)) {
    Serial.println("NetworkService: failed to open preferences for write");
    return;
  }
  preferences.putBool(kEnabledKey, network_enabled);
  preferences.putBool(kDemoSelectedKey, demo_network_selected);
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
    preferences.end();
  }
}

void loop()
{
  // Placeholder for future Wi-Fi scan/connect/NTP sync work. Core clock/alarm remains local.
}

NetworkSnapshot snapshot()
{
  NetworkSnapshot result;
  result.enabled = network_enabled;
  result.connected = false;
  result.ssid = demo_network_selected ? "Demo network" : "not selected";
  if (!network_enabled) {
    result.status = "Wi-Fi skipped";
  } else if (demo_network_selected) {
    result.status = "saved; not connected";
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
  network_enabled = true;
  save();
}

} // namespace NetworkService
} // namespace DeskClock
