#include <clsp/protocol/Basic.hpp>
#include <clsp/protocol/Capabilities.hpp>
#include <clsp/protocol/Lifecycle.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace lsp;

// ── Basic types
// ───────────────────────────────────────────────────────────────

TEST(Position, RoundTrip) {
  Position p{3, 10};
  auto p2 = nlohmann::json(p).get<Position>();
  EXPECT_EQ(p2.line, 3u);
  EXPECT_EQ(p2.character, 10u);
}

TEST(Range, RoundTrip) {
  Range r{{0, 0}, {1, 5}};
  auto r2 = nlohmann::json(r).get<Range>();
  EXPECT_EQ(r2.start.line, 0u);
  EXPECT_EQ(r2.end.character, 5u);
}

// ── ServerCapabilities
// ────────────────────────────────────────────────────────

TEST(ServerCapabilities, EmptyCapabilities) {
  ServerCapabilities c{};
  auto j = nlohmann::json(c);
  EXPECT_TRUE(j.is_object());
  EXPECT_FALSE(j.contains("textDocumentSync"));
}

TEST(ServerCapabilities, WithSyncKind) {
  ServerCapabilities c{TextDocumentSyncKind::Full};
  auto j = nlohmann::json(c);
  EXPECT_EQ(j["textDocumentSync"], 1);
}

// ── InitializeParams
// ──────────────────────────────────────────────────────────

TEST(InitializeParams, MinimalDeserialization) {
  auto j = nlohmann::json::parse(R"({
        "processId": 42,
        "capabilities": {},
        "rootUri": null
    })");
  auto p = j.get<InitializeParams>();
  EXPECT_EQ(p.processId, 42);
  EXPECT_FALSE(p.clientInfo.has_value());
  EXPECT_FALSE(p.rootUri.has_value());
}

TEST(InitializeParams, WithClientInfo) {
  auto j = nlohmann::json::parse(R"({
        "processId": 1,
        "clientInfo": {"name": "vscode", "version": "1.80"},
        "capabilities": {}
    })");
  auto p = j.get<InitializeParams>();
  ASSERT_TRUE(p.clientInfo.has_value());
  EXPECT_EQ(p.clientInfo->name, "vscode");
  ASSERT_TRUE(p.clientInfo->version.has_value());
  EXPECT_EQ(*p.clientInfo->version, "1.80");
}

TEST(InitializeParams, ClientInfoWithoutVersion) {
  auto j = nlohmann::json::parse(R"({
        "processId": 1,
        "clientInfo": {"name": "helix"},
        "capabilities": {}
    })");
  auto p = j.get<InitializeParams>();
  ASSERT_TRUE(p.clientInfo.has_value());
  EXPECT_FALSE(p.clientInfo->version.has_value());
}

// ── InitializeResult
// ──────────────────────────────────────────────────────────

TEST(InitializeResult, SerializesCapabilities) {
  InitializeResult r;
  r.capabilities.textDocumentSync = TextDocumentSyncKind::Incremental;
  auto j = nlohmann::json(r);
  EXPECT_EQ(j["capabilities"]["textDocumentSync"], 2);
  EXPECT_FALSE(j.contains("serverInfo"));
}

TEST(InitializeResult, WithServerInfo) {
  InitializeResult r;
  r.serverInfo = InitializeResult::ServerInfo{"my-lsp", "0.1"};
  auto j = nlohmann::json(r);
  EXPECT_EQ(j["serverInfo"]["name"], "my-lsp");
  EXPECT_EQ(j["serverInfo"]["version"], "0.1");
}
