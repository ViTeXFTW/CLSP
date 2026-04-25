#include "DocumentStore.hpp"
#include <mutex>
#include <shared_mutex>

namespace lsp {

// Note on returned pointers: `get` and `applyChange` return raw pointers
// into the store. They are stable while the store is mutated only by the
// serial sync executor (the contract used by ILanguageServer). Callers
// must not retain them across yield points to other writers.

const TextDocumentItem& DocumentStore::open(const TextDocumentItem& item) {
  std::unique_lock lock(mu_);
  auto [it, _] = documents_.insert_or_assign(
      item.uri,
      TextDocumentItem{item.uri, item.languageId, item.version, item.text});
  return it->second;
}

TextDocumentItem* DocumentStore::applyChange(
    const VersionedTextDocumentIdentifier& id,
    const std::vector<TextDocumentChangeEvent>& changes) {
  std::unique_lock lock(mu_);
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

namespace {

// Decode one UTF-8 code point starting at `text[offset]`. Sets `byteLen` to
// the number of bytes consumed (1..4) and `utf16Units` to its UTF-16
// code-unit count (1 for BMP, 2 for surrogate pairs). Malformed sequences
// are treated as a single replacement byte.
void decodeUtf8(const std::string& text, size_t offset, size_t& byteLen,
                size_t& utf16Units) {
  unsigned char b0 = static_cast<unsigned char>(text[offset]);
  if (b0 < 0x80) {
    byteLen = 1;
    utf16Units = 1;
  } else if ((b0 & 0xE0) == 0xC0) {
    byteLen = 2;
    utf16Units = 1;
  } else if ((b0 & 0xF0) == 0xE0) {
    byteLen = 3;
    utf16Units = 1;
  } else if ((b0 & 0xF8) == 0xF0) {
    byteLen = 4;
    utf16Units = 2; // > U+FFFF encoded as surrogate pair in UTF-16
  } else {
    byteLen = 1;
    utf16Units = 1;
  }
  if (offset + byteLen > text.size()) {
    byteLen = text.size() - offset;
    if (byteLen == 0)
      byteLen = 1;
  }
}

} // namespace

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
  if (encoding_ == PositionEncoding::UTF8) {
    return offset + pos.character;
  }

  uint32_t consumed = 0;
  while (consumed < pos.character && offset < text.size() &&
         text[offset] != '\n') {
    size_t byteLen = 1;
    size_t units = 1;
    decodeUtf8(text, offset, byteLen, units);
    if (encoding_ == PositionEncoding::UTF16) {
      consumed += static_cast<uint32_t>(units);
    } else { // UTF32: one code point per character
      consumed += 1;
    }
    offset += byteLen;
  }
  return offset;
}

bool DocumentStore::close(const DocumentUri& uri) {
  std::unique_lock lock(mu_);
  return documents_.erase(uri) > 0;
}

const TextDocumentItem* DocumentStore::get(const DocumentUri& uri) const {
  std::shared_lock lock(mu_);
  auto it = documents_.find(uri);
  return it != documents_.end() ? &it->second : nullptr;
}

size_t DocumentStore::size() const {
  std::shared_lock lock(mu_);
  return documents_.size();
}

bool DocumentStore::contains(const DocumentUri& uri) const {
  std::shared_lock lock(mu_);
  return documents_.contains(uri);
}

std::vector<DocumentUri> DocumentStore::uris() const {
  std::shared_lock lock(mu_);
  std::vector<DocumentUri> result;
  result.reserve(documents_.size());
  for (const auto& [uri, _] : documents_) {
    result.push_back(uri);
  }
  return result;
}

} // namespace lsp
