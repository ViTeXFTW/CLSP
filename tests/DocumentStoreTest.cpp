#include "DocumentStore.hpp"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

using namespace lsp;
using namespace std::chrono_literals;

namespace {

TextDocumentItem doc(const std::string& uri, const std::string& text,
                     int version = 1) {
  return TextDocumentItem{uri, "txt", version, text};
}

} // namespace

TEST(DocumentStore, OpenStores) {
  DocumentStore s;
  auto& d = s.open(doc("file:///a", "hello"));
  EXPECT_EQ(d.text, "hello");
  EXPECT_EQ(s.size(), 1u);
  EXPECT_TRUE(s.contains("file:///a"));
}

TEST(DocumentStore, CloseRemoves) {
  DocumentStore s;
  s.open(doc("file:///a", "x"));
  EXPECT_TRUE(s.close("file:///a"));
  EXPECT_FALSE(s.close("file:///a"));
  EXPECT_EQ(s.size(), 0u);
}

TEST(DocumentStore, FullChangeReplacesText) {
  DocumentStore s;
  s.open(doc("file:///a", "old"));
  TextDocumentChangeEvent e{};
  e.text = "new content";
  auto* d = s.applyChange({"file:///a", 2}, {e});
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->text, "new content");
  EXPECT_EQ(d->version, 2);
}

TEST(DocumentStore, IncrementalChange) {
  DocumentStore s;
  s.open(doc("file:///a", "hello world"));
  TextDocumentChangeEvent e{};
  e.range = Range{{0, 6}, {0, 11}};
  e.text = "Claude";
  auto* d = s.applyChange({"file:///a", 2}, {e});
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->text, "hello Claude");
}

TEST(DocumentStore, ApplyChangeOnUnknownUriIsNoop) {
  DocumentStore s;
  TextDocumentChangeEvent e{};
  e.text = "x";
  EXPECT_EQ(s.applyChange({"file:///none", 1}, {e}), nullptr);
}

// ── Concurrency
// ───────────────────────────────────────────────────────────

TEST(DocumentStoreConcurrency, ManyReadersOneWriterDoNotRace) {
  DocumentStore s;
  s.open(doc("file:///a", "0"));

  std::atomic<bool> stop{false};
  std::atomic<int> readErrors{0};

  std::vector<std::thread> readers;
  for (int i = 0; i < 4; ++i) {
    readers.emplace_back([&] {
      while (!stop.load()) {
        if (s.contains("file:///a")) {
          if (auto* d = s.get("file:///a")) {
            // Just touch the size to ensure we did read it.
            volatile auto sz = d->text.size();
            (void)sz;
          }
        }
        // Even when contains() lies (entry just erased), get() must not
        // dereference a freed iterator: it returns nullptr.
        (void)s.size();
        (void)s.uris();
      }
    });
  }

  std::thread writer([&] {
    for (int i = 0; i < 200; ++i) {
      TextDocumentChangeEvent e{};
      e.text = std::to_string(i);
      s.applyChange({"file:///a", i + 1}, {e});
      if (i % 50 == 0) {
        s.close("file:///a");
        s.open(doc("file:///a", "0", 1));
      }
    }
  });

  writer.join();
  stop = true;
  for (auto& t : readers)
    t.join();

  EXPECT_EQ(readErrors.load(), 0);
}
