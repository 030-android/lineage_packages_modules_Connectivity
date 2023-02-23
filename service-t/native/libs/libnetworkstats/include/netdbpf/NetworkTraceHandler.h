/**
 * Copyright (c) 2023, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <perfetto/base/task_runner.h>
#include <perfetto/tracing.h>

#include <string>
#include <unordered_map>

#include "bpf/BpfMap.h"
#include "bpf/BpfRingbuf.h"

// For PacketTrace struct definition
#include "netd.h"

namespace android {
namespace bpf {

// NetworkTracePoller is responsible for interactions with the BPF ring buffer
// including polling. This class is an internal helper for NetworkTraceHandler,
// it is not meant to be used elsewhere.
class NetworkTracePoller {
 public:
  // Testonly: initialize with a callback capable of intercepting data.
  NetworkTracePoller(std::function<void(const PacketTrace&)> callback)
      : mCallback(std::move(callback)) {}

  // Testonly: standalone functions without perfetto dependency.
  bool Start(uint32_t pollMs);
  bool Stop();
  bool ConsumeAll();

 private:
  void SchedulePolling();

  // How often to poll the ring buffer, defined by the trace config.
  uint32_t mPollMs;

  // The function to process PacketTrace, typically a Perfetto sink.
  std::function<void(const PacketTrace&)> mCallback;

  // The BPF ring buffer handle.
  std::unique_ptr<BpfRingbuf<PacketTrace>> mRingBuffer;

  // The packet tracing config map (really a 1-element array).
  BpfMap<uint32_t, bool> mConfigurationMap;

  // This must be the last member, causing it to be the first deleted. If it is
  // not, members required for callbacks can be deleted before it's stopped.
  std::unique_ptr<perfetto::base::TaskRunner> mTaskRunner;
};

// NetworkTraceHandler implements the android.network_packets data source. This
// class is registered with Perfetto and is instantiated when tracing starts and
// destroyed when tracing ends. There is one instance per trace session.
class NetworkTraceHandler : public perfetto::DataSource<NetworkTraceHandler> {
 public:
  // Registers this DataSource.
  static void RegisterDataSource();

  // Connects to the system Perfetto daemon and registers the trace handler.
  static void InitPerfettoTracing();

  // Initialize with the default Perfetto callback.
  NetworkTraceHandler();

  // perfetto::DataSource overrides:
  void OnSetup(const SetupArgs& args) override;
  void OnStart(const StartArgs&) override;
  void OnStop(const StopArgs&) override;

 private:
  // Convert a PacketTrace into a Perfetto trace packet.
  static void Fill(const PacketTrace& src,
                   ::perfetto::protos::pbzero::TracePacket& dst);

  NetworkTracePoller mPoller;
  uint32_t mPollMs;
};

}  // namespace bpf
}  // namespace android
