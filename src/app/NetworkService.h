#pragma once

namespace DeskClock {

struct NetworkSnapshot {
  bool enabled = false;
  bool connected = false;
  bool phone_setup_active = false;
  const char *status = "offline";
  const char *ssid = "not selected";
  const char *password_preview = "";
  const char *phone_setup_name = "DeskClock";
  const char *phone_setup_pin = "DC000000";
  const char *phone_setup_transport = "SoftAP";
  const char *setup_url = "not connected";
};

namespace NetworkService {

void begin();
void loop();
NetworkSnapshot snapshot();
void setEnabled(bool enabled);
void selectDemoNetwork();
int scanNetworks();
int scannedNetworkCount();
const char *scannedSsid(int index);
void selectScannedNetwork(int index);
void appendPasswordChar(char value);
void backspacePassword();
void clearPassword();
bool connectSelected();
bool startPhoneSetup();

} // namespace NetworkService
} // namespace DeskClock
