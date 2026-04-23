#include <clsp/ILanguageServer.hpp>
#include <clsp/ITransport.hpp>
#include <clsp/protocol/Basic.hpp>
#include <clsp/protocol/Capabilities.hpp>
#include <clsp/protocol/Lifecycle.hpp>
#include <functional>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <queue>
#include <string>
#include <vector>

using namespace lsp;

// ── MockTransport
// ─────────────────────────────────────────────────────────────────

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

static std::string response(int id, nlohmann::json result) {
  return nlohmann::json{{"jsonrpc", "2.0"}, {"id", id}, {"result", result}}
      .dump();
}

static std::string errorResponse(int id, int code,
                                 const std::string& message) {
  return nlohmann::json{{"jsonrpc", "2.0"},
                        {"id", id},
                        {"error", {{"code", code}, {"message", message}}}}
      .dump();
}

static nlohmann::json initParams() {
  return {{"processId", 1}, {"capabilities", nlohmann::json::object()}};
}

// ── MessagingServer
// ───────────────────────────────────────────────────────────

class MessagingServer : public ILanguageServer {
public:
  using ILanguageServer::ILanguageServer;
  using ILanguageServer::sendNotification;
  using ILanguageServer::sendRequest;

  std::function<void(MessagingServer&)> onInitializedAction;

  InitializeResult onInitialize(const InitializeParams&) override {
    return InitializeResult{ServerCapabilities{}, std::nullopt};
  }

  void onInitialized() override {
    ILanguageServer::onInitialized();
    if (onInitializedAction)
      onInitializedAction(*this);
  }
};

// ── sendNotification tests
// ────────────────────────────────────────────────

TEST(SendNotification, EmitsCorrectFrame) {
  auto* t = new MockTransport();
  t->push(notification("exit"));

  MessagingServer server{std::unique_ptr<ITransport>(t)};
  server.sendNotification("window/logMessage",
                          {{"type", 3}, {"message", "hello"}});
  server.run();

  ASSERT_GE(t->sent().size(), 1u);
  auto j = nlohmann::json::parse(t->sent()[0]);
  EXPECT_EQ(j["jsonrpc"], "2.0");
  EXPECT_EQ(j["method"], "window/logMessage");
  EXPECT_EQ(j["params"]["message"], "hello");
  EXPECT_FALSE(j.contains("id"));
  EXPECT_FALSE(j.contains("result"));
}

TEST(SendNotification, NullParamsEmitsNullParams) {
  auto* t = new MockTransport();
  t->push(notification("exit"));

  MessagingServer server{std::unique_ptr<ITransport>(t)};
  server.sendNotification("custom/ping");
  server.run();

  ASSERT_GE(t->sent().size(), 1u);
  auto j = nlohmann::json::parse(t->sent()[0]);
  EXPECT_EQ(j["method"], "custom/ping");
  EXPECT_TRUE(j["params"].is_null());
}

TEST(SendNotification, CalledFromHandlerIsDelivered) {
  auto* t = new MockTransport();
  t->push(request(1, "initialize", initParams()));
  t->push(notification("initialized"));
  t->push(notification("exit"));

  MessagingServer server{std::unique_ptr<ITransport>(t)};
  server.onInitializedAction = [](MessagingServer& s) {
    s.sendNotification("textDocument/publishDiagnostics",
                       {{"uri", "file:///test.cpp"},
                        {"diagnostics", nlohmann::json::array()}});
  };
  server.run();

  // sent[0] = initialize response, sent[1] = our notification
  ASSERT_GE(t->sent().size(), 2u);
  auto j = nlohmann::json::parse(t->sent()[1]);
  EXPECT_EQ(j["method"], "textDocument/publishDiagnostics");
  EXPECT_EQ(j["params"]["uri"], "file:///test.cpp");
  EXPECT_TRUE(j["params"]["diagnostics"].is_array());
}

TEST(SendNotification, TypedParamsUseToJson) {
  auto* t = new MockTransport();
  t->push(notification("exit"));

  MessagingServer server{std::unique_ptr<ITransport>(t)};
  Position pos{10, 5};
  server.sendNotification("custom/cursorMoved", pos);
  server.run();

  ASSERT_GE(t->sent().size(), 1u);
  auto j = nlohmann::json::parse(t->sent()[0]);
  EXPECT_EQ(j["params"]["line"], 10u);
  EXPECT_EQ(j["params"]["character"], 5u);
}

// ── sendRequest tests
// ─────────────────────────────────────────────────────────

TEST(SendRequest, EmitsCorrectFrame) {
  auto* t = new MockTransport();
  t->push(notification("exit"));

  MessagingServer server{std::unique_ptr<ITransport>(t)};
  server.sendRequest("client/registerCapability",
                     {{"registrations", nlohmann::json::array()}},
                     [](const nlohmann::json&, bool) {});
  server.run();

  ASSERT_GE(t->sent().size(), 1u);
  auto j = nlohmann::json::parse(t->sent()[0]);
  EXPECT_EQ(j["jsonrpc"], "2.0");
  EXPECT_EQ(j["method"], "client/registerCapability");
  EXPECT_TRUE(j.contains("id"));
  EXPECT_TRUE(j["id"].is_number_integer());
  EXPECT_FALSE(j.contains("result"));
  EXPECT_FALSE(j.contains("error"));
}

TEST(SendRequest, IdsAreMonotonicallyIncreasing) {
  auto* t = new MockTransport();
  t->push(notification("exit"));

  MessagingServer server{std::unique_ptr<ITransport>(t)};
  server.sendRequest("method/one", nullptr, [](const nlohmann::json&, bool) {});
  server.sendRequest("method/two", nullptr, [](const nlohmann::json&, bool) {});
  server.run();

  ASSERT_GE(t->sent().size(), 2u);
  int id1 = nlohmann::json::parse(t->sent()[0])["id"].get<int>();
  int id2 = nlohmann::json::parse(t->sent()[1])["id"].get<int>();
  EXPECT_LT(id1, id2);
  EXPECT_EQ(id2 - id1, 1);
}

TEST(SendRequest, SuccessResponseRoutesToCallback) {
  auto* t = new MockTransport();
  t->push(request(1, "initialize", initParams()));
  t->push(notification("initialized"));
  // Server's sendRequest will claim id=1; pre-push the client's response
  t->push(response(1, {{"applied", true}}));
  t->push(notification("exit"));

  nlohmann::json capturedPayload;
  bool capturedIsError = true;
  bool callbackFired = false;

  MessagingServer server{std::unique_ptr<ITransport>(t)};
  server.onInitializedAction = [&](MessagingServer& s) {
    s.sendRequest("workspace/applyEdit", {{"edit", nlohmann::json::object()}},
                  [&](const nlohmann::json& payload, bool isError) {
                    capturedPayload = payload;
                    capturedIsError = isError;
                    callbackFired = true;
                  });
  };
  server.run();

  EXPECT_TRUE(callbackFired);
  EXPECT_FALSE(capturedIsError);
  EXPECT_EQ(capturedPayload["applied"], true);
}

TEST(SendRequest, ErrorResponseRoutesToCallbackWithIsErrorTrue) {
  auto* t = new MockTransport();
  t->push(request(1, "initialize", initParams()));
  t->push(notification("initialized"));
  t->push(errorResponse(1, -32603, "Internal error"));
  t->push(notification("exit"));

  bool callbackFired = false;
  bool capturedIsError = false;
  std::string capturedMessage;

  MessagingServer server{std::unique_ptr<ITransport>(t)};
  server.onInitializedAction = [&](MessagingServer& s) {
    s.sendRequest("workspace/applyEdit", {},
                  [&](const nlohmann::json& payload, bool isError) {
                    callbackFired = true;
                    capturedIsError = isError;
                    if (isError)
                      capturedMessage = payload["message"].get<std::string>();
                  });
  };
  server.run();

  EXPECT_TRUE(callbackFired);
  EXPECT_TRUE(capturedIsError);
  EXPECT_EQ(capturedMessage, "Internal error");
}

TEST(SendRequest, UnknownResponseIdIsIgnored) {
  auto* t = new MockTransport();
  // Response id=999 has no matching pending request
  t->push(response(999, {{"key", "val"}}));
  t->push(notification("exit"));

  bool callbackFired = false;
  MessagingServer server{std::unique_ptr<ITransport>(t)};
  // This request gets id=1, not id=999
  server.sendRequest("some/method", nullptr,
                     [&](const nlohmann::json&, bool) { callbackFired = true; });
  server.run();

  EXPECT_FALSE(callbackFired);
}

TEST(SendRequest, CallbackCalledExactlyOnce) {
  auto* t = new MockTransport();
  t->push(request(1, "initialize", initParams()));
  t->push(notification("initialized"));
  t->push(response(1, {}));
  t->push(response(1, {})); // duplicate — should be ignored
  t->push(notification("exit"));

  int callCount = 0;
  MessagingServer server{std::unique_ptr<ITransport>(t)};
  server.onInitializedAction = [&](MessagingServer& s) {
    s.sendRequest("some/method", {},
                  [&](const nlohmann::json&, bool) { ++callCount; });
  };
  server.run();

  EXPECT_EQ(callCount, 1);
}

TEST(SendRequest, MultipleInFlightOutOfOrderResponses) {
  auto* t = new MockTransport();
  t->push(request(1, "initialize", initParams()));
  t->push(notification("initialized"));
  // Server sends id=1 then id=2; responses arrive in reverse order
  t->push(response(2, {{"second", true}}));
  t->push(response(1, {{"first", true}}));
  t->push(notification("exit"));

  nlohmann::json payload1, payload2;
  bool got1 = false, got2 = false;

  MessagingServer server{std::unique_ptr<ITransport>(t)};
  server.onInitializedAction = [&](MessagingServer& s) {
    s.sendRequest("method/one", {},
                  [&](const nlohmann::json& p, bool) {
                    payload1 = p;
                    got1 = true;
                  });
    s.sendRequest("method/two", {},
                  [&](const nlohmann::json& p, bool) {
                    payload2 = p;
                    got2 = true;
                  });
  };
  server.run();

  EXPECT_TRUE(got1);
  EXPECT_TRUE(got2);
  EXPECT_EQ(payload1["first"], true);
  EXPECT_EQ(payload2["second"], true);
}

TEST(SendRequest, TypedResultIsDeserialized) {
  auto* t = new MockTransport();
  t->push(request(1, "initialize", initParams()));
  t->push(notification("initialized"));
  t->push(response(1, {{"line", 5}, {"character", 3}}));
  t->push(notification("exit"));

  Position capturedPos{};
  bool callbackFired = false;

  MessagingServer server{std::unique_ptr<ITransport>(t)};
  server.onInitializedAction = [&](MessagingServer& s) {
    s.sendRequest<Position, Position>(
        "custom/positionQuery", Position{0, 0},
        std::function<void(const Position&)>([&](const Position& pos) {
          capturedPos = pos;
          callbackFired = true;
        }));
  };
  server.run();

  EXPECT_TRUE(callbackFired);
  EXPECT_EQ(capturedPos.line, 5u);
  EXPECT_EQ(capturedPos.character, 3u);
}
