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

} // namespace NetworkService
} // namespace DeskClock
