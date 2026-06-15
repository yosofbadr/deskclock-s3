#include "app/SetupSession.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

using namespace DeskClock;

namespace {

struct FakeClockCore {
  int ticks = 0;
  void tick() { ticks++; }
};

class FakeSetupAdapters : public SetupSessionAdapters {
public:
  bool phone_setup_started = false;
  bool portal_started = false;
  bool save_called = false;
  bool start_connection_called = false;
  int connection_start_count = 0;
  bool connection_should_start = false;
  bool connected = false;
  bool failed = false;
  bool wifi_scan_called = false;
  bool observed_saving_status = false;
  SetupSession *observed_session = nullptr;
  char saved_ssid[kSetupSsidBufferLength] = "";
  char saved_password[kSetupPasswordBufferLength] = "";

  bool startPhoneSetup(SetupIdentity &identity) override
  {
    phone_setup_started = true;
    std::strncpy(identity.url, "http://192.168.4.1/", sizeof(identity.url) - 1);
    identity.url[sizeof(identity.url) - 1] = '\0';
    return true;
  }

  bool startPortal(const SetupIdentity &) override
  {
    portal_started = true;
    return true;
  }

  int scanNetworks(SetupNetworkRecord *records, int max_records) override
  {
    wifi_scan_called = true;
    const char *names[] = {"DeskClock Lab", "Home Wi-Fi", "Guest"};
    const int count = max_records < 3 ? max_records : 3;
    for (int index = 0; index < count; ++index) {
      std::strncpy(records[index].ssid, names[index], sizeof(records[index].ssid) - 1);
      records[index].ssid[sizeof(records[index].ssid) - 1] = '\0';
    }
    return count;
  }

  bool saveCredentials(const char *ssid, const char *password, bool, bool) override
  {
    save_called = true;
    if (observed_session != nullptr) {
      const SetupStatus status = observed_session->status();
      observed_saving_status = status.state == SetupState::SavingCredentials && std::string(status.status_text) == "saving credentials";
    }
    std::strncpy(saved_ssid, ssid == nullptr ? "" : ssid, sizeof(saved_ssid) - 1);
    saved_ssid[sizeof(saved_ssid) - 1] = '\0';
    std::strncpy(saved_password, password == nullptr ? "" : password, sizeof(saved_password) - 1);
    saved_password[sizeof(saved_password) - 1] = '\0';
    return true;
  }

  bool startConnection(const char *, const char *) override
  {
    start_connection_called = true;
    connection_start_count++;
    return connection_should_start;
  }

  bool isConnected() override { return connected; }
  bool connectionFailed() override { return failed; }
};

void require(bool condition, const char *message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    std::exit(1);
  }
}

SetupIdentity identity()
{
  SetupIdentity result = {};
  std::strncpy(result.network_name, "PROV_DC1234", sizeof(result.network_name) - 1);
  std::strncpy(result.network_password, "DC123456", sizeof(result.network_password) - 1);
  std::strncpy(result.transport, "SoftAP", sizeof(result.transport) - 1);
  std::strncpy(result.url, "not connected", sizeof(result.url) - 1);
  return result;
}

void starting_phone_setup_reports_ready_without_stopping_clock_core()
{
  FakeSetupAdapters adapters;
  SetupSession session(adapters);
  session.begin(identity(), "", "", false, false);
  FakeClockCore clock;

  require(session.dispatch(SetupIntent::startPhoneSetup()), "start phone setup intent accepted");
  clock.tick();
  session.loop();
  clock.tick();

  const SetupStatus status = session.status();
  require(adapters.phone_setup_started, "temporary setup network was started");
  require(adapters.portal_started, "temporary setup portal was started");
  require(status.state == SetupState::PhoneSetupReady, "status reached Phone setup ready state");
  require(std::string(status.status_text) == "Phone setup ready", "ready status text is user-visible");
  require(std::string(status.setup_network_name) == "PROV_DC1234", "status includes setup network name");
  require(std::string(status.setup_network_password) == "DC123456", "status includes setup network password");
  require(std::string(status.setup_url) == "http://192.168.4.1/", "status includes setup URL");
  require(clock.ticks == 2, "clock core continued ticking around setup start");
}

void credential_submission_saves_before_connecting_and_survives_failure()
{
  FakeSetupAdapters adapters;
  SetupSession session(adapters);
  session.begin(identity(), "", "", false, false);
  adapters.observed_session = &session;
  FakeClockCore clock;

  require(session.dispatch(SetupIntent::submitCredentials("Home Wi-Fi", "bad-password")), "credential submission accepted");
  SetupStatus saved = session.status();
  require(adapters.save_called, "credentials were durably saved by the setup session");
  require(adapters.observed_saving_status, "unified setup status exposes saving state while credentials are being written");
  require(!adapters.start_connection_called, "connection did not start inside the save definition");
  require(std::string(adapters.saved_ssid) == "Home Wi-Fi", "saved SSID recorded");
  require(std::string(adapters.saved_password) == "bad-password", "saved password recorded");
  require(saved.credentials_saved, "status reports credentials saved before connection");
  require(saved.state == SetupState::CredentialsSaved, "status separates credentials saved from connected");

  clock.tick();
  session.loop();
  clock.tick();
  SetupStatus failed = session.status();
  require(adapters.start_connection_called, "connection starts as a later setup transition");
  require(failed.state == SetupState::ConnectionFailed, "failed connection reaches user-visible failure state");
  require(failed.credentials_saved, "saved credentials remain available after failure");
  require(failed.retry_available, "retry is available from failed state");
  require(clock.ticks == 2, "clock core continued ticking while connection failed");
}

void failed_credentials_can_be_edited_and_retried_without_erasing_them()
{
  FakeSetupAdapters adapters;
  SetupSession session(adapters);
  session.begin(identity(), "", "", false, false);

  require(session.dispatch(SetupIntent::submitCredentials("Home Wi-Fi", "bad-password")), "credential submission accepted");
  session.loop();
  require(session.status().state == SetupState::ConnectionFailed, "first connection fails");

  require(session.dispatch(SetupIntent::editCredentials()), "edit credentials intent accepted");
  SetupStatus editing = session.status();
  require(editing.state == SetupState::EditingCredentials, "editing credentials state shown");
  require(editing.credentials_saved, "editing preserves saved credentials");
  require(std::string(session.selectedSsid()) == "Home Wi-Fi", "SSID preserved for edit");
  require(std::string(session.selectedPassword()) == "bad-password", "password preserved for edit");

  adapters.connection_should_start = true;
  FakeClockCore clock;
  require(session.dispatch(SetupIntent::retryConnection()), "retry intent accepted from failure/edit flow");
  require(session.status().credentials_saved, "retry preserves saved credentials before reconnect");
  clock.tick();
  session.loop();
  require(adapters.connection_start_count == 2, "retry starts a second connection attempt");
  require(session.status().state == SetupState::Connecting, "retry reports connection progress");
  adapters.connected = true;
  session.loop();
  clock.tick();
  SetupStatus connected = session.status();
  require(connected.state == SetupState::Connected, "retry can reach connected state");
  require(connected.credentials_saved, "credentials remain saved after successful retry");
  require(clock.ticks == 2, "clock core continued ticking around retry flow");
}

void network_selection_requests_scan_select_and_connect_through_intents()
{
  FakeSetupAdapters adapters;
  SetupSession session(adapters);
  session.begin(identity(), "", "", false, false);
  FakeClockCore clock;

  require(session.dispatch(SetupIntent::scanNetworks()), "scan intent accepted");
  require(!adapters.wifi_scan_called, "scan request did not invoke Wi-Fi inline");
  require(session.status().state == SetupState::Scanning, "status reports scanning after scan intent");
  clock.tick();
  session.loop();
  clock.tick();

  require(adapters.wifi_scan_called, "setup session performed Wi-Fi scan in its loop");
  require(session.scannedNetworkCount() == 3, "scan results are captured in setup session");
  require(std::string(session.scannedSsid(1)) == "Home Wi-Fi", "discovered network is visible");
  require(session.status().state == SetupState::ScanComplete, "status reports scan complete");

  require(session.dispatch(SetupIntent::selectNetwork(1)), "select network intent accepted");
  SetupStatus selected = session.status();
  require(std::string(selected.selected_network) == "Home Wi-Fi", "selected network is reported in setup status");
  require(std::string(adapters.saved_ssid) == "Home Wi-Fi", "selected network is persisted through setup session");
  require(!adapters.start_connection_called, "select network does not connect inline");

  require(session.dispatch(SetupIntent::connectSelected()), "connect selected intent accepted");
  require(!adapters.start_connection_called, "connect request is deferred until setup session loop");
  session.loop();
  require(adapters.start_connection_called, "setup session owns connect selected work");
  require(clock.ticks == 2, "clock core kept ticking around scan transition");
}

} // namespace

int main()
{
  starting_phone_setup_reports_ready_without_stopping_clock_core();
  credential_submission_saves_before_connecting_and_survives_failure();
  failed_credentials_can_be_edited_and_retried_without_erasing_them();
  network_selection_requests_scan_select_and_connect_through_intents();
  std::cout << "SetupSession harness tests passed\n";
  return 0;
}
