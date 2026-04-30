#pragma once

#include <algorithm>
#include <array>
#include <clsp/protocol/Basic.hpp>
#include <clsp/protocol/Documents.hpp>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace lsp {

// Standard LSP semantic token types (specification 3.17). Provided as a
// convenience; servers may declare their own legend with arbitrary strings.
namespace semantic_token_types {
inline constexpr const char* kNamespace = "namespace";
inline constexpr const char* kType = "type";
inline constexpr const char* kClass = "class";
inline constexpr const char* kEnum = "enum";
inline constexpr const char* kInterface = "interface";
inline constexpr const char* kStruct = "struct";
inline constexpr const char* kTypeParameter = "typeParameter";
inline constexpr const char* kParameter = "parameter";
inline constexpr const char* kVariable = "variable";
inline constexpr const char* kProperty = "property";
inline constexpr const char* kEnumMember = "enumMember";
inline constexpr const char* kEvent = "event";
inline constexpr const char* kFunction = "function";
inline constexpr const char* kMethod = "method";
inline constexpr const char* kMacro = "macro";
inline constexpr const char* kKeyword = "keyword";
inline constexpr const char* kModifier = "modifier";
inline constexpr const char* kComment = "comment";
inline constexpr const char* kString = "string";
inline constexpr const char* kNumber = "number";
inline constexpr const char* kRegexp = "regexp";
inline constexpr const char* kOperator = "operator";
inline constexpr const char* kDecorator = "decorator";
} // namespace semantic_token_types

// Standard LSP semantic token modifiers. Bit positions correspond to legend
// indices when used with the helpers below.
namespace semantic_token_modifiers {
inline constexpr const char* kDeclaration = "declaration";
inline constexpr const char* kDefinition = "definition";
inline constexpr const char* kReadonly = "readonly";
inline constexpr const char* kStatic = "static";
inline constexpr const char* kDeprecated = "deprecated";
inline constexpr const char* kAbstract = "abstract";
inline constexpr const char* kAsync = "async";
inline constexpr const char* kModification = "modification";
inline constexpr const char* kDocumentation = "documentation";
inline constexpr const char* kDefaultLibrary = "defaultLibrary";
} // namespace semantic_token_modifiers

struct SemanticTokensLegend {
  std::vector<std::string> tokenTypes;
  std::vector<std::string> tokenModifiers;
};

inline void to_json(nlohmann::json& j, const SemanticTokensLegend& l) {
  j = nlohmann::json{{"tokenTypes", l.tokenTypes},
                     {"tokenModifiers", l.tokenModifiers}};
}

inline void from_json(const nlohmann::json& j, SemanticTokensLegend& l) {
  j.at("tokenTypes").get_to(l.tokenTypes);
  j.at("tokenModifiers").get_to(l.tokenModifiers);
}

struct SemanticTokensOptions {
  SemanticTokensLegend legend;
  std::optional<bool> range;
  std::optional<bool> full;
};

inline void to_json(nlohmann::json& j, const SemanticTokensOptions& o) {
  j = nlohmann::json::object();
  j["legend"] = o.legend;
  if (o.range) {
    j["range"] = *o.range;
  }
  if (o.full) {
    j["full"] = *o.full;
  }
}

struct SemanticTokensParams {
  TextDocumentIdentifier textDocument;
};

inline void from_json(const nlohmann::json& j, SemanticTokensParams& p) {
  j.at("textDocument").get_to(p.textDocument);
}

inline void to_json(nlohmann::json& j, const SemanticTokensParams& p) {
  j = nlohmann::json{{"textDocument", p.textDocument}};
}

struct SemanticTokensRangeParams {
  TextDocumentIdentifier textDocument;
  Range range;
};

inline void from_json(const nlohmann::json& j, SemanticTokensRangeParams& p) {
  j.at("textDocument").get_to(p.textDocument);
  j.at("range").get_to(p.range);
}

inline void to_json(nlohmann::json& j, const SemanticTokensRangeParams& p) {
  j = nlohmann::json{{"textDocument", p.textDocument}, {"range", p.range}};
}

struct SemanticTokens {
  std::optional<std::string> resultId;
  std::vector<uint32_t> data;
};

inline void to_json(nlohmann::json& j, const SemanticTokens& t) {
  j = nlohmann::json::object();
  if (t.resultId) {
    j["resultId"] = *t.resultId;
  }
  j["data"] = t.data;
}

inline void from_json(const nlohmann::json& j, SemanticTokens& t) {
  if (j.contains("resultId") && j["resultId"].is_string()) {
    t.resultId = j["resultId"].get<std::string>();
  }
  j.at("data").get_to(t.data);
}

// Builds the LSP semantic tokens `data` array from absolute (line, character,
// length, type, modifiers) tuples. Handles sorting and the relative encoding
// required by the protocol so callers do not have to.
//
// Token type and modifier values are indices into the legend declared in
// SemanticTokensOptions. `modifiers` is a bitmask: bit N set means the
// modifier at legend index N applies.
//
// Usage:
//     SemanticTokensBuilder b;
//     b.push(0, 0, 3, kKeyword);
//     b.push(0, 4, 5, kVariable, (1u << kReadonly));
//     SemanticTokens result = b.build();
class SemanticTokensBuilder {
public
  void push(uint32_t line, uint32_t startChar, uint32_t length,
            uint32_t tokenType, uint32_t tokenModifiers = 0) {
    if (length == 0) {
      return;
    }
    tokens_.push_back({line, startChar, length, tokenType, tokenModifiers});
  }

  void reserve(std::size_t n) { tokens_.reserve(n); }

  std::size_t size() const { return tokens_.size(); }
  bool empty() const { return tokens_.empty(); }
  void clear() { tokens_.clear(); }

  SemanticTokens build() {
    std::sort(tokens_.begin(), tokens_.end(),
              [](const Token& a, const Token& b) {
                if (a.line != b.line)
                  return a.line < b.line;
                return a.startChar < b.startChar;
              });

    SemanticTokens out;
    out.data.reserve(tokens_.size() * 5);

    uint32_t prevLine = 0;
    uint32_t prevChar = 0;
    bool first = true;
    for (const auto& t : tokens_) {
      uint32_t deltaLine = first ? t.line : t.line - prevLine;
      uint32_t deltaChar =
          (first || deltaLine != 0) ? t.startChar : t.startChar - prevChar;
      out.data.push_back(deltaLine);
      out.data.push_back(deltaChar);
      out.data.push_back(t.length);
      out.data.push_back(t.tokenType);
      out.data.push_back(t.tokenModifiers);
      prevLine = t.line;
      prevChar = t.startChar;
      first = false;
    }
    return out;
  }

private:
  struct Token {
    uint32_t line;
    uint32_t startChar;
    uint32_t length;
    uint32_t tokenType;
    uint32_t tokenModifiers;
  };
  std::vector<Token> tokens_;
};

} // namespace lsp: