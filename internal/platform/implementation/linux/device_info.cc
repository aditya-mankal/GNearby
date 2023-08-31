#include <cstdlib>
#include <cstring>
#include <optional>
#include <pwd.h>
#include <string>
#include <sys/types.h>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/device_info.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
void CurrentUserSession::RegisterScreenLockedListener(
    absl::string_view listener_name,
    std::function<void(api::DeviceInfo::ScreenStatus)> callback) {
  absl::MutexLock l(&screen_lock_listeners_mutex_);
  screen_lock_listeners_[listener_name] = std::move(callback);
}

void CurrentUserSession::UnregisterScreenLockedListener(
    absl::string_view listener_name) {
  absl::MutexLock l(&screen_lock_listeners_mutex_);
  screen_lock_listeners_.erase(listener_name);
}

void CurrentUserSession::onLock() {
  absl::ReaderMutexLock l(&screen_lock_listeners_mutex_);
  for (auto &[_, callback] : screen_lock_listeners_) {
    callback(api::DeviceInfo::ScreenStatus::kLocked);
  }
}

void CurrentUserSession::onUnlock() {
  absl::ReaderMutexLock l(&screen_lock_listeners_mutex_);
  for (auto &[_, callback] : screen_lock_listeners_) {
    callback(api::DeviceInfo::ScreenStatus::kUnlocked);
  }
}

DeviceInfo::DeviceInfo(sdbus::IConnection &system_bus)
    : system_bus_(system_bus),
      current_user_session_(std::make_unique<CurrentUserSession>(system_bus_)),
      login_manager_(std::make_unique<LoginManager>(system_bus_)) {}

std::optional<std::u16string> DeviceInfo::GetOsDeviceName() const {
  Hostnamed hostnamed(system_bus_);
  try {
    std::string hostname = hostnamed.PrettyHostname();
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.from_bytes(hostname);
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(&hostnamed, "PrettyHostname", e);
    return std::nullopt;
  }
}

api::DeviceInfo::DeviceType DeviceInfo::GetDeviceType() const {
  Hostnamed hostnamed(system_bus_);
  try {
    std::string chasis = hostnamed.Chassis();
    api::DeviceInfo::DeviceType device = api::DeviceInfo::DeviceType::kUnknown;
    if (chasis == "phone") {
      device = api::DeviceInfo::DeviceType::kPhone;
    } else if (chasis == "laptop" || chasis == "desktop") {
      device = api::DeviceInfo::DeviceType::kLaptop;
    } else if (chasis == "tablet") {
      device = api::DeviceInfo::DeviceType::kTablet;
    } else if (chasis == "handset") {
      device = api::DeviceInfo::DeviceType::kPhone;
    }
    return device;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(&hostnamed, "Chasis", e);
    return api::DeviceInfo::DeviceType::kUnknown;
  }
}

std::optional<std::u16string> DeviceInfo::GetFullName() const {
  struct passwd *pwd = getpwuid(getuid());
  if (!pwd) {
    return std::nullopt;
  }
  char *name = strtok(pwd->pw_gecos, ",");

  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  return convert.from_bytes(name ? name : pwd->pw_gecos);
}

std::optional<std::string> DeviceInfo::GetProfileUserName() const {
  struct passwd *pwd = getpwuid(getuid());
  if (!pwd) {
    return std::nullopt;
  }
  char *name = strtok(pwd->pw_gecos, ",");
  return std::string(name);
}

std::optional<std::filesystem::path> DeviceInfo::GetDownloadPath() const {
  char *dir = getenv("XDG_DOWNLOAD_DIR");
  return std::filesystem::path(std::string(dir));
}

std::optional<std::filesystem::path> DeviceInfo::GetLocalAppDataPath() const {
  char *dir = getenv("XDG_CONFIG_HOME");
  if (dir == nullptr) {
    return std::filesystem::path("/tmp");
  }
  return std::filesystem::path(std::string(dir)) / "Google Nearby";
}

std::optional<std::filesystem::path> DeviceInfo::GetTemporaryPath() const {
  char *dir = getenv("XDG_RUNTIME_PATH");
  if (dir == nullptr) {
    return std::filesystem::path("/tmp");
  }
  return std::filesystem::path(std::string(dir)) / "Google Nearby";
}

std::optional<std::filesystem::path> DeviceInfo::GetLogPath() const {
  char *dir = getenv("XDG_STATE_HOME");
  if (dir == nullptr) {
    return std::filesystem::path("/tmp");
  }
  return std::filesystem::path(std::string(dir)) / "Google Nearby" / "logs";
}

std::optional<std::filesystem::path> DeviceInfo::GetCrashDumpPath() const {
  char *dir = getenv("XDG_STATE_HOME");
  if (dir == nullptr) {
    return std::filesystem::path("/tmp");
  }
  return std::filesystem::path(std::string(dir)) / "Google Nearby" / "crashes";
}

bool DeviceInfo::IsScreenLocked() const {
  try {
    return current_user_session_->LockedHint();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(current_user_session_, "LockedHint", e);
    return false;
  }
}

bool DeviceInfo::PreventSleep() {
  try {
    inhibit_fd_ = login_manager_->Inhibit("sleep", "Google Nearby",
                                          "Google Nearby", "block");
    return true;
  } catch (const sdbus::Error& e) {
    DBUS_LOG_METHOD_CALL_ERROR(login_manager_, "Inhibit", e);
    return false;
  }
}

bool DeviceInfo::AllowSleep() {
  if (!inhibit_fd_.has_value()) {
    NEARBY_LOGS(ERROR) << __func__
                       << "No inhibit lock is acquired at the moment";
    return false;
  }

  inhibit_fd_.reset();
  return true;
}

} // namespace linux
} // namespace nearby
