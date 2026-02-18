#include "pipewire_capture.h"

#include "ffmpeg_writer.h"

#include <pipewire/pipewire.h>

#include <spa/param/format-utils.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>

#include <cerrno>
#include <cstring>
#include <sstream>

PipeWireCapture::PipeWireCapture(uint32_t node_id,
                                 int pipewire_fd,
                                 int width,
                                 int height,
                                 uint32_t fps,
                                 std::string output_path,
                                 uint32_t max_frames,
                                 bool encode_mp4,
                                 bool capture_audio,
                                 std::string audio_device)
    : node_id_(node_id),
      pipewire_fd_(pipewire_fd),
      width_(width),
      height_(height),
      fps_(fps),
      output_path_(std::move(output_path)),
      max_frames_(max_frames),
      encode_mp4_(encode_mp4),
      capture_audio_(capture_audio),
      audio_device_(std::move(audio_device)) {
  stream_events_.version = PW_VERSION_STREAM_EVENTS;
  stream_events_.state_changed = OnStreamStateChanged;
  stream_events_.param_changed = OnStreamParamChanged;
  stream_events_.process = OnProcess;
}

PipeWireCapture::~PipeWireCapture() {
  Shutdown();
}

void PipeWireCapture::OnStreamStateChanged(void* data,
                                           enum pw_stream_state,
                                           enum pw_stream_state state,
                                           const char* error) {
  auto* self = static_cast<PipeWireCapture*>(data);
  if (state == PW_STREAM_STATE_ERROR) {
    self->stream_failed_ = true;
    self->stream_error_ = error ? error : "PipeWire stream error";
    if (self->loop_) {
      pw_main_loop_quit(self->loop_);
    }
  }
}

void PipeWireCapture::OnStreamParamChanged(void* data, uint32_t id, const struct spa_pod* param) {
  auto* self = static_cast<PipeWireCapture*>(data);
  if (id != SPA_PARAM_Format || !param) {
    return;
  }
  spa_format_video_raw_parse(param, &self->video_info_);
  if (self->encode_mp4_) {
    const uint32_t width = self->video_info_.size.width > 0 ? self->video_info_.size.width
                                                            : static_cast<uint32_t>(self->width_);
    const uint32_t height = self->video_info_.size.height > 0 ? self->video_info_.size.height
                                                              : static_cast<uint32_t>(self->height_);
    self->frame_size_bytes_ = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    self->pending_frame_bytes_.clear();
  }
}

void PipeWireCapture::OnProcess(void* data) {
  auto* self = static_cast<PipeWireCapture*>(data);
  struct pw_buffer* buffer = pw_stream_dequeue_buffer(self->stream_);
  if (!buffer) {
    return;
  }

  const struct spa_buffer* spa_buffer = buffer->buffer;
  uint64_t frame_bytes = 0;
  for (uint32_t i = 0; i < spa_buffer->n_datas; ++i) {
    const struct spa_data* d = &spa_buffer->datas[i];
    if (!d->data || !d->chunk) {
      continue;
    }
    const uint32_t size = d->chunk->size;
    if (size == 0) {
      continue;
    }

    const uint8_t* bytes = static_cast<const uint8_t*>(d->data) + d->chunk->offset;
    if (self->encode_mp4_) {
      if (self->frame_size_bytes_ == 0) {
        self->frame_size_bytes_ = static_cast<size_t>(self->width_) * static_cast<size_t>(self->height_) * 4;
      }
      self->pending_frame_bytes_.insert(self->pending_frame_bytes_.end(), bytes, bytes + size);

      while (self->pending_frame_bytes_.size() >= self->frame_size_bytes_) {
        std::string write_error;
        if (!self->ffmpeg_writer_->WriteFrame(self->pending_frame_bytes_.data(),
                                              self->frame_size_bytes_,
                                              &write_error)) {
          self->stream_failed_ = true;
          self->stream_error_ = write_error;
          if (self->loop_) {
            pw_main_loop_quit(self->loop_);
          }
          break;
        }
        frame_bytes += self->frame_size_bytes_;
        self->pending_frame_bytes_.erase(self->pending_frame_bytes_.begin(),
                                         self->pending_frame_bytes_.begin() + self->frame_size_bytes_);
      }
      if (self->stream_failed_) {
        break;
      }
    } else {
      const size_t written = fwrite(bytes, 1, size, self->output_file_);
      if (written != size) {
        self->stream_failed_ = true;
        self->stream_error_ = "Failed writing raw frame data";
        if (self->loop_) {
          pw_main_loop_quit(self->loop_);
        }
        break;
      }
      frame_bytes += written;
    }
  }

  pw_stream_queue_buffer(self->stream_, buffer);

  if (frame_bytes > 0) {
    self->bytes_written_ += frame_bytes;
    const uint32_t frame = ++self->frame_count_;
    if (self->max_frames_ > 0 && frame >= self->max_frames_ && self->loop_) {
      pw_main_loop_quit(self->loop_);
    }
  }

  if (self->stream_failed_ && self->loop_) {
    pw_main_loop_quit(self->loop_);
  }
  if (self->stop_requested_ && self->loop_) {
    pw_main_loop_quit(self->loop_);
  }
}

bool PipeWireCapture::Init(std::string* error_out) {
  if (encode_mp4_) {
    ffmpeg_writer_ = new FfmpegWriter();
    if (!ffmpeg_writer_->Start(width_, height_, fps_, output_path_, capture_audio_, audio_device_,
                               error_out)) {
      return false;
    }
  } else {
    output_file_ = fopen(output_path_.c_str(), "wb");
    if (!output_file_) {
      *error_out = "Failed to open output file: " + std::string(std::strerror(errno));
      return false;
    }
  }

  pw_init(nullptr, nullptr);

  loop_ = pw_main_loop_new(nullptr);
  if (!loop_) {
    *error_out = "Failed to create PipeWire main loop";
    return false;
  }

  context_ = pw_context_new(pw_main_loop_get_loop(loop_), nullptr, 0);
  if (!context_) {
    *error_out = "Failed to create PipeWire context";
    return false;
  }

  if (pipewire_fd_ >= 0) {
    core_ = pw_context_connect_fd(context_, pipewire_fd_, nullptr, 0);
    if (core_) {
      return true;
    }
  }

  core_ = pw_context_connect(context_, nullptr, 0);
  if (!core_) {
    *error_out = "Failed to connect PipeWire core";
    return false;
  }
  return true;
}

bool PipeWireCapture::ConnectStream(std::string* error_out) {
  stream_ = pw_stream_new(
      core_,
      "phase2-capture",
      pw_properties_new(PW_KEY_MEDIA_TYPE, "Video",
                        PW_KEY_MEDIA_CATEGORY, "Capture",
                        PW_KEY_MEDIA_ROLE, "Screen",
                        nullptr));
  if (!stream_) {
    *error_out = "Failed to create PipeWire stream";
    return false;
  }

  pw_stream_add_listener(stream_, &stream_listener_, &stream_events_, this);

  uint8_t buffer[256];
  struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
  const struct spa_pod* params[1];
  params[0] = static_cast<const spa_pod*>(spa_pod_builder_add_object(
      &builder,
      SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
      SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
      SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw)));

  const int rv = pw_stream_connect(
      stream_,
      PW_DIRECTION_INPUT,
      node_id_,
      static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
      params,
      1);
  if (rv != 0) {
    *error_out = "Failed to connect PipeWire stream";
    return false;
  }
  return true;
}

bool PipeWireCapture::Run(std::string* error_out) {
  if (!Init(error_out)) {
    return false;
  }
  if (!ConnectStream(error_out)) {
    return false;
  }

  pw_main_loop_run(loop_);

  if (output_file_) {
    fflush(output_file_);
  }
  if (ffmpeg_writer_) {
    if (!ffmpeg_writer_->Stop(error_out)) {
      return false;
    }
  }

  if (stream_failed_) {
    *error_out = stream_error_.empty() ? "PipeWire stream failed" : stream_error_;
    return false;
  }
  if (bytes_written_ == 0) {
    *error_out = "Capture completed but produced zero bytes";
    return false;
  }
  return true;
}

void PipeWireCapture::RequestStop() {
  stop_requested_ = true;
  if (loop_) {
    pw_main_loop_quit(loop_);
  }
}

void PipeWireCapture::Shutdown() {
  if (stream_) {
    pw_stream_destroy(stream_);
    stream_ = nullptr;
  }
  if (core_) {
    pw_core_disconnect(core_);
    core_ = nullptr;
  }
  if (context_) {
    pw_context_destroy(context_);
    context_ = nullptr;
  }
  if (loop_) {
    pw_main_loop_destroy(loop_);
    loop_ = nullptr;
  }
  if (output_file_) {
    fclose(output_file_);
    output_file_ = nullptr;
  }
  if (ffmpeg_writer_) {
    std::string ignored;
    ffmpeg_writer_->Stop(&ignored);
    delete ffmpeg_writer_;
    ffmpeg_writer_ = nullptr;
  }
}
