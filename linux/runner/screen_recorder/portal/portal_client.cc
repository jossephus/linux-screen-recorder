#include "portal_client.h"

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include <sstream>

namespace {

constexpr const char* kPortalBusName = "org.freedesktop.portal.Desktop";
constexpr const char* kPortalObjectPath = "/org/freedesktop/portal/desktop";
constexpr const char* kScreenCastIface = "org.freedesktop.portal.ScreenCast";
constexpr const char* kRequestIface = "org.freedesktop.portal.Request";
constexpr const char* kSessionIface = "org.freedesktop.portal.Session";

struct WaitContext {
  GMainLoop* loop = nullptr;
  PortalClient::RequestResult* out = nullptr;
};

void OnRequestResponse(GDBusConnection*,
                       const gchar*,
                       const gchar*,
                       const gchar*,
                       const gchar*,
                       GVariant* parameters,
                       gpointer user_data) {
  auto* ctx = static_cast<WaitContext*>(user_data);
  guint32 code = 2;
  GVariant* results = nullptr;
  g_variant_get(parameters, "(u@a{sv})", &code, &results);
  ctx->out->response_code = code;
  ctx->out->results = results;
  g_main_loop_quit(ctx->loop);
}

std::string BuildRequestPathFromHandle(const std::string& handle) {
  return handle;
}

}  // namespace

PortalClient::PortalClient() {
  GError* error = nullptr;
  connection_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
  if (!connection_ && error) {
    g_error_free(error);
  }
}

PortalClient::~PortalClient() {
  if (connection_) {
    g_object_unref(static_cast<GDBusConnection*>(connection_));
  }
}

std::string PortalClient::MakeHandleToken(const char* prefix) {
  std::ostringstream oss;
  oss << prefix << "_" << ++token_counter_;
  return oss.str();
}

std::optional<std::string> PortalClient::CallRequestAndWait(const char* method_name,
                                                            void* parameters,
                                                            std::string* error_out,
                                                            RequestResult* request_out) {
  if (!connection_) {
    *error_out = "DBus session bus is unavailable";
    return std::nullopt;
  }

  GError* error = nullptr;
  GVariant* result = g_dbus_connection_call_sync(
      static_cast<GDBusConnection*>(connection_),
      kPortalBusName,
      kPortalObjectPath,
      kScreenCastIface,
      method_name,
      static_cast<GVariant*>(parameters),
      G_VARIANT_TYPE("(o)"),
      G_DBUS_CALL_FLAGS_NONE,
      -1,
      nullptr,
      &error);
  if (!result) {
    *error_out = error ? error->message : "Unknown DBus call failure";
    if (error) {
      g_error_free(error);
    }
    return std::nullopt;
  }

  const gchar* request_handle = nullptr;
  g_variant_get(result, "(o)", &request_handle);
  std::string request_path = BuildRequestPathFromHandle(request_handle);
  g_variant_unref(result);

  GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
  WaitContext wait_ctx;
  wait_ctx.loop = loop;
  wait_ctx.out = request_out;
  guint sub_id = g_dbus_connection_signal_subscribe(
      static_cast<GDBusConnection*>(connection_),
      kPortalBusName,
      kRequestIface,
      "Response",
      request_path.c_str(),
      nullptr,
      G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
      OnRequestResponse,
      &wait_ctx,
      nullptr);

  g_main_loop_run(loop);
  g_dbus_connection_signal_unsubscribe(static_cast<GDBusConnection*>(connection_), sub_id);
  g_main_loop_unref(loop);

  if (request_out->response_code != 0) {
    *error_out = "Portal request was denied or canceled";
    if (request_out->results) {
      g_variant_unref(static_cast<GVariant*>(request_out->results));
      request_out->results = nullptr;
    }
    return std::nullopt;
  }

  return request_path;
}

bool PortalClient::SelectSources(const std::string& session_handle, std::string* error_out) {
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "types", g_variant_new_uint32(1));
  g_variant_builder_add(&options, "{sv}", "multiple", g_variant_new_boolean(FALSE));
  g_variant_builder_add(&options, "{sv}", "handle_token",
                        g_variant_new_string(MakeHandleToken("select").c_str()));

  RequestResult response;
  auto request = CallRequestAndWait(
      "SelectSources",
      g_variant_new("(oa{sv})", session_handle.c_str(), &options),
      error_out,
      &response);
  if (!request) {
    return false;
  }

  if (response.results) {
    g_variant_unref(static_cast<GVariant*>(response.results));
  }
  return true;
}

std::optional<PortalSession> PortalClient::StartSession(const std::string& session_handle,
                                                        std::string* error_out) {
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "handle_token",
                        g_variant_new_string(MakeHandleToken("start").c_str()));

  RequestResult response;
  auto request = CallRequestAndWait(
      "Start",
      g_variant_new("(osa{sv})", session_handle.c_str(), "", &options),
      error_out,
      &response);
  if (!request) {
    return std::nullopt;
  }

  auto* results = static_cast<GVariant*>(response.results);
  if (!results) {
    *error_out = "Portal returned empty start results";
    return std::nullopt;
  }

  GVariant* streams = g_variant_lookup_value(results, "streams", G_VARIANT_TYPE("a(ua{sv})"));
  if (!streams) {
    g_variant_unref(results);
    *error_out = "Portal start response has no stream list";
    return std::nullopt;
  }

  GVariantIter iter;
  g_variant_iter_init(&iter, streams);
  GVariant* stream = g_variant_iter_next_value(&iter);
  if (!stream) {
    g_variant_unref(streams);
    g_variant_unref(results);
    *error_out = "Portal returned zero streams";
    return std::nullopt;
  }

  guint32 node_id = 0;
  GVariant* props = nullptr;
  g_variant_get(stream, "(u@a{sv})", &node_id, &props);

  int width = 0;
  int height = 0;
  GVariant* size = g_variant_lookup_value(props, "size", G_VARIANT_TYPE("(ii)"));
  if (size) {
    g_variant_get(size, "(ii)", &width, &height);
    g_variant_unref(size);
  }

  g_variant_unref(props);
  g_variant_unref(stream);
  g_variant_unref(streams);
  g_variant_unref(results);

  auto pipewire_fd = OpenPipeWireRemote(session_handle, error_out);
  if (!pipewire_fd) {
    return std::nullopt;
  }

  PortalSession session;
  session.session_handle = session_handle;
  session.node_id = node_id;
  session.width = width;
  session.height = height;
  session.pipewire_fd = *pipewire_fd;
  return session;
}

std::optional<int> PortalClient::OpenPipeWireRemote(const std::string& session_handle,
                                                    std::string* error_out) {
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);

  GError* error = nullptr;
  GUnixFDList* out_fds = nullptr;
  GVariant* reply = g_dbus_connection_call_with_unix_fd_list_sync(
      static_cast<GDBusConnection*>(connection_),
      kPortalBusName,
      kPortalObjectPath,
      kScreenCastIface,
      "OpenPipeWireRemote",
      g_variant_new("(oa{sv})", session_handle.c_str(), &options),
      G_VARIANT_TYPE("(h)"),
      G_DBUS_CALL_FLAGS_NONE,
      -1,
      nullptr,
      &out_fds,
      nullptr,
      &error);
  if (!reply) {
    *error_out = error ? error->message : "OpenPipeWireRemote failed";
    if (error) {
      g_error_free(error);
    }
    return std::nullopt;
  }

  gint fd_idx = -1;
  g_variant_get(reply, "(h)", &fd_idx);
  g_variant_unref(reply);

  int fd = g_unix_fd_list_get(out_fds, fd_idx, &error);
  g_object_unref(out_fds);
  if (fd < 0) {
    *error_out = error ? error->message : "Failed to extract PipeWire fd";
    if (error) {
      g_error_free(error);
    }
    return std::nullopt;
  }
  return fd;
}

std::optional<PortalSession> PortalClient::StartMonitorSession(std::string* error_out) {
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "handle_token",
                        g_variant_new_string(MakeHandleToken("create").c_str()));
  g_variant_builder_add(&options, "{sv}", "session_handle_token",
                        g_variant_new_string(MakeHandleToken("session").c_str()));

  RequestResult response;
  auto request = CallRequestAndWait("CreateSession",
                                    g_variant_new("(a{sv})", &options),
                                    error_out,
                                    &response);
  if (!request) {
    return std::nullopt;
  }

  auto* create_results = static_cast<GVariant*>(response.results);
  if (!create_results) {
    *error_out = "CreateSession returned no results";
    return std::nullopt;
  }

  GVariant* session_handle_v = g_variant_lookup_value(create_results, "session_handle", nullptr);
  if (!session_handle_v) {
    g_variant_unref(create_results);
    *error_out = "CreateSession missing session_handle";
    return std::nullopt;
  }

  const gchar* session_handle_c = nullptr;
  if (g_variant_is_of_type(session_handle_v, G_VARIANT_TYPE_OBJECT_PATH)) {
    g_variant_get(session_handle_v, "o", &session_handle_c);
  } else if (g_variant_is_of_type(session_handle_v, G_VARIANT_TYPE_STRING)) {
    g_variant_get(session_handle_v, "s", &session_handle_c);
  } else {
    g_variant_unref(session_handle_v);
    g_variant_unref(create_results);
    *error_out = "CreateSession session_handle has unexpected type";
    return std::nullopt;
  }
  std::string session_handle = session_handle_c;
  g_variant_unref(session_handle_v);
  g_variant_unref(create_results);

  if (!SelectSources(session_handle, error_out)) {
    CloseSession(session_handle);
    return std::nullopt;
  }

  auto started = StartSession(session_handle, error_out);
  if (!started) {
    CloseSession(session_handle);
    return std::nullopt;
  }

  return started;
}

void PortalClient::CloseSession(const std::string& session_handle) {
  if (!connection_) {
    return;
  }

  g_dbus_connection_call(
      static_cast<GDBusConnection*>(connection_),
      kPortalBusName,
      session_handle.c_str(),
      kSessionIface,
      "Close",
      nullptr,
      nullptr,
      G_DBUS_CALL_FLAGS_NONE,
      -1,
      nullptr,
      nullptr,
      nullptr);
}
