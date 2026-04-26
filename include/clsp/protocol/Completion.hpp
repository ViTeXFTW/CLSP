#pragma once

#include <clsp/protocol/Basic.hpp>
#include <clsp/protocol/Documents.hpp>
#include <clsp/protocol/Hover.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace lsp {

enum class CompletionTriggerKind {
  Invoked = 1,
  TriggerCharacter = 2,
  TriggerForIncompleteCompletions = 3
};

struct CompletionContext {
  CompletionTriggerKind triggerKind = CompletionTriggerKind::Invoked;
  std::optional<std::string> triggerCharacter;
};

inline void to_json(nlohmann::json& j, const CompletionContext& c) {
  j = nlohmann::json{{"triggerKind", static_cast<int>(c.triggerKind)}};
  if (c.triggerCharacter) {
    j["triggerCharacter"] = *c.triggerCharacter;
  }
}

inline void from_json(const nlohmann::json& j, CompletionContext& c) {
  c.triggerKind =
      static_cast<CompletionTriggerKind>(j.at("triggerKind").get<int>());
  if (j.contains("triggerCharacter") && !j["triggerCharacter"].is_null()) {
    c.triggerCharacter = j["triggerCharacter"].get<std::string>();
  }
}

struct CompletionParams {
  TextDocumentIdentifier textDocument;
  Position position;
  std::optional<CompletionContext> context;
};

inline void to_json(nlohmann::json& j, const CompletionParams& p) {
  j = nlohmann::json{{"textDocument", p.textDocument},
                     {"position", p.position}};
  if (p.context) {
    j["context"] = *p.context;
  }
}

inline void from_json(const nlohmann::json& j, CompletionParams& p) {
  j.at("textDocument").get_to(p.textDocument);
  j.at("position").get_to(p.position);
  if (j.contains("context") && !j["context"].is_null()) {
    p.context = j["context"].get<CompletionContext>();
  }
}

enum class CompletionItemKind {
  Text = 1,
  Method = 2,
  Function = 3,
  Constructor = 4,
  Field = 5,
  Variable = 6,
  Class = 7,
  Interface = 8,
  Module = 9,
  Property = 10,
  Unit = 11,
  Value = 12,
  Enum = 13,
  Keyword = 14,
  Snippet = 15,
  Color = 16,
  File = 17,
  Reference = 18,
  Folder = 19,
  EnumMember = 20,
  Constant = 21,
  Struct = 22,
  Event = 23,
  Operator = 24,
  TypeParameter = 25
};

enum class InsertTextFormat { PlainText = 1, Snippet = 2 };

enum class CompletionItemTag { Deprecated = 1 };

struct TextEdit {
  Range range;
  std::string newText;
};

inline void to_json(nlohmann::json& j, const TextEdit& e) {
  j = nlohmann::json{{"range", e.range}, {"newText", e.newText}};
}

inline void from_json(const nlohmann::json& j, TextEdit& e) {
  j.at("range").get_to(e.range);
  j.at("newText").get_to(e.newText);
}

// `documentation` may be either a plain string or a MarkupContent object.
using CompletionDocumentation = std::variant<std::string, MarkupContent>;

struct CompletionItem {
  std::string label;
  std::optional<CompletionItemKind> kind;
  std::optional<std::vector<CompletionItemTag>> tags;
  std::optional<std::string> detail;
  std::optional<CompletionDocumentation> documentation;
  std::optional<bool> preselect;
  std::optional<std::string> sortText;
  std::optional<std::string> filterText;
  std::optional<std::string> insertText;
  std::optional<InsertTextFormat> insertTextFormat;
  std::optional<TextEdit> textEdit;
  std::optional<std::vector<TextEdit>> additionalTextEdits;
  std::optional<std::vector<std::string>> commitCharacters;
  std::optional<nlohmann::json> data;
};

inline void to_json(nlohmann::json& j, const CompletionItem& i) {
  j = nlohmann::json::object();
  j["label"] = i.label;
  if (i.kind) {
    j["kind"] = static_cast<int>(*i.kind);
  }
  if (i.tags) {
    nlohmann::json arr = nlohmann::json::array();
    for (auto t : *i.tags) {
      arr.push_back(static_cast<int>(t));
    }
    j["tags"] = std::move(arr);
  }
  if (i.detail) {
    j["detail"] = *i.detail;
  }
  if (i.documentation) {
    std::visit(
        [&j](const auto& doc) { j["documentation"] = doc; },
        *i.documentation);
  }
  if (i.preselect) {
    j["preselect"] = *i.preselect;
  }
  if (i.sortText) {
    j["sortText"] = *i.sortText;
  }
  if (i.filterText) {
    j["filterText"] = *i.filterText;
  }
  if (i.insertText) {
    j["insertText"] = *i.insertText;
  }
  if (i.insertTextFormat) {
    j["insertTextFormat"] = static_cast<int>(*i.insertTextFormat);
  }
  if (i.textEdit) {
    j["textEdit"] = *i.textEdit;
  }
  if (i.additionalTextEdits) {
    j["additionalTextEdits"] = *i.additionalTextEdits;
  }
  if (i.commitCharacters) {
    j["commitCharacters"] = *i.commitCharacters;
  }
  if (i.data) {
    j["data"] = *i.data;
  }
}

inline void from_json(const nlohmann::json& j, CompletionItem& i) {
  j.at("label").get_to(i.label);
  if (j.contains("kind") && !j["kind"].is_null()) {
    i.kind = static_cast<CompletionItemKind>(j["kind"].get<int>());
  }
  if (j.contains("tags") && j["tags"].is_array()) {
    std::vector<CompletionItemTag> tags;
    tags.reserve(j["tags"].size());
    for (const auto& t : j["tags"]) {
      tags.push_back(static_cast<CompletionItemTag>(t.get<int>()));
    }
    i.tags = std::move(tags);
  }
  if (j.contains("detail") && !j["detail"].is_null()) {
    i.detail = j["detail"].get<std::string>();
  }
  if (j.contains("documentation") && !j["documentation"].is_null()) {
    const auto& d = j["documentation"];
    if (d.is_string()) {
      i.documentation = d.get<std::string>();
    } else {
      i.documentation = d.get<MarkupContent>();
    }
  }
  if (j.contains("preselect") && !j["preselect"].is_null()) {
    i.preselect = j["preselect"].get<bool>();
  }
  if (j.contains("sortText") && !j["sortText"].is_null()) {
    i.sortText = j["sortText"].get<std::string>();
  }
  if (j.contains("filterText") && !j["filterText"].is_null()) {
    i.filterText = j["filterText"].get<std::string>();
  }
  if (j.contains("insertText") && !j["insertText"].is_null()) {
    i.insertText = j["insertText"].get<std::string>();
  }
  if (j.contains("insertTextFormat") && !j["insertTextFormat"].is_null()) {
    i.insertTextFormat =
        static_cast<InsertTextFormat>(j["insertTextFormat"].get<int>());
  }
  if (j.contains("textEdit") && !j["textEdit"].is_null()) {
    i.textEdit = j["textEdit"].get<TextEdit>();
  }
  if (j.contains("additionalTextEdits") &&
      j["additionalTextEdits"].is_array()) {
    i.additionalTextEdits =
        j["additionalTextEdits"].get<std::vector<TextEdit>>();
  }
  if (j.contains("commitCharacters") && j["commitCharacters"].is_array()) {
    i.commitCharacters =
        j["commitCharacters"].get<std::vector<std::string>>();
  }
  if (j.contains("data")) {
    i.data = j["data"];
  }
}

struct CompletionList {
  bool isIncomplete = false;
  std::vector<CompletionItem> items;
};

inline void to_json(nlohmann::json& j, const CompletionList& l) {
  j = nlohmann::json{{"isIncomplete", l.isIncomplete}, {"items", l.items}};
}

inline void from_json(const nlohmann::json& j, CompletionList& l) {
  j.at("isIncomplete").get_to(l.isIncomplete);
  j.at("items").get_to(l.items);
}

} // namespace lsp
