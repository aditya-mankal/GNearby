// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_CLASSIC_DEVICE_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_CLASSIC_DEVICE_H_

#include <atomic>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/StandardInterfaces.h>
#include <sdbus-c++/Types.h>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/base/observer_list.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/device_client.h"

namespace nearby {
#pragma push_macro("linux")
#undef linux
namespace linux {
// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.
class BluetoothDevice
    : public api::BluetoothDevice,
      public sdbus::ProxyInterfaces<org::bluez::Device1_proxy>,
      public api::ble_v2::BlePeripheral {
 public:
  using UniqueId = std::uint64_t;

  BluetoothDevice(const BluetoothDevice &) = delete;
  BluetoothDevice(BluetoothDevice &&) = delete;
  BluetoothDevice &operator=(const BluetoothDevice &) = delete;
  BluetoothDevice &operator=(BluetoothDevice &&) = delete;
  BluetoothDevice(sdbus::IConnection &system_bus,
                  sdbus::ObjectPath device_object_path);
  ~BluetoothDevice() override { unregisterProxy(); }

  // BluetoothDevice methods
  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
  std::string GetName() const override;
  // Returns BT MAC address assigned to this device.
  std::string GetMacAddress() const override;

  // BlePeripheral methods
  std::string GetAddress() const override { return GetMacAddress(); }
  UniqueId GetUniqueId() const override { return unique_id_; };

  void set_pair_reply_callback(
      absl::AnyInvocable<void(const sdbus::Error *)> cb)
      ABSL_LOCKS_EXCLUDED(pair_callback_lock_) {
    absl::MutexLock l(&pair_callback_lock_);
    on_pair_reply_cb_ = std::move(cb);
  }

  void reset_pair_reply_callback() ABSL_LOCKS_EXCLUDED(pair_callback_lock_) {
    absl::MutexLock l(&pair_callback_lock_);
    on_pair_reply_cb_ = nullptr;
  }

  bool ConnectToProfile(absl::string_view service_uuid);
  void MarkLost() { lost_ = true; }
  void UnmarkLost() { lost_ = false; }
  bool Lost() const { return lost_; }

 protected:
  void onPairReply(const sdbus::Error *error) override
      ABSL_LOCKS_EXCLUDED(pair_callback_lock_) {
    absl::ReaderMutexLock l(&pair_callback_lock_);
    if (on_pair_reply_cb_ != nullptr) on_pair_reply_cb_(error);
  };

 private:
  absl::Mutex pair_callback_lock_;
  absl::AnyInvocable<void(const sdbus::Error *)> on_pair_reply_cb_
      ABSL_GUARDED_BY(pair_callback_lock_) = nullptr;
  UniqueId unique_id_;
  std::atomic_bool lost_;

  mutable absl::Mutex properties_mutex_;
  mutable std::string last_known_name_ ABSL_GUARDED_BY(properties_mutex_);
  mutable std::string last_known_address_ ABSL_GUARDED_BY(properties_mutex_);
};

class MonitoredBluetoothDevice final
    : public BluetoothDevice,
      public sdbus::ProxyInterfaces<sdbus::Properties_proxy> {
 public:
  using sdbus::ProxyInterfaces<sdbus::Properties_proxy>::registerProxy;
  using sdbus::ProxyInterfaces<sdbus::Properties_proxy>::unregisterProxy;
  using sdbus::ProxyInterfaces<sdbus::Properties_proxy>::getObjectPath;

  MonitoredBluetoothDevice(const MonitoredBluetoothDevice &) = delete;
  MonitoredBluetoothDevice(MonitoredBluetoothDevice &&) = delete;
  MonitoredBluetoothDevice &operator=(const MonitoredBluetoothDevice &) =
      delete;
  MonitoredBluetoothDevice &operator=(MonitoredBluetoothDevice &&) = delete;
  MonitoredBluetoothDevice(
      sdbus::IConnection &system_bus, const sdbus::ObjectPath &,
      ObserverList<api::BluetoothClassicMedium::Observer> &observers);
  ~MonitoredBluetoothDevice() override { unregisterProxy(); }

  void SetDiscoveryCallback(
      std::shared_ptr<api::BluetoothClassicMedium::DiscoveryCallback> &callback)
      ABSL_LOCKS_EXCLUDED(discovery_cb_mutex_) {
    absl::MutexLock lock(&discovery_cb_mutex_);
    discovery_cb_ = callback;
  };

 protected:
  void onPropertiesChanged(
      const std::string &interfaceName,
      const std::map<std::string, sdbus::Variant> &changedProperties,
      const std::vector<std::string> &invalidatedProperties) override;

 private:
  std::shared_ptr<api::BluetoothClassicMedium::DiscoveryCallback>
  GetDiscoveryCallback() ABSL_LOCKS_EXCLUDED(discovery_cb_mutex_) {
    discovery_cb_mutex_.ReaderLock();
    auto callback = discovery_cb_.lock();
    discovery_cb_mutex_.ReaderUnlock();

    return callback;
  }

  ObserverList<api::BluetoothClassicMedium::Observer> &observers_;
  absl::Mutex discovery_cb_mutex_;
  std::weak_ptr<api::BluetoothClassicMedium::DiscoveryCallback> discovery_cb_
      ABSL_GUARDED_BY(discovery_cb_mutex_);
};

}  // namespace linux
#pragma pop_macro("linux")
}  // namespace nearby

#endif
