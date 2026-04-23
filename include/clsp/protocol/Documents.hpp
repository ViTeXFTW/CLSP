#pragma once

#include <clsp/protocol/Basic.hpp>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace lsp {

struct TextDocumentItem {
  DocumentUri uri;
  std::string languageId;
  int32_t version;
  std::string text;
};

inline void from_json(const nlohmann::json& j, TextDocumentItem& item) {
  j.at("uri").get_to(item.uri);
  j.at("languageId").get_to(item.languageId);
  j.at("version").get_to(item.version);
  j.at("text").get_to(item.text);
}

struct VersionedTextDocumentIdentifier {
  DocumentUri uri;
  int32_t version;
};

inline void from_json(const nlohmann::json& j,
                      VersionedTextDocumentIdentifier& id) {
  j.at("uri").get_to(id.uri);
  j.at("version").get_to(id.version);
}

struct TextDocumentChangeEvent {
  std::optional<Range> range;
  std::string text;
};

inline void from_json(const nlohmann::json& j, TextDocumentChangeEvent& e) {
  if (j.contains("range") && !j["range"].is_null()) {
    e.range = j["range"].get<Range>();
  }
  j.at("text").get_to(e.text);
}

} // namespace lsp
