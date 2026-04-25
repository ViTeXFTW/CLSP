#pragma once

#include <clsp/IDocumentStore.hpp>
#include <clsp/protocol/Basic.hpp>
#include <clsp/protocol/Documents.hpp>
#include <cstddef>
#include <shared_mutex>
#include <vector>

namespace lsp {

class DocumentStore : public IDocumentStore {
public:
  const TextDocumentItem& open(const TextDocumentItem& item) override;
  virtual TextDocumentItem*
  applyChange(const VersionedTextDocumentIdentifier& id,
              const std::vector<TextDocumentChangeEvent>& changes) override;
  virtual bool close(const DocumentUri& uri) override;
  virtual const TextDocumentItem* get(const DocumentUri& uri) const override;
  virtual size_t size() const override;
  virtual bool contains(const DocumentUri& uri) const override;
  std::vector<DocumentUri> uris() const override;

protected:
  virtual size_t positionToOffset(const std::string& text,
                                  const Position& pos) override;

private:
  mutable std::shared_mutex mu_;
};

} // namespace lsp
