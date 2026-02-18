#pragma once

#include <cstdint>
#include <optional>
#include <string>

struct PortalSession {
  std::string session_handle;
  uint32_t node_id = 0;
  int width = 0;
  int height = 0;
  int pipewire_fd = -1;
};

class PortalClient {
 public:
  struct RequestResult {
    uint32_t response_code = 2;
    void* results = nullptr;
  };

  PortalClient();
  ~PortalClient();

  std::optional<PortalSession> StartMonitorSession(std::string* error_out);
  void CloseSession(const std::string& session_handle);

 private:

  std::string MakeHandleToken(const char* prefix);
  std::optional<std::string> CallRequestAndWait(const char* method_name,
                                                void* parameters,
                                                std::string* error_out,
                                                RequestResult* request_out);
  bool SelectSources(const std::string& session_handle, std::string* error_out);
  std::optional<PortalSession> StartSession(const std::string& session_handle,
                                            std::string* error_out);
  std::optional<int> OpenPipeWireRemote(const std::string& session_handle,
                                        std::string* error_out);

  void* connection_ = nullptr;
  uint64_t token_counter_ = 0;
};
