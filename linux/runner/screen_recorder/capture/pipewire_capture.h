#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <pipewire/pipewire.h>
#include <spa/param/video/raw.h>
#include <string>
#include <vector>

class PipeWireCapture {
 public:
  PipeWireCapture(uint32_t node_id,
                  int pipewire_fd,
                  int width,
                  int height,
                  uint32_t fps,
                  std::string output_path,
                  uint32_t max_frames,
                  bool encode_mp4,
                  bool capture_audio,
                  std::string audio_device);
  ~PipeWireCapture();

  bool Run(std::string* error_out);
  void RequestStop();

  static void OnStreamStateChanged(void* data,
                                   enum pw_stream_state old_state,
                                   enum pw_stream_state state,
                                   const char* error);
  static void OnStreamParamChanged(void* data, uint32_t id, const struct spa_pod* param);
  static void OnProcess(void* data);

 private:
  bool Init(std::string* error_out);
  bool ConnectStream(std::string* error_out);
  void Shutdown();

  uint32_t node_id_;
  int pipewire_fd_;
  int width_;
  int height_;
  uint32_t fps_;
  std::string output_path_;
  uint32_t max_frames_;
  bool encode_mp4_;
  bool capture_audio_;
  std::string audio_device_;

  FILE* output_file_ = nullptr;
  class FfmpegWriter* ffmpeg_writer_ = nullptr;
  struct pw_main_loop* loop_ = nullptr;
  struct pw_context* context_ = nullptr;
  struct pw_core* core_ = nullptr;
  struct pw_stream* stream_ = nullptr;
  struct pw_stream_events stream_events_ {};
  struct spa_hook stream_listener_ {};
  struct spa_video_info_raw video_info_ {};
  std::atomic<uint32_t> frame_count_ {0};
  std::atomic<uint64_t> bytes_written_ {0};
  size_t frame_size_bytes_ = 0;
  std::vector<uint8_t> pending_frame_bytes_;
  std::atomic<bool> stop_requested_ {false};
  bool stream_failed_ = false;
  std::string stream_error_;
};
