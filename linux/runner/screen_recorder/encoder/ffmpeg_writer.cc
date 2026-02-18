#include "ffmpeg_writer.h"

#include <cerrno>
#include <csignal>
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

    if (capture_audio) {
      const std::string input_device = audio_device.empty() ? "default" : audio_device;
      execlp("ffmpeg",
             "ffmpeg",
             "-y",
             "-loglevel",
             "error",
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
             "-f",
             "pulse",
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
             "-shortest",
             output_path.c_str(),
             static_cast<char*>(nullptr));
    } else {
      execlp("ffmpeg",
             "ffmpeg",
             "-y",
             "-loglevel",
             "error",
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
             output_path.c_str(),
             static_cast<char*>(nullptr));
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
