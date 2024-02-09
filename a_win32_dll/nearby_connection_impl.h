// Copyright 2022-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTION_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTION_IMPL_H_

#include <cstdint>
#include <functional>
#include <queue>
#include <string>
#include <vector>

#include "dll_config.h"
#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "internal/platform/mutex.h"
#include "nearby_connection.h"

namespace nearby {
namespace sharing {

class NearbyConnectionsManager;

class DLL_API NearbyConnectionImpl : public NearbyConnection {
 public:
  NearbyConnectionImpl(std::string endpoint_id);
  ~NearbyConnectionImpl() override;

  // NearbyConnection:
  void ReceiveIntroduction();
  void OnReceivedIntroduction();
  void Read(ReadCallback callback) override;
  void Write(std::vector<uint8_t> bytes) override;
  void Close() override;
  void SetDisconnectionListener(std::function<void()> listener) override;

  // Add bytes to the read queue, notifying ReadCallback.
  void WriteMessage(std::vector<uint8_t> bytes);

 private:
  std::string endpoint_id_;

  RecursiveMutex mutex_;
  ReadCallback read_callback_ ABSL_GUARDED_BY(mutex_) = nullptr;
  std::function<void()> disconnect_listener_ ABSL_GUARDED_BY(mutex_);

  // A read queue. The data that we've read from the remote device ends up here
  // until Read() is called to dequeue it.
  std::queue<std::vector<uint8_t>> reads_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTION_IMPL_H_
