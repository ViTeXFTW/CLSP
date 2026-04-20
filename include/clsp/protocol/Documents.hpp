#pragma once

#include <clsp/protocol/Basic.hpp>
#include <cstdint>
#include <optional>
#include <string>

namespace lsp {

struct TextDocumentItem {
  DocumentUri uri;
  std::string languageId;
  int32_t version;
  std::string text;
};

struct VersionedTextDocumentIdentifier {
  DocumentUri uri;
  uint32_t version;
};

struct TextDocumentContentChangeEvent {
  std::optional<Range> range;
  std::string text;
};

} // namespace lsp
