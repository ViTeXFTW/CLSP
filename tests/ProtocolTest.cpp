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
  ServerCapabilities c{};
  c.textDocumentSync = TextDocumentSyncKind::Full;
  auto j = nlohmann::json(c);
  EXPECT_EQ(j["textDocumentSync"], 1);
}

TEST(ServerCapabilities, HoverProviderEmittedOnlyWhenSet) {
  ServerCapabilities c{};
  auto j1 = nlohmann::json(c);
  EXPECT_FALSE(j1.contains("hoverProvider"));

  c.hoverProvider = true;
  auto j2 = nlohmann::json(c);
  EXPECT_TRUE(j2["hoverProvider"].get<bool>());
}

TEST(ServerCapabilities, CompletionOptionsSerialized) {
  ServerCapabilities c{};
  CompletionOptions co{};
  co.triggerCharacters = std::vector<std::string>{".", "->"};
  co.resolveProvider = true;
  c.completionProvider = co;

  auto j = nlohmann::json(c);
  ASSERT_TRUE(j.contains("completionProvider"));
  EXPECT_EQ(j["completionProvider"]["triggerCharacters"][0], ".");
  EXPECT_EQ(j["completionProvider"]["triggerCharacters"][1], "->");
  EXPECT_TRUE(j["completionProvider"]["resolveProvider"].get<bool>());
  EXPECT_FALSE(j["completionProvider"].contains("allCommitCharacters"));
}

TEST(ServerCapabilities, SignatureHelpOptionsSerialized) {
  ServerCapabilities c{};
  SignatureHelpOptions so{};
  so.triggerCharacters = std::vector<std::string>{"(", ","};
  c.signatureHelpProvider = so;

  auto j = nlohmann::json(c);
  EXPECT_EQ(j["signatureHelpProvider"]["triggerCharacters"][0], "(");
  EXPECT_FALSE(j["signatureHelpProvider"].contains("retriggerCharacters"));
}

TEST(ServerCapabilities, CodeActionRenameDiagnosticOptions) {
  ServerCapabilities c{};
  CodeActionOptions ca{};
  ca.codeActionKinds = std::vector<std::string>{"quickfix", "refactor"};
  c.codeActionProvider = ca;

  RenameOptions ro{};
  ro.prepareProvider = true;
  c.renameProvider = ro;

  DiagnosticOptions diag{};
  diag.identifier = "clsp";
  diag.interFileDependencies = true;
  c.diagnosticProvider = diag;

  auto j = nlohmann::json(c);
  EXPECT_EQ(j["codeActionProvider"]["codeActionKinds"][0], "quickfix");
  EXPECT_TRUE(j["renameProvider"]["prepareProvider"].get<bool>());
  EXPECT_EQ(j["diagnosticProvider"]["identifier"], "clsp");
  EXPECT_TRUE(j["diagnosticProvider"]["interFileDependencies"].get<bool>());
  EXPECT_FALSE(j["diagnosticProvider"]["workspaceDiagnostics"].get<bool>());
}

TEST(ServerCapabilities, BoolProvidersAreOptional) {
  ServerCapabilities c{};
  c.definitionProvider = true;
  c.referencesProvider = false;
  c.documentSymbolProvider = true;
  c.documentFormattingProvider = false;

  auto j = nlohmann::json(c);
  EXPECT_TRUE(j["definitionProvider"].get<bool>());
  EXPECT_FALSE(j["referencesProvider"].get<bool>());
  EXPECT_TRUE(j["documentSymbolProvider"].get<bool>());
  EXPECT_FALSE(j["documentFormattingProvider"].get<bool>());
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
