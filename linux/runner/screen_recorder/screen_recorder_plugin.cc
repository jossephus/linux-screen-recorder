#include "screen_recorder_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include <sys/utsname.h>

#include <memory>
#include <string>

#include "screen_recorder_native.h"

#define SCREEN_RECORDER_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), screen_recorder_plugin_get_type(), ScreenRecorderPlugin))

struct _ScreenRecorderPlugin {
  GObject parent_instance;
  std::unique_ptr<ScreenRecorderNative> native;
};

G_DEFINE_TYPE(ScreenRecorderPlugin, screen_recorder_plugin, g_object_get_type())

static FlMethodResponse* start_recording(ScreenRecorderPlugin* self, FlValue* args) {
  if (!args || fl_value_get_type(args) != FL_VALUE_TYPE_MAP) {
    return FL_METHOD_RESPONSE(fl_method_error_response_new(
        "invalid_args", "Expected map args with path and fps", nullptr));
  }

  FlValue* path_v = fl_value_lookup_string(args, "path");
  FlValue* fps_v = fl_value_lookup_string(args, "fps");
  FlValue* audio_v = fl_value_lookup_string(args, "audio");
  FlValue* audio_device_v = fl_value_lookup_string(args, "audioDevice");
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
          : "default";

  std::string error;
  if (!self->native->StartRecording(path, fps, capture_audio, audio_device, &error)) {
    return FL_METHOD_RESPONSE(fl_method_error_response_new("start_failed", error.c_str(), nullptr));
  }

  return FL_METHOD_RESPONSE(fl_method_success_response_new(fl_value_new_bool(true)));
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
