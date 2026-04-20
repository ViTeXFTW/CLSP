#pragma once

#include <clsp/protocol/Capabilities.hpp>
#include <clsp/protocol/Documents.hpp>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace lsp {

class IDocumentStore {
public:
  virtual ~IDocumentStore() = default;

  virtual const TextDocumentItem &open(const TextDocumentItem &item) = 0;
  virtual TextDocumentItem *
  applyChange(const VersionedTextDocumentIdentifier &id,
              const std::vector<TextDocumentContentChangeEvent> &changes) = 0;

  virtual bool close(const DocumentUri &uri) = 0;
  virtual const TextDocumentItem *get(const DocumentUri &uri) const = 0;
  virtual size_t size() const = 0;
  virtual bool contains(const DocumentUri &uri) const = 0;
  virtual std::vector<DocumentUri> uris() const = 0;

protected:
  virtual size_t positionToOffset(const std::string &text, const Position &pos) = 0;

  std::unordered_map<DocumentUri, TextDocumentItem> documents_;
  TextDocumentSyncKind syncKind_ = TextDocumentSyncKind::None;
};

} // namespace lsp
