#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/types.h>

class FfmpegWriter {
 public:
  FfmpegWriter() = default;
  ~FfmpegWriter();

  bool Start(int width,
             int height,
             uint32_t fps,
             const std::string& output_path,
             bool capture_audio,
             const std::string& audio_device,
             std::string* error_out);
  bool WriteFrame(const uint8_t* data, size_t size, std::string* error_out);
  bool Stop(std::string* error_out);

 private:
  pid_t child_pid_ = -1;
  int stdin_fd_ = -1;
  bool started_ = false;
};
