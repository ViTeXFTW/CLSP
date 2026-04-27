#include <clsp/ILanguageServer.hpp>
#include <clsp/ITransport.hpp>
#include <clsp/protocol/Capabilities.hpp>
#include <clsp/protocol/Lifecycle.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <queue>
#include <string>
#include <vector>

using namespace lsp;

class MockTransport : public ITransport {
public:
  void push(const std::string& msg) { input_.push(msg); }

  const std::vector<std::string>& sent() const { return sent_; }

  std::optional<std::string> readMessage() override {
    if (input_.empty())
      return std::nullopt;
    auto msg = std::move(input_.front());
    input_.pop();
    return msg;
  }

  void sendMessage(const std::string& body) override { sent_.push_back(body); }

private:
  std::queue<std::string> input_;
  std::vector<std::string> sent_;
};

// ── Helpers
// ───────────────────────────────────────────────────────────────────

static std::string request(int id, const std::string& method,
                           nlohmann::json params = nullptr) {
  return nlohmann::json{
      {"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}}
      .dump();
}

static std::string notification(const std::string& method,
                                nlohmann::json params = nullptr) {
  return nlohmann::json{
      {"jsonrpc", "2.0"}, {"method", method}, {"params", params}}
      .dump();
}

static nlohmann::json initParams() {
  return {{"processId", 1}, {"capabilities", nlohmann::json::object()}};
}

// ── MinimalServer
// ─────────────────────────────────────────────────────────────

class MinimalServer : public ILanguageServer {
public:
  using ILanguageServer::ILanguageServer;

  InitializeResult onInitialize(const InitializeParams&) override {
    return InitializeResult{ServerCapabilities{}, std::nullopt};
  }
};

// ── Tests
// ─────────────────────────────────────────────────────────────────────

TEST(Lifecycle, InitializeReturnsResult) {
  auto* t = new MockTransport();
  t->push(request(1, "initialize", initParams()));
  t->push(notification("initialized"));
  t->push(notification("exit"));

  MinimalServer server((std::unique_ptr<ITransport>(t)));
  server.run();

  ASSERT_FALSE(t->sent().empty());
  auto resp = nlohmann::json::parse(t->sent()[0]);
  EXPECT_EQ(resp["id"], 1);
  EXPECT_TRUE(resp.contains("result"));
  EXPECT_TRUE(resp["result"].contains("capabilities"));
}

TEST(Lifecycle, RequestBeforeInitializeIsRejected) {
  auto* t = new MockTransport();
  t->push(request(1, "textDocument/hover", {}));
  t->push(notification("exit"));

  MinimalServer server((std::unique_ptr<ITransport>(t)));
  server.run();

  ASSERT_FALSE(t->sent().empty());
  auto resp = nlohmann::json::parse(t->sent()[0]);
  EXPECT_EQ(resp["error"]["code"], -32002); // ServerNotInitialized
}

TEST(Lifecycle, CleanShutdownReturnsZero) {
  auto* t = new MockTransport();
  t->push(request(1, "initialize", initParams()));
  t->push(notification("initialized"));
  t->push(request(2, "shutdown"));
  t->push(notification("exit"));

  MinimalServer server((std::unique_ptr<ITransport>(t)));
  EXPECT_EQ(server.run(), 0);
}

TEST(Lifecycle, ExitWithoutShutdownReturnsOne) {
  auto* t = new MockTransport();
  t->push(notification("exit"));

  MinimalServer server((std::unique_ptr<ITransport>(t)));
  EXPECT_EQ(server.run(), 1);
}

// ── InitializeParams from_json spec compliance
// ────────────────────────────────────────────

TEST(InitializeParams, NullableFieldsParseWithoutThrowing) {
  nlohmann::json j = {{"processId", nullptr},
                      {"rootUri", nullptr},
                      {"rootPath", nullptr},
                      {"trace", "off"},
                      {"capabilities", nlohmann::json::object()}};
  InitializeParams p;
  EXPECT_NO_THROW(from_json(j, p));
  EXPECT_FALSE(p.processId.has_value());
  EXPECT_FALSE(p.rootPath.has_value());
  ASSERT_TRUE(p.trace.has_value());
  EXPECT_EQ(*p.trace, "off");
}

TEST(InitializeParams, IntegerProcessIdAndVerboseTrace) {
  nlohmann::json j = {{"processId", 12345},
                      {"trace", "verbose"},
                      {"capabilities", nlohmann::json::object()}};
  InitializeParams p;
  EXPECT_NO_THROW(from_json(j, p));
  ASSERT_TRUE(p.processId.has_value());
  EXPECT_EQ(*p.processId, 12345);
  ASSERT_TRUE(p.trace.has_value());
  EXPECT_EQ(*p.trace, "verbose");
}

TEST(InitializeParams, MissingTraceIsAbsent) {
  nlohmann::json j = {{"processId", 1},
                      {"capabilities", nlohmann::json::object()}};
  InitializeParams p;
  EXPECT_NO_THROW(from_json(j, p));
  EXPECT_FALSE(p.trace.has_value());
}

TEST(InitializeParams, TraceOffIsStored) {
  nlohmann::json j = {{"processId", 1},
                      {"trace", "off"},
                      {"capabilities", nlohmann::json::object()}};
  InitializeParams p;
  EXPECT_NO_THROW(from_json(j, p));
  ASSERT_TRUE(p.trace.has_value());
  EXPECT_EQ(*p.trace, "off");
}

TEST(InitializeParams, RootPathStringIsStored) {
  nlohmann::json j = {{"processId", 1},
                      {"rootPath", "/some/path"},
                      {"capabilities", nlohmann::json::object()}};
  InitializeParams p;
  EXPECT_NO_THROW(from_json(j, p));
  ASSERT_TRUE(p.rootPath.has_value());
  EXPECT_EQ(*p.rootPath, "/some/path");
}

TEST(InitializeParams, RootPathNullIsAbsent) {
  nlohmann::json j = {{"processId", 1},
                      {"rootPath", nullptr},
                      {"capabilities", nlohmann::json::object()}};
  InitializeParams p;
  EXPECT_NO_THROW(from_json(j, p));
  EXPECT_FALSE(p.rootPath.has_value());
}
