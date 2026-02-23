#include "ffmpeg_writer.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

bool WriteAll(int fd, const uint8_t* data, size_t size) {
  size_t written_total = 0;
  while (written_total < size) {
    const ssize_t written = write(fd, data + written_total, size - written_total);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    written_total += static_cast<size_t>(written);
  }
  return true;
}

}  // namespace

FfmpegWriter::~FfmpegWriter() {
  std::string ignored;
  Stop(&ignored);
}

bool FfmpegWriter::Start(int width,
                         int height,
                         uint32_t fps,
                         const std::string& output_path,
                         bool capture_audio,
                         const std::string& audio_device,
                         int output_height,
                         std::string* error_out) {
  if (started_) {
    *error_out = "FFmpeg writer already started";
    return false;
  }

  int pipefd[2];
  if (pipe(pipefd) != 0) {
    *error_out = "Failed to create ffmpeg stdin pipe: " + std::string(std::strerror(errno));
    return false;
  }

  const pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    *error_out = "Failed to fork ffmpeg process: " + std::string(std::strerror(errno));
    return false;
  }

  if (pid == 0) {
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);

    const std::string video_size = std::to_string(width) + "x" + std::to_string(height);
    const std::string fps_s = std::to_string(fps);
    // Output preset is a max target only. Never upscale above captured source size.
    const int target_height = (output_height > 0 && output_height < height) ? output_height : height;
    const int scaled_width =
        std::max(2, static_cast<int>(std::round((static_cast<double>(width) * target_height) /
                                                static_cast<double>(height))));
    const int even_scaled_width = (scaled_width / 2) * 2;
    const int even_scaled_height = (target_height / 2) * 2;
    const bool use_downscale = even_scaled_width > 0 && even_scaled_height > 0 &&
                               (even_scaled_width != width || even_scaled_height != height);
    const std::string scale_filter =
        "scale=" + std::to_string(even_scaled_width) + ":" + std::to_string(even_scaled_height) +
        ":flags=lanczos";

    if (capture_audio) {
      const std::string input_device = audio_device.empty() ? "default" : audio_device;
      if (use_downscale) {
        execlp("ffmpeg",
               "ffmpeg",
               "-y",
               "-loglevel",
               "error",
               "-use_wallclock_as_timestamps",
               "1",
               "-fflags",
               "+genpts",
               "-f",
               "rawvideo",
               "-pix_fmt",
               "bgr0",
               "-video_size",
               video_size.c_str(),
               "-framerate",
               fps_s.c_str(),
               "-i",
               "-",
               "-thread_queue_size",
                "512",
               "-use_wallclock_as_timestamps",
                "1",
               "-f",
                "pulse",
               "-sample_rate",
               "48000",
               "-channels",
               "2",
               "-fragment_size",
               "1024",
               "-i",
                input_device.c_str(),
               "-vf",
               scale_filter.c_str(),
               "-c:v",
               "libx264",
               "-preset",
               "ultrafast",
               "-tune",
               "zerolatency",
               "-bf",
               "0",
               "-pix_fmt",
               "yuv420p",
               "-c:a",
               "aac",
               "-b:a",
               "128k",
               "-af",
               "aresample=async=1:first_pts=0",
               "-vsync",
               "cfr",
               "-shortest",
               output_path.c_str(),
               static_cast<char*>(nullptr));
      } else {
        execlp("ffmpeg",
               "ffmpeg",
               "-y",
               "-loglevel",
               "error",
               "-use_wallclock_as_timestamps",
               "1",
               "-fflags",
               "+genpts",
               "-f",
               "rawvideo",
               "-pix_fmt",
               "bgr0",
               "-video_size",
               video_size.c_str(),
               "-framerate",
               fps_s.c_str(),
               "-i",
               "-",
               "-thread_queue_size",
                "512",
               "-use_wallclock_as_timestamps",
                "1",
               "-f",
                "pulse",
               "-sample_rate",
               "48000",
               "-channels",
               "2",
               "-fragment_size",
               "1024",
               "-i",
                input_device.c_str(),
               "-c:v",
               "libx264",
               "-preset",
               "ultrafast",
               "-tune",
               "zerolatency",
               "-bf",
               "0",
               "-pix_fmt",
               "yuv420p",
               "-c:a",
               "aac",
               "-b:a",
               "128k",
               "-af",
               "aresample=async=1:first_pts=0",
               "-vsync",
               "cfr",
               "-shortest",
               output_path.c_str(),
               static_cast<char*>(nullptr));
      }
    } else {
      if (use_downscale) {
        execlp("ffmpeg",
               "ffmpeg",
               "-y",
               "-loglevel",
               "error",
               "-use_wallclock_as_timestamps",
               "1",
               "-fflags",
               "+genpts",
               "-f",
               "rawvideo",
               "-pix_fmt",
               "bgr0",
               "-video_size",
               video_size.c_str(),
               "-framerate",
               fps_s.c_str(),
               "-i",
               "-",
               "-vf",
               scale_filter.c_str(),
               "-c:v",
               "libx264",
               "-preset",
               "ultrafast",
               "-tune",
               "zerolatency",
               "-bf",
               "0",
               "-pix_fmt",
               "yuv420p",
               "-vsync",
               "cfr",
               output_path.c_str(),
               static_cast<char*>(nullptr));
      } else {
        execlp("ffmpeg",
               "ffmpeg",
               "-y",
               "-loglevel",
               "error",
               "-use_wallclock_as_timestamps",
               "1",
               "-fflags",
               "+genpts",
               "-f",
               "rawvideo",
               "-pix_fmt",
               "bgr0",
               "-video_size",
               video_size.c_str(),
               "-framerate",
               fps_s.c_str(),
               "-i",
               "-",
               "-c:v",
               "libx264",
               "-preset",
               "ultrafast",
               "-tune",
               "zerolatency",
               "-bf",
               "0",
               "-pix_fmt",
               "yuv420p",
               "-vsync",
               "cfr",
               output_path.c_str(),
               static_cast<char*>(nullptr));
      }
    }
    _exit(127);
  }

  close(pipefd[0]);
  stdin_fd_ = pipefd[1];
  child_pid_ = pid;
  started_ = true;
  return true;
}

bool FfmpegWriter::WriteFrame(const uint8_t* data, size_t size, std::string* error_out) {
  if (!started_ || stdin_fd_ < 0) {
    *error_out = "FFmpeg writer is not started";
    return false;
  }
  if (!WriteAll(stdin_fd_, data, size)) {
    *error_out = "Failed writing frame to ffmpeg stdin: " + std::string(std::strerror(errno));
    return false;
  }
  return true;
}

bool FfmpegWriter::Stop(std::string* error_out) {
  if (!started_) {
    return true;
  }

  if (stdin_fd_ >= 0) {
    close(stdin_fd_);
    stdin_fd_ = -1;
  }

  int status = 0;
  if (waitpid(child_pid_, &status, 0) < 0) {
    *error_out = "Failed waiting for ffmpeg process: " + std::string(std::strerror(errno));
    started_ = false;
    child_pid_ = -1;
    return false;
  }

  started_ = false;
  child_pid_ = -1;

  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    return true;
  }

  std::ostringstream oss;
  if (WIFEXITED(status)) {
    oss << "ffmpeg exited with code " << WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    oss << "ffmpeg killed by signal " << WTERMSIG(status);
  } else {
    oss << "ffmpeg exited abnormally";
  }
  *error_out = oss.str();
  return false;
}
