#pragma once

namespace DeskClock {

struct NetworkSnapshot {
  bool enabled = false;
  bool connected = false;
  const char *status = "offline";
  const char *ssid = "not selected";
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

} // namespace NetworkService
} // namespace DeskClock
