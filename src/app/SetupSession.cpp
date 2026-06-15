#include "SetupSession.h"

#include <string.h>

namespace DeskClock {
namespace {

void bounded_copy(char *destination, size_t destination_size, const char *source)
{
  if (destination == nullptr || destination_size == 0) {
    return;
  }
  if (source == nullptr) {
    destination[0] = '\0';
    return;
  }
  size_t index = 0;
  for (; index + 1 < destination_size && source[index] != '\0'; ++index) {
    destination[index] = source[index];
  }
  destination[index] = '\0';
}

bool text_present(const char *value)
{
  return value != nullptr && value[0] != '\0';
}

void clear_text(char *destination, size_t destination_size)
{
  if (destination != nullptr && destination_size > 0) {
    destination[0] = '\0';
  }
}

const char *state_text(SetupState state,
                       bool enabled,
                       bool demo_selected,
                       bool credentials_saved,
                       bool has_selected_network,
                       bool has_password,
                       bool connected)
{
  switch (state) {
  case SetupState::PhoneSetupStarting:
    return "Phone setup starting";
  case SetupState::PhoneSetupReady:
    return "Phone setup ready";
  case SetupState::PhoneSetupFailed:
    return "Phone setup failed";
  case SetupState::Scanning:
    return "scanning networks";
  case SetupState::ScanComplete:
    return "scan complete";
  case SetupState::NetworkSelected:
    return has_password ? "credentials saved; not connected" : "network selected; password needed";
  case SetupState::SavingCredentials:
    return "saving credentials";
  case SetupState::CredentialsSaved:
    return "credentials saved; connecting...";
  case SetupState::Connecting:
    return "connecting...";
  case SetupState::Connected:
    return "connected; setup web ready";
  case SetupState::ConnectionFailed:
    return "connection failed; retry or edit";
  case SetupState::EditingCredentials:
    return "edit credentials";
  case SetupState::Disabled:
    return "Wi-Fi skipped";
  case SetupState::DemoSelected:
    return "demo selected; not connected";
  case SetupState::Idle:
  default:
    if (connected) {
      return "connected; setup web ready";
    }
    if (!enabled) {
      return "Wi-Fi skipped";
    }
    if (credentials_saved || (has_selected_network && has_password)) {
      return "credentials saved; not connected";
    }
    if (has_selected_network) {
      return "network selected; password needed";
    }
    if (demo_selected) {
      return "demo selected; not connected";
    }
    return "select network";
  }
}

} // namespace

SetupIntent SetupIntent::startPhoneSetup()
{
  SetupIntent intent;
  intent.type = SetupIntentType::StartPhoneSetup;
  return intent;
}

SetupIntent SetupIntent::submitCredentials(const char *ssid, const char *password)
{
  SetupIntent intent;
  intent.type = SetupIntentType::SubmitCredentials;
  bounded_copy(intent.ssid, sizeof(intent.ssid), ssid);
  bounded_copy(intent.password, sizeof(intent.password), password);
  return intent;
}

SetupIntent SetupIntent::scanNetworks()
{
  SetupIntent intent;
  intent.type = SetupIntentType::ScanNetworks;
  return intent;
}

SetupIntent SetupIntent::selectNetwork(int index)
{
  SetupIntent intent;
  intent.type = SetupIntentType::SelectNetwork;
  intent.index = index;
  return intent;
}

SetupIntent SetupIntent::connectSelected()
{
  SetupIntent intent;
  intent.type = SetupIntentType::ConnectSelected;
  return intent;
}

SetupIntent SetupIntent::retryConnection()
{
  SetupIntent intent;
  intent.type = SetupIntentType::RetryConnection;
  return intent;
}

SetupIntent SetupIntent::editCredentials()
{
  SetupIntent intent;
  intent.type = SetupIntentType::EditCredentials;
  return intent;
}

SetupIntent SetupIntent::disableWifi()
{
  SetupIntent intent;
  intent.type = SetupIntentType::DisableWifi;
  return intent;
}

SetupIntent SetupIntent::selectDemoNetwork()
{
  SetupIntent intent;
  intent.type = SetupIntentType::SelectDemoNetwork;
  return intent;
}

SetupIntent SetupIntent::appendPasswordChar(char value)
{
  SetupIntent intent;
  intent.type = SetupIntentType::AppendPasswordChar;
  intent.character = value;
  return intent;
}

SetupIntent SetupIntent::backspacePassword()
{
  SetupIntent intent;
  intent.type = SetupIntentType::BackspacePassword;
  return intent;
}

SetupIntent SetupIntent::clearPassword()
{
  SetupIntent intent;
  intent.type = SetupIntentType::ClearPassword;
  return intent;
}

SetupIntent SetupIntent::replacePassword(const char *password)
{
  SetupIntent intent;
  intent.type = SetupIntentType::ReplacePassword;
  bounded_copy(intent.password, sizeof(intent.password), password);
  return intent;
}

int SetupSessionAdapters::scanNetworks(SetupNetworkRecord *, int)
{
  return 0;
}

bool SetupSessionAdapters::saveCredentials(const char *, const char *, bool, bool)
{
  return true;
}

bool SetupSessionAdapters::setEnabled(bool)
{
  return true;
}

bool SetupSessionAdapters::saveDemoNetwork()
{
  return true;
}

bool SetupSessionAdapters::startConnection(const char *, const char *)
{
  return false;
}

bool SetupSessionAdapters::isConnected()
{
  return false;
}

bool SetupSessionAdapters::connectionFailed()
{
  return false;
}

void SetupSessionAdapters::updateSetupUrl(char *, size_t)
{
}

SetupSession::SetupSession(SetupSessionAdapters &adapters) : adapters_(adapters) {}

void SetupSession::begin(const SetupIdentity &identity, const char *saved_ssid, const char *saved_password, bool enabled, bool demo_selected)
{
  identity_ = identity;
  bounded_copy(selected_ssid_, sizeof(selected_ssid_), saved_ssid);
  bounded_copy(selected_password_, sizeof(selected_password_), saved_password);
  enabled_ = enabled;
  demo_selected_ = demo_selected;
  credentials_saved_ = text_present(selected_ssid_) && text_present(selected_password_);
  connection_start_pending_ = false;
  start_phone_setup_pending_ = false;
  scan_pending_ = false;
  scanned_network_count_ = 0;
  updatePasswordPreview();
  setStateFromIdleInputs();
}

bool SetupSession::dispatch(const SetupIntent &intent)
{
  switch (intent.type) {
  case SetupIntentType::StartPhoneSetup:
    start_phone_setup_pending_ = true;
    state_ = SetupState::PhoneSetupStarting;
    return true;
  case SetupIntentType::SubmitCredentials:
    state_ = SetupState::SavingCredentials;
    bounded_copy(selected_ssid_, sizeof(selected_ssid_), intent.ssid);
    bounded_copy(selected_password_, sizeof(selected_password_), intent.password);
    demo_selected_ = false;
    enabled_ = true;
    updatePasswordPreview();
    if (!saveCurrentCredentials()) {
      state_ = SetupState::ConnectionFailed;
      return false;
    }
    state_ = SetupState::CredentialsSaved;
    connection_start_pending_ = true;
    return true;
  case SetupIntentType::ScanNetworks:
    scan_pending_ = true;
    state_ = SetupState::Scanning;
    return true;
  case SetupIntentType::SelectNetwork:
    if (intent.index < 0 || intent.index >= scanned_network_count_) {
      return false;
    }
    bounded_copy(selected_ssid_, sizeof(selected_ssid_), scanned_networks_[intent.index].ssid);
    clear_text(selected_password_, sizeof(selected_password_));
    demo_selected_ = false;
    enabled_ = true;
    credentials_saved_ = false;
    updatePasswordPreview();
    (void)adapters_.saveCredentials(selected_ssid_, selected_password_, enabled_, demo_selected_);
    state_ = SetupState::NetworkSelected;
    return true;
  case SetupIntentType::ConnectSelected:
    if (!hasSelectedNetwork()) {
      return false;
    }
    startConnectionLater();
    return true;
  case SetupIntentType::RetryConnection:
    if (!hasSavedCredentials()) {
      return false;
    }
    startConnectionLater();
    return true;
  case SetupIntentType::EditCredentials:
    state_ = SetupState::EditingCredentials;
    return true;
  case SetupIntentType::DisableWifi:
    enabled_ = false;
    demo_selected_ = false;
    (void)adapters_.setEnabled(false);
    state_ = SetupState::Disabled;
    return true;
  case SetupIntentType::SelectDemoNetwork:
    clear_text(selected_ssid_, sizeof(selected_ssid_));
    clear_text(selected_password_, sizeof(selected_password_));
    enabled_ = true;
    demo_selected_ = true;
    credentials_saved_ = false;
    updatePasswordPreview();
    (void)adapters_.saveDemoNetwork();
    state_ = SetupState::DemoSelected;
    return true;
  case SetupIntentType::AppendPasswordChar: {
    const size_t length = strlen(selected_password_);
    if (length + 1 >= sizeof(selected_password_)) {
      return false;
    }
    selected_password_[length] = intent.character;
    selected_password_[length + 1] = '\0';
    updatePasswordPreview();
    credentials_saved_ = saveCurrentCredentials();
    state_ = credentials_saved_ ? SetupState::CredentialsSaved : SetupState::NetworkSelected;
    return credentials_saved_;
  }
  case SetupIntentType::BackspacePassword: {
    const size_t length = strlen(selected_password_);
    if (length > 0) {
      selected_password_[length - 1] = '\0';
    }
    updatePasswordPreview();
    credentials_saved_ = saveCurrentCredentials() && text_present(selected_password_);
    state_ = credentials_saved_ ? SetupState::CredentialsSaved : SetupState::NetworkSelected;
    return true;
  }
  case SetupIntentType::ClearPassword:
    clear_text(selected_password_, sizeof(selected_password_));
    updatePasswordPreview();
    credentials_saved_ = false;
    (void)adapters_.saveCredentials(selected_ssid_, selected_password_, enabled_, demo_selected_);
    state_ = SetupState::NetworkSelected;
    return true;
  case SetupIntentType::ReplacePassword:
    bounded_copy(selected_password_, sizeof(selected_password_), intent.password);
    updatePasswordPreview();
    credentials_saved_ = saveCurrentCredentials();
    state_ = credentials_saved_ ? SetupState::CredentialsSaved : SetupState::NetworkSelected;
    return credentials_saved_;
  }
  return false;
}

void SetupSession::loop()
{
  if (start_phone_setup_pending_) {
    start_phone_setup_pending_ = false;
    const bool transport_ok = adapters_.startPhoneSetup(identity_);
    const bool portal_ok = transport_ok && adapters_.startPortal(identity_);
    state_ = portal_ok ? SetupState::PhoneSetupReady : SetupState::PhoneSetupFailed;
  }

  if (scan_pending_) {
    scan_pending_ = false;
    const int found = adapters_.scanNetworks(scanned_networks_, kSetupMaxScannedNetworks);
    scanned_network_count_ = found < 0 ? 0 : static_cast<uint8_t>(found > kSetupMaxScannedNetworks ? kSetupMaxScannedNetworks : found);
    state_ = SetupState::ScanComplete;
  }

  if (connection_start_pending_) {
    connection_start_pending_ = false;
    if (!hasSelectedNetwork()) {
      state_ = SetupState::NetworkSelected;
    } else if (adapters_.startConnection(selected_ssid_, selected_password_)) {
      state_ = SetupState::Connecting;
    } else {
      state_ = SetupState::ConnectionFailed;
    }
  }

  if (state_ == SetupState::Connecting) {
    if (adapters_.isConnected()) {
      adapters_.updateSetupUrl(identity_.url, sizeof(identity_.url));
      state_ = SetupState::Connected;
    } else if (adapters_.connectionFailed()) {
      state_ = SetupState::ConnectionFailed;
    }
  } else if (state_ == SetupState::Connected) {
    adapters_.updateSetupUrl(identity_.url, sizeof(identity_.url));
  }
}

SetupStatus SetupSession::status() const
{
  SetupStatus result;
  result.state = state_;
  result.enabled = enabled_;
  result.connected = state_ == SetupState::Connected;
  result.phone_setup_ready = state_ == SetupState::PhoneSetupReady;
  result.credentials_saved = hasSavedCredentials();
  result.retry_available = state_ == SetupState::ConnectionFailed && hasSavedCredentials();
  result.scanned_network_count = scanned_network_count_;
  bounded_copy(result.status_text,
               sizeof(result.status_text),
               state_text(state_, enabled_, demo_selected_, credentials_saved_, hasSelectedNetwork(), text_present(selected_password_), result.connected));
  if (hasSelectedNetwork()) {
    bounded_copy(result.selected_network, sizeof(result.selected_network), selected_ssid_);
  } else if (demo_selected_) {
    bounded_copy(result.selected_network, sizeof(result.selected_network), "Demo network");
  } else {
    bounded_copy(result.selected_network, sizeof(result.selected_network), "not selected");
  }
  bounded_copy(result.password_preview, sizeof(result.password_preview), password_preview_);
  copyIdentityToStatus(result);
  return result;
}

uint8_t SetupSession::scannedNetworkCount() const
{
  return scanned_network_count_;
}

const char *SetupSession::scannedSsid(uint8_t index) const
{
  if (index >= scanned_network_count_) {
    return "";
  }
  return scanned_networks_[index].ssid;
}

const char *SetupSession::selectedSsid() const
{
  return selected_ssid_;
}

const char *SetupSession::selectedPassword() const
{
  return selected_password_;
}

bool SetupSession::enabled() const
{
  return enabled_;
}

bool SetupSession::demoSelected() const
{
  return demo_selected_;
}

void SetupSession::copyIdentityToStatus(SetupStatus &result) const
{
  bounded_copy(result.setup_network_name, sizeof(result.setup_network_name), identity_.network_name);
  bounded_copy(result.setup_network_password, sizeof(result.setup_network_password), identity_.network_password);
  bounded_copy(result.setup_transport, sizeof(result.setup_transport), identity_.transport);
  bounded_copy(result.setup_url, sizeof(result.setup_url), identity_.url);
}

void SetupSession::updatePasswordPreview()
{
  const size_t length = strlen(selected_password_);
  const size_t visible = length < 2 ? length : 2;
  size_t index = 0;
  for (; index < length && index + 1 < sizeof(password_preview_); ++index) {
    password_preview_[index] = index < visible ? selected_password_[index] : '*';
  }
  password_preview_[index] = '\0';
}

bool SetupSession::hasSelectedNetwork() const
{
  return text_present(selected_ssid_);
}

bool SetupSession::hasSavedCredentials() const
{
  return credentials_saved_ && hasSelectedNetwork() && text_present(selected_password_);
}

bool SetupSession::saveCurrentCredentials()
{
  enabled_ = hasSelectedNetwork();
  demo_selected_ = false;
  const bool saved = adapters_.saveCredentials(selected_ssid_, selected_password_, enabled_, demo_selected_);
  credentials_saved_ = saved && hasSelectedNetwork() && text_present(selected_password_);
  return saved;
}

void SetupSession::setStateFromIdleInputs()
{
  if (!enabled_) {
    state_ = SetupState::Disabled;
  } else if (demo_selected_) {
    state_ = SetupState::DemoSelected;
  } else if (credentials_saved_) {
    state_ = SetupState::Idle;
  } else if (hasSelectedNetwork()) {
    state_ = SetupState::NetworkSelected;
  } else {
    state_ = SetupState::Idle;
  }
}

void SetupSession::startConnectionLater()
{
  enabled_ = true;
  connection_start_pending_ = true;
  state_ = SetupState::Connecting;
}

} // namespace DeskClock
