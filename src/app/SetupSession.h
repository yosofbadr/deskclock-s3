#pragma once

#include <stddef.h>
#include <stdint.h>

namespace DeskClock {

constexpr size_t kSetupSsidBufferLength = 33;
constexpr size_t kSetupPasswordBufferLength = 65;
constexpr size_t kSetupNameBufferLength = 32;
constexpr size_t kSetupPinBufferLength = 12;
constexpr size_t kSetupTransportBufferLength = 12;
constexpr size_t kSetupUrlBufferLength = 48;
constexpr uint8_t kSetupMaxScannedNetworks = 5;

struct SetupNetworkRecord {
  char ssid[kSetupSsidBufferLength] = "";
};

struct SetupIdentity {
  char network_name[kSetupNameBufferLength] = "DeskClock";
  char network_password[kSetupPinBufferLength] = "DC000000";
  char transport[kSetupTransportBufferLength] = "SoftAP";
  char url[kSetupUrlBufferLength] = "not connected";
};

enum class SetupState : uint8_t {
  Idle,
  PhoneSetupStarting,
  PhoneSetupReady,
  PhoneSetupFailed,
  Scanning,
  ScanComplete,
  NetworkSelected,
  SavingCredentials,
  CredentialsSaved,
  Connecting,
  Connected,
  ConnectionFailed,
  EditingCredentials,
  Disabled,
  DemoSelected,
};

struct SetupStatus {
  SetupState state = SetupState::Idle;
  bool enabled = false;
  bool connected = false;
  bool phone_setup_ready = false;
  bool credentials_saved = false;
  bool retry_available = false;
  uint8_t scanned_network_count = 0;
  char status_text[72] = "select network";
  char selected_network[kSetupSsidBufferLength] = "not selected";
  char password_preview[kSetupPasswordBufferLength] = "";
  char setup_network_name[kSetupNameBufferLength] = "DeskClock";
  char setup_network_password[kSetupPinBufferLength] = "DC000000";
  char setup_transport[kSetupTransportBufferLength] = "SoftAP";
  char setup_url[kSetupUrlBufferLength] = "not connected";
};

enum class SetupIntentType : uint8_t {
  StartPhoneSetup,
  SubmitCredentials,
  ScanNetworks,
  SelectNetwork,
  ConnectSelected,
  RetryConnection,
  EditCredentials,
  DisableWifi,
  SelectDemoNetwork,
  AppendPasswordChar,
  BackspacePassword,
  ClearPassword,
  ReplacePassword,
};

struct SetupIntent {
  SetupIntentType type = SetupIntentType::StartPhoneSetup;
  int index = 0;
  char ssid[kSetupSsidBufferLength] = "";
  char password[kSetupPasswordBufferLength] = "";
  char character = '\0';

  static SetupIntent startPhoneSetup();
  static SetupIntent submitCredentials(const char *ssid, const char *password);
  static SetupIntent scanNetworks();
  static SetupIntent selectNetwork(int index);
  static SetupIntent connectSelected();
  static SetupIntent retryConnection();
  static SetupIntent editCredentials();
  static SetupIntent disableWifi();
  static SetupIntent selectDemoNetwork();
  static SetupIntent appendPasswordChar(char value);
  static SetupIntent backspacePassword();
  static SetupIntent clearPassword();
  static SetupIntent replacePassword(const char *password);
};

class SetupSessionAdapters {
public:
  virtual ~SetupSessionAdapters() = default;
  virtual bool startPhoneSetup(SetupIdentity &identity) = 0;
  virtual bool startPortal(const SetupIdentity &identity) = 0;
  virtual int scanNetworks(SetupNetworkRecord *records, int max_records);
  virtual bool saveCredentials(const char *ssid, const char *password, bool enabled, bool demo_selected);
  virtual bool setEnabled(bool enabled);
  virtual bool saveDemoNetwork();
  virtual bool startConnection(const char *ssid, const char *password);
  virtual bool isConnected();
  virtual bool connectionFailed();
  virtual void updateSetupUrl(char *url, size_t url_size);
};

class SetupSession {
public:
  explicit SetupSession(SetupSessionAdapters &adapters);

  void begin(const SetupIdentity &identity, const char *saved_ssid, const char *saved_password, bool enabled, bool demo_selected);
  bool dispatch(const SetupIntent &intent);
  void loop();

  SetupStatus status() const;
  uint8_t scannedNetworkCount() const;
  const char *scannedSsid(uint8_t index) const;
  const char *selectedSsid() const;
  const char *selectedPassword() const;
  bool enabled() const;
  bool demoSelected() const;

private:
  SetupSessionAdapters &adapters_;
  SetupIdentity identity_;
  SetupState state_ = SetupState::Idle;
  bool enabled_ = false;
  bool demo_selected_ = false;
  bool credentials_saved_ = false;
  bool connection_start_pending_ = false;
  bool start_phone_setup_pending_ = false;
  bool scan_pending_ = false;
  char selected_ssid_[kSetupSsidBufferLength] = "";
  char selected_password_[kSetupPasswordBufferLength] = "";
  char password_preview_[kSetupPasswordBufferLength] = "";
  SetupNetworkRecord scanned_networks_[kSetupMaxScannedNetworks] = {};
  uint8_t scanned_network_count_ = 0;

  void copyIdentityToStatus(SetupStatus &result) const;
  void updatePasswordPreview();
  bool hasSelectedNetwork() const;
  bool hasSavedCredentials() const;
  bool saveCurrentCredentials();
  void setStateFromIdleInputs();
  void startConnectionLater();
};

} // namespace DeskClock
