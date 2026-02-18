#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "capture/pipewire_capture.h"
#include "portal/portal_client.h"

class ScreenRecorderNative {
 public:
  ScreenRecorderNative();
  ~ScreenRecorderNative();

  bool StartRecording(const std::string& output_path,
                      uint32_t fps,
                      bool capture_audio,
                      const std::string& audio_device,
                      std::string* error_out);
  bool StopRecording(std::string* error_out);
  void GetStatus(std::string* state_out, std::string* message_out) const;

 private:
  enum class State {
    kIdle,
    kStarting,
    kRecording,
    kStopping,
  };

  static const char* StateToString(State state);

  mutable std::mutex mutex_;
  State state_ = State::kIdle;
  std::string message_;

  std::unique_ptr<PortalClient> portal_;
  std::optional<PortalSession> session_;
  std::unique_ptr<PipeWireCapture> capture_;
  std::thread worker_;
};
