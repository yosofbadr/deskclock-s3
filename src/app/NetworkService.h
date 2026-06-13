#pragma once

namespace DeskClock {

struct NetworkSnapshot {
  bool enabled = false;
  bool connected = false;
  const char *status = "offline";
  const char *ssid = "not selected";
  const char *password_preview = "";
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

} // namespace NetworkService
} // namespace DeskClock
