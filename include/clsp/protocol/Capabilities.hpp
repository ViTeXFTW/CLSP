#pragma once

#include <nlohmann/json.hpp>
#include <optional>

namespace lsp {

enum class TextDocumentSyncKind { None = 0, Full = 1, Incremental = 2 };

struct ServerCapabilities {
  std::optional<TextDocumentSyncKind> textDocumentSync;
};

inline void to_json(nlohmann::json& j, const ServerCapabilities& c) {
  j = nlohmann::json::object();
  if (c.textDocumentSync) {
    j["textDocumentSync"] = static_cast<int>(*c.textDocumentSync);
  }
}

struct ClientCapabilities {};

} // namespace lsp
