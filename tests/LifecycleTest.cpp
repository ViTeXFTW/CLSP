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
