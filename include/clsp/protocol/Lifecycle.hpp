#pragma once

#include <clsp/protocol/Basic.hpp>
#include <clsp/protocol/Capabilities.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace lsp {

struct InitializeParams {
  int processId;
  struct ClientInfo {
    std::string name;
    std::optional<std::string> version;
  };
  std::optional<ClientInfo> clientInfo;
  std::optional<std::string> rootPath;
  std::optional<DocumentUri> rootUri;
  std::optional<nlohmann::json> initializationOptions;
  ClientCapabilities capabilities;
  std::optional<int> trace;
};

inline void from_json(const nlohmann::json &j, InitializeParams &p) {
  j.at("processId").get_to(p.processId);
  if (j.contains("clientInfo")) {
    InitializeParams::ClientInfo ci;
    ci.name = j["clientInfo"].at("name").get<std::string>();
    if (j["clientInfo"].contains("version")) {
      ci.version = j["clientInfo"]["version"].get<std::string>();
    }
    p.clientInfo = std::move(ci);
  }
  if (j.contains("rootPath")) {
    p.rootPath = j["rootPath"].get<std::string>();
  }
  if (j.contains("rootUri") && !j["rootUri"].is_null()) {
    p.rootUri = j["rootUri"].get_to(p.rootUri);
  }
  if (j.contains("initializationOptions") &&
      !j["initializationOptions"].is_null()) {
    p.initializationOptions = j["initializationOptions"];
  }
  if (j.contains("trace")) {
    p.trace = j["trace"].get<int>();
  }
}

struct InitializeResult {
  struct ServerInfo {
    std::string name;
    std::optional<std::string> version;
  };
  ServerCapabilities capabilities;
  std::optional<ServerInfo> serverInfo;
};

inline void to_json(nlohmann::json &j, const InitializeResult::ServerInfo &si) {
  j = {{"name", si.name}};
  if (si.version) {
    j["version"] = *si.version;
  }
}

inline void to_json(nlohmann::json &j, const InitializeResult &r) {
  j = {{"capabilities", r.capabilities}};
  if (r.serverInfo) {
    j["serverInfo"] = *r.serverInfo;
  }
}

} // namespace lsp
