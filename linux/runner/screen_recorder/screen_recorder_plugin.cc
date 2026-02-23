#include "screen_recorder_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include <sys/utsname.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "screen_recorder_native.h"

#define SCREEN_RECORDER_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), screen_recorder_plugin_get_type(), ScreenRecorderPlugin))

struct _ScreenRecorderPlugin {
  GObject parent_instance;
  std::unique_ptr<ScreenRecorderNative> native;
};

G_DEFINE_TYPE(ScreenRecorderPlugin, screen_recorder_plugin, g_object_get_type())

namespace {

std::string Trim(std::string value) {
  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }
  size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }
  return value.substr(start, end - start);
}

std::string RunCommandAndCapture(const std::string& command) {
  std::array<char, 256> buffer {};
  std::string output;
  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe) {
    return output;
  }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }
  pclose(pipe);
  return Trim(output);
}

std::string ReadFirstLine(const std::string& file_path) {
  std::ifstream file(file_path);
  if (!file.is_open()) {
    return "";
  }
  std::string line;
  std::getline(file, line);
  return Trim(line);
}

bool SourceExists(const std::string& source_name) {
  const std::string sources = RunCommandAndCapture("pactl list short sources 2>/dev/null");
  if (sources.empty()) {
    return false;
  }
  std::istringstream stream(sources);
  std::string line;
  while (std::getline(stream, line)) {
    if (line.find(source_name) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::string DetectRecommendedAudioDevice() {
  const char* env_audio_device = std::getenv("SCREEN_RECORDER_AUDIO_DEVICE");
  if (env_audio_device != nullptr) {
    const std::string value = Trim(env_audio_device);
    if (!value.empty()) {
      return value;
    }
  }

  const char* home = std::getenv("HOME");
  if (home != nullptr) {
    const std::string config_path = std::string(home) + "/.config/screen-recorder/audio-device";
    const std::string from_file = ReadFirstLine(config_path);
    if (!from_file.empty()) {
      return from_file;
    }
  }

  const std::string default_sink = RunCommandAndCapture("pactl get-default-sink 2>/dev/null");
  if (!default_sink.empty()) {
    const std::string monitor_source = default_sink + ".monitor";
    if (SourceExists(monitor_source)) {
      return monitor_source;
    }
  }

  const std::string sources = RunCommandAndCapture("pactl list short sources 2>/dev/null");
  if (!sources.empty()) {
    std::istringstream stream(sources);
    std::string line;
    while (std::getline(stream, line)) {
      if (line.find(".monitor") == std::string::npos) {
        continue;
      }
      std::istringstream columns(line);
      std::string id;
      std::string source_name;
      if (columns >> id >> source_name) {
        return source_name;
      }
    }
  }

  return "default";
}

}  // namespace

static FlMethodResponse* start_recording(ScreenRecorderPlugin* self, FlValue* args) {
  if (!args || fl_value_get_type(args) != FL_VALUE_TYPE_MAP) {
    return FL_METHOD_RESPONSE(fl_method_error_response_new(
        "invalid_args", "Expected map args with path and fps", nullptr));
  }

  FlValue* path_v = fl_value_lookup_string(args, "path");
  FlValue* fps_v = fl_value_lookup_string(args, "fps");
  FlValue* audio_v = fl_value_lookup_string(args, "audio");
  FlValue* audio_device_v = fl_value_lookup_string(args, "audioDevice");
  FlValue* output_height_v = fl_value_lookup_string(args, "outputHeight");
  if (!path_v || fl_value_get_type(path_v) != FL_VALUE_TYPE_STRING || !fps_v ||
      fl_value_get_type(fps_v) != FL_VALUE_TYPE_INT) {
    return FL_METHOD_RESPONSE(fl_method_error_response_new(
        "invalid_args", "Missing required args: path(string), fps(int)", nullptr));
  }

  const std::string path = fl_value_get_string(path_v);
  const uint32_t fps = static_cast<uint32_t>(fl_value_get_int(fps_v));
  const bool capture_audio = audio_v && fl_value_get_type(audio_v) == FL_VALUE_TYPE_BOOL
                                 ? fl_value_get_bool(audio_v)
                                 : false;
  const std::string audio_device =
      audio_device_v && fl_value_get_type(audio_device_v) == FL_VALUE_TYPE_STRING
          ? fl_value_get_string(audio_device_v)
          : "auto";
  int output_height =
      output_height_v && fl_value_get_type(output_height_v) == FL_VALUE_TYPE_INT
          ? static_cast<int>(fl_value_get_int(output_height_v))
          : 0;
  if (output_height < 0) {
    output_height = 0;
  }
  std::string resolved_audio_device = audio_device;
  if (capture_audio && (resolved_audio_device.empty() || resolved_audio_device == "auto")) {
    resolved_audio_device = DetectRecommendedAudioDevice();
  }

  std::string error;
  if (!self->native->StartRecording(path,
                                    fps,
                                    capture_audio,
                                    resolved_audio_device,
                                    output_height,
                                    &error)) {
    return FL_METHOD_RESPONSE(fl_method_error_response_new("start_failed", error.c_str(), nullptr));
  }

  return FL_METHOD_RESPONSE(fl_method_success_response_new(fl_value_new_bool(true)));
}

static FlMethodResponse* get_recommended_audio_device() {
  const std::string device = DetectRecommendedAudioDevice();
  return FL_METHOD_RESPONSE(fl_method_success_response_new(fl_value_new_string(device.c_str())));
}

static FlMethodResponse* get_display_resolution() {
  int width = 1920;
  int height = 1080;
  GdkDisplay* display = gdk_display_get_default();
  if (display != nullptr) {
    GdkMonitor* monitor = gdk_display_get_primary_monitor(display);
    if (monitor == nullptr && gdk_display_get_n_monitors(display) > 0) {
      monitor = gdk_display_get_monitor(display, 0);
    }
    if (monitor != nullptr) {
      GdkRectangle geometry;
      gdk_monitor_get_geometry(monitor, &geometry);
      const int scale_factor = std::max(1, gdk_monitor_get_scale_factor(monitor));
      width = geometry.width * scale_factor;
      height = geometry.height * scale_factor;
    }
  }
  FlValue* map = fl_value_new_map();
  fl_value_set_string_take(map, "width", fl_value_new_int(width));
  fl_value_set_string_take(map, "height", fl_value_new_int(height));
  return FL_METHOD_RESPONSE(fl_method_success_response_new(map));
}

static FlMethodResponse* stop_recording(ScreenRecorderPlugin* self) {
  std::string error;
  if (!self->native->StopRecording(&error)) {
    return FL_METHOD_RESPONSE(fl_method_error_response_new("stop_failed", error.c_str(), nullptr));
  }
  return FL_METHOD_RESPONSE(fl_method_success_response_new(fl_value_new_bool(true)));
}

static FlMethodResponse* get_status(ScreenRecorderPlugin* self) {
  std::string state;
  std::string message;
  self->native->GetStatus(&state, &message);

  FlValue* map = fl_value_new_map();
  fl_value_set_string_take(map, "state", fl_value_new_string(state.c_str()));
  fl_value_set_string_take(map, "message", fl_value_new_string(message.c_str()));
  return FL_METHOD_RESPONSE(fl_method_success_response_new(map));
}

static void screen_recorder_plugin_handle_method_call(ScreenRecorderPlugin* self,
                                                      FlMethodCall* method_call) {
  g_autoptr(FlMethodResponse) response = nullptr;
  const gchar* method = fl_method_call_get_name(method_call);

  if (strcmp(method, "startRecording") == 0) {
    response = start_recording(self, fl_method_call_get_args(method_call));
  } else if (strcmp(method, "getRecommendedAudioDevice") == 0) {
    response = get_recommended_audio_device();
  } else if (strcmp(method, "getDisplayResolution") == 0) {
    response = get_display_resolution();
  } else if (strcmp(method, "stopRecording") == 0) {
    response = stop_recording(self);
  } else if (strcmp(method, "getStatus") == 0) {
    response = get_status(self);
  } else {
    response = FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
  }

  fl_method_call_respond(method_call, response, nullptr);
}

static void screen_recorder_plugin_dispose(GObject* object) {
  auto* self = SCREEN_RECORDER_PLUGIN(object);
  self->native.reset();
  G_OBJECT_CLASS(screen_recorder_plugin_parent_class)->dispose(object);
}

static void screen_recorder_plugin_class_init(ScreenRecorderPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = screen_recorder_plugin_dispose;
}

static void screen_recorder_plugin_init(ScreenRecorderPlugin* self) {
  self->native = std::make_unique<ScreenRecorderNative>();
}

static void method_call_cb(FlMethodChannel* channel, FlMethodCall* method_call, gpointer user_data) {
  auto* plugin = SCREEN_RECORDER_PLUGIN(user_data);
  screen_recorder_plugin_handle_method_call(plugin, method_call);
}

void screen_recorder_plugin_register_with_registrar(FlPluginRegistrar* registrar) {
  ScreenRecorderPlugin* plugin = SCREEN_RECORDER_PLUGIN(
      g_object_new(screen_recorder_plugin_get_type(), nullptr));

  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
  g_autoptr(FlMethodChannel) channel =
      fl_method_channel_new(fl_plugin_registrar_get_messenger(registrar),
                            "screen_recorder",
                            FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(channel, method_call_cb, g_object_ref(plugin),
                                            g_object_unref);

  g_object_unref(plugin);
}
