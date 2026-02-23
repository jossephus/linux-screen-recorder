#include "screen_recorder_native.h"

#include <utility>

ScreenRecorderNative::ScreenRecorderNative() = default;

ScreenRecorderNative::~ScreenRecorderNative() {
  std::string ignored;
  StopRecording(&ignored);
}

const char* ScreenRecorderNative::StateToString(State state) {
  switch (state) {
    case State::kIdle:
      return "idle";
    case State::kStarting:
      return "starting";
    case State::kRecording:
      return "recording";
    case State::kStopping:
      return "stopping";
  }
  return "unknown";
}

bool ScreenRecorderNative::StartRecording(const std::string& output_path,
                                          uint32_t fps,
                                          bool capture_audio,
                                          const std::string& audio_device,
                                          int output_height,
                                          std::string* error_out) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != State::kIdle) {
      *error_out = "Recorder is not idle";
      return false;
    }
    state_ = State::kStarting;
    message_.clear();
  }

  auto portal = std::make_unique<PortalClient>();
  std::string error;
  auto session = portal->StartMonitorSession(&error);
  if (!session) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::kIdle;
    message_ = error;
    *error_out = error;
    return false;
  }

  auto capture = std::make_unique<PipeWireCapture>(session->node_id,
                                                   session->pipewire_fd,
                                                   session->width,
                                                   session->height,
                                                   fps,
                                                   output_path,
                                                   0,
                                                   true,
                                                   capture_audio,
                                                   audio_device,
                                                   output_height);

  {
    std::lock_guard<std::mutex> lock(mutex_);
    portal_ = std::move(portal);
    session_ = *session;
    capture_ = std::move(capture);
    state_ = State::kRecording;
    message_.clear();
  }

  worker_ = std::thread([this]() {
    std::string run_error;
    bool ok = false;

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (capture_) {
        // Run without holding lock.
      } else {
        state_ = State::kIdle;
        message_ = "Capture instance missing";
        return;
      }
    }

    PipeWireCapture* capture_ptr = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      capture_ptr = capture_.get();
    }
    if (capture_ptr) {
      ok = capture_ptr->Run(&run_error);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (portal_ && session_) {
      portal_->CloseSession(session_->session_handle);
    }
    capture_.reset();
    portal_.reset();
    session_.reset();

    if (state_ == State::kStopping) {
      state_ = State::kIdle;
      message_.clear();
      return;
    }

    state_ = State::kIdle;
    if (!ok) {
      message_ = run_error;
    } else {
      message_.clear();
    }
  });

  return true;
}

bool ScreenRecorderNative::StopRecording(std::string* error_out) {
  std::thread join_thread;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::kIdle) {
      return true;
    }
    if (state_ == State::kStarting) {
      *error_out = "Recorder is still starting";
      return false;
    }
    state_ = State::kStopping;
    if (capture_) {
      capture_->RequestStop();
    }
    if (worker_.joinable()) {
      join_thread = std::move(worker_);
    }
  }

  if (join_thread.joinable()) {
    join_thread.join();
  }
  return true;
}

void ScreenRecorderNative::GetStatus(std::string* state_out, std::string* message_out) const {
  std::lock_guard<std::mutex> lock(mutex_);
  *state_out = StateToString(state_);
  *message_out = message_;
}
