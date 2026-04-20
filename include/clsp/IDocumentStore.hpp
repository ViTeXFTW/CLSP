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

  virtual const TextDocumentItem &open(const TextDocumentItem &item);
  virtual TextDocumentItem *
  applyChange(const VersionedTextDocumentIdentifier &id,
              const std::vector<TextDocumentContentChangeEvent> &changes);

  virtual bool close(const DocumentUri &uri);
  virtual const TextDocumentItem *get(const DocumentUri &uri) const;
  virtual size_t size() const;
  virtual bool contains(const DocumentUri &uri) const;

  std::vector<DocumentUri> uris() const;

protected:
  virtual size_t positionToOffset(const std::string &text, const Position &pos);

  std::unordered_map<DocumentUri, TextDocumentItem> documents_;
  TextDocumentSyncKind syncKind_ = TextDocumentSyncKind::None;
};

} // namespace lsp
