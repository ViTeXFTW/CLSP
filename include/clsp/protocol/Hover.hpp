#pragma once

#include <clsp/protocol/Basic.hpp>
#include <clsp/protocol/Documents.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace lsp {

enum class MarkupKind { PlainText, Markdown };

inline const char* to_string(MarkupKind k) {
  switch (k) {
  case MarkupKind::PlainText:
    return "plaintext";
  case MarkupKind::Markdown:
    return "markdown";
  }
  return "plaintext";
}

inline std::optional<MarkupKind> markupKindFromString(const std::string& s) {
  if (s == "plaintext")
    return MarkupKind::PlainText;
  if (s == "markdown")
    return MarkupKind::Markdown;
  return std::nullopt;
}

struct MarkupContent {
  MarkupKind kind = MarkupKind::PlainText;
  std::string value;
};

inline void to_json(nlohmann::json& j, const MarkupContent& m) {
  j = nlohmann::json{{"kind", to_string(m.kind)}, {"value", m.value}};
}

inline void from_json(const nlohmann::json& j, MarkupContent& m) {
  if (j.contains("kind") && j["kind"].is_string()) {
    if (auto k = markupKindFromString(j["kind"].get<std::string>())) {
      m.kind = *k;
    }
  }
  j.at("value").get_to(m.value);
}

struct HoverParams {
  TextDocumentIdentifier textDocument;
  Position position;
};

inline void from_json(const nlohmann::json& j, HoverParams& p) {
  j.at("textDocument").get_to(p.textDocument);
  j.at("position").get_to(p.position);
}

inline void to_json(nlohmann::json& j, const HoverParams& p) {
  j = nlohmann::json{{"textDocument", p.textDocument},
                     {"position", p.position}};
}

struct Hover {
  MarkupContent contents;
  std::optional<Range> range;
};

inline void to_json(nlohmann::json& j, const Hover& h) {
  j = nlohmann::json::object();
  j["contents"] = h.contents;
  if (h.range) {
    j["range"] = *h.range;
  }
}

inline void from_json(const nlohmann::json& j, Hover& h) {
  j.at("contents").get_to(h.contents);
  if (j.contains("range") && !j["range"].is_null()) {
    h.range = j["range"].get<Range>();
  }
}

} // namespace lsp
