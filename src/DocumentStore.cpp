#include "DocumentStore.hpp"

namespace lsp {

const TextDocumentItem& DocumentStore::open(const TextDocumentItem& item) {
  auto [it, _] = documents_.insert_or_assign(
      item.uri,
      TextDocumentItem{item.uri, item.languageId, item.version, item.text});
  return it->second;
}

TextDocumentItem* DocumentStore::applyChange(
    const VersionedTextDocumentIdentifier& id,
    const std::vector<TextDocumentContentChangeEvent>& changes) {
  auto it = documents_.find(id.uri);
  if (it == documents_.end()) {
    return nullptr;
  }
  for (const auto& change : changes) {
    if (change.range) {
      auto start = positionToOffset(it->second.text, change.range->start);
      auto end = positionToOffset(it->second.text, change.range->end);
      it->second.text.replace(start, end - start, change.text);
    } else {
      // No range means full content replacement
      it->second.text = change.text;
    }
  }
  it->second.version = id.version;
  return &it->second;
}

size_t DocumentStore::positionToOffset(const std::string& text,
                                       const Position& pos) {
  size_t offset = 0;
  uint32_t line = 0;
  while (line < pos.line && offset < text.size()) {
    if (text[offset] == '\n') {
      ++line;
    }
    ++offset;
  }
  return offset + pos.character;
}

bool DocumentStore::close(const DocumentUri& uri) {
  return documents_.erase(uri) > 0;
}

const TextDocumentItem* DocumentStore::get(const DocumentUri& uri) const {
  auto it = documents_.find(uri);
  return it != documents_.end() ? &it->second : nullptr;
}

size_t DocumentStore::size() const { return documents_.size(); }

bool DocumentStore::contains(const DocumentUri& uri) const {
  return documents_.contains(uri);
}

std::vector<DocumentUri> DocumentStore::uris() const {
  std::vector<DocumentUri> result;
  result.reserve(documents_.size());
  for (const auto& [uri, _] : documents_) {
    result.push_back(uri);
  }
  return result;
}

} // namespace lsp
