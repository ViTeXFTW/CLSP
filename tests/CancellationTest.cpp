#include <atomic>
#include <chrono>
#include <clsp/ILanguageServer.hpp>
#include <clsp/ITransport.hpp>
#include <clsp/protocol/Capabilities.hpp>
#include <clsp/protocol/Lifecycle.hpp>
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

using namespace lsp;
using namespace std::chrono_literals;

// MockTransport that lets the test feed messages dynamically (after run()
// has already started). Reads block until a message is available or close()
// is called.
class BlockingMockTransport : public ITransport {
public:
  void push(const std::string& msg) {
    {
      std::lock_guard lock(mu_);
      input_.push(msg);
    }
    cv_.notify_one();
  }

  void close() {
    {
      std::lock_guard lock(mu_);
      closed_ = true;
    }
    cv_.notify_all();
  }

  std::vector<std::string> sent() {
    std::lock_guard lock(sentMu_);
    return sent_;
  }

  std::optional<std::string> readMessage() override {
    std::unique_lock lock(mu_);
    cv_.wait(lock, [this] { return !input_.empty() || closed_; });
    if (input_.empty())
      return std::nullopt;
    auto m = std::move(input_.front());
    input_.pop();
    return m;
  }

  void sendMessage(const std::string& body) override {
    std::lock_guard lock(sentMu_);
    sent_.push_back(body);
  }

private:
  std::mutex mu_;
  std::condition_variable cv_;
  std::queue<std::string> input_;
  bool closed_ = false;

  std::mutex sentMu_;
  std::vector<std::string> sent_;
};

namespace {

std::string request(const nlohmann::json& id, const std::string& method,
                    nlohmann::json params = nullptr) {
  return nlohmann::json{
      {"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}}
      .dump();
}

std::string notification(const std::string& method,
                         nlohmann::json params = nullptr) {
  return nlohmann::json{
      {"jsonrpc", "2.0"}, {"method", method}, {"params", params}}
      .dump();
}

nlohmann::json initParams() {
  return {{"processId", 1}, {"capabilities", nlohmann::json::object()}};
}

class TestServer : public ILanguageServer {
public:
  using ILanguageServer::ILanguageServer;
  using ILanguageServer::registerRequest;

  InitializeResult onInitialize(const InitializeParams&) override {
    return InitializeResult{ServerCapabilities{}, std::nullopt};
  }
};

} // namespace

// ── Cancellation: handler observes flipped token
// ───────────────────────────

TEST(Cancellation, HandlerObservesCancelFlag) {
  auto* t = new BlockingMockTransport();
  TestServer server{std::unique_ptr<ITransport>(t)};

  std::atomic<bool> handlerStarted{false};
  std::atomic<bool> handlerSawCancel{false};

  server.registerRequest(
      "test/slow",
      [&](const nlohmann::json&, CancellationToken token) -> nlohmann::json {
        handlerStarted = true;
        for (int i = 0; i < 200; ++i) {
          if (token.isCancelled()) {
            handlerSawCancel = true;
            throw RequestCancelled{};
          }
          std::this_thread::sleep_for(5ms);
        }
        return nullptr;
      });

  std::thread driver([&] {
    t->push(request(1, "initialize", initParams()));
    t->push(notification("initialized"));
    t->push(request(7, "test/slow"));
    while (!handlerStarted.load()) {
      std::this_thread::sleep_for(2ms);
    }
    t->push(notification("$/cancelRequest", {{"id", 7}}));
    while (!handlerSawCancel.load()) {
      std::this_thread::sleep_for(2ms);
    }
    t->push(notification("exit"));
    t->close();
  });

  server.run();
  driver.join();

  EXPECT_TRUE(handlerSawCancel.load());

  bool foundCancelResponse = false;
  for (const auto& body : t->sent()) {
    auto j = nlohmann::json::parse(body);
    if (j.contains("id") && j["id"] == 7 && j.contains("error")) {
      EXPECT_EQ(j["error"]["code"], -32800); // RequestCancelled
      foundCancelResponse = true;
    }
  }
  EXPECT_TRUE(foundCancelResponse);
}

// ── Cancellation: cancel arrives before handler starts
// ──────────────────────

TEST(Cancellation, CancelBeforeStartStillReturnsCancelledError) {
  auto* t = new BlockingMockTransport();
  // Single worker so we can guarantee request is queued (not yet running).
  LanguageServerOptions opts;
  opts.workerThreads = 1;
  TestServer server{std::unique_ptr<ITransport>(t), opts};

  std::atomic<bool> firstRunning{false};
  std::atomic<bool> firstCanRelease{false};
  std::atomic<bool> secondHandlerEntered{false};

  // First handler: blocks the single worker thread until released.
  server.registerRequest(
      "test/block",
      [&](const nlohmann::json&, CancellationToken) -> nlohmann::json {
        firstRunning = true;
        while (!firstCanRelease.load()) {
          std::this_thread::sleep_for(2ms);
        }
        return nullptr;
      });
  // Second handler: signals when entered. If pre-cancelled it should be
  // short-circuited and never enter.
  server.registerRequest(
      "test/second",
      [&](const nlohmann::json&, CancellationToken) -> nlohmann::json {
        secondHandlerEntered = true;
        return nullptr;
      });

  std::thread driver([&] {
    t->push(request(1, "initialize", initParams()));
    t->push(notification("initialized"));
    t->push(request(10, "test/block"));
    while (!firstRunning.load()) {
      std::this_thread::sleep_for(2ms);
    }
    // While worker is busy, queue request 11 then cancel it.
    t->push(request(11, "test/second"));
    std::this_thread::sleep_for(20ms); // allow handleRequest to register token
    t->push(notification("$/cancelRequest", {{"id", 11}}));
    std::this_thread::sleep_for(20ms);
    firstCanRelease = true;
    std::this_thread::sleep_for(50ms);
    t->push(notification("exit"));
    t->close();
  });

  server.run();
  driver.join();

  EXPECT_FALSE(secondHandlerEntered.load());

  bool found = false;
  for (const auto& body : t->sent()) {
    auto j = nlohmann::json::parse(body);
    if (j.contains("id") && j["id"] == 11 && j.contains("error")) {
      EXPECT_EQ(j["error"]["code"], -32800);
      found = true;
    }
  }
  EXPECT_TRUE(found);
}

// ── Cancellation: unknown id is a silent no-op
// ────────────────────────────

TEST(Cancellation, UnknownIdIsIgnored) {
  auto* t = new BlockingMockTransport();
  TestServer server{std::unique_ptr<ITransport>(t)};

  std::thread driver([&] {
    t->push(request(1, "initialize", initParams()));
    t->push(notification("initialized"));
    t->push(notification("$/cancelRequest", {{"id", 9999}}));
    t->push(notification("exit"));
    t->close();
  });
  server.run();
  driver.join();
  // Just a smoke test that nothing crashes / no extra error frame is sent.
  for (const auto& body : t->sent()) {
    auto j = nlohmann::json::parse(body);
    if (j.contains("error")) {
      EXPECT_NE(j["error"]["code"], -32800);
    }
  }
}
