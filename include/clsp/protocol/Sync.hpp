#pragma once

#include "clsp/protocol/Basic.hpp"
#include "clsp/protocol/Documents.hpp"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace lsp {

struct DidOpenParams {
  TextDocumentItem textDocument;
};

inline void from_json(const nlohmann::json& j, DidOpenParams& p) {
  p.textDocument = j.at("textDocument").get<TextDocumentItem>();
}

struct DidChangeParams {
  VersionedTextDocumentIdentifier textDocument;
  std::vector<TextDocumentContentChangeEvent> contentChanges;
};

inline void from_json(const nlohmann::json& j, DidChangeParams& p) {
  p.textDocument = j.at("textDocument").get<VersionedTextDocumentIdentifier>();
  p.contentChanges =
      j.at("contentChanges").get<std::vector<TextDocumentContentChangeEvent>>();
}

struct DidCloseParams {
  DocumentUri uri;
};

inline void from_json(const nlohmann::json& j, DidCloseParams& p) {
  p.uri = j.at("textDocument").at("uri").get<DocumentUri>();
}

struct DidSaveParams {
  DocumentUri uri;
  std::optional<std::string> text;
};

inline void from_json(const nlohmann::json& j, DidSaveParams& p) {
  p.uri = j.at("textDocument").at("uri").get<DocumentUri>();
  if (j.contains("text") && !j["text"].is_null()) {
    p.text = j["text"].get<std::string>();
  }
}

} // namespace lsp
