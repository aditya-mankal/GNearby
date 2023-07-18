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

use thiserror::Error;

/// Library error type.
#[non_exhaustive]
#[derive(Error, Debug)]
pub enum BluetoothError {
    /// Indicates that the operation was rejected because the system is not in
    /// a state required for the operation's execution.
    /// E.g. The user calls `stop_scan()` or polls the advertisement stream
    /// before calling `start_scan()`.
    #[error("failed precondition: {0}")]
    FailedPrecondition(String),
    /// Reported when the user calls an operation that is supported by their
    /// Operating System, but is not supported by their device.
    /// E.g. a Windows machine with an old BT Classic adapter that
    /// doesn't support BLE).
    #[error("bluetooth operation not supported by system: {0}")]
    NotSupported(String),
    /// Wrapper around OS-level errors, e.g. `windows::core::Error` for Windows.
    /// These typically mean something is very wrong with the system (e.g. OOM).
    #[error("bluetooth system-level error: {0}")]
    System(String),
    /// Reported when a bug occurs inside the library. Whenever a seemingly
    /// impossible error condition arises where you could call `expect()`,
    /// return this error instead.
    #[error("internal error: {0}")]
    Internal(String),
}
