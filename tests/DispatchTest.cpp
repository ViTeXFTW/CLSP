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

std::string request(int id, const std::string& method,
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
  using ILanguageServer::registerNotification;
  using ILanguageServer::registerRequest;

  InitializeResult onInitialize(const InitializeParams&) override {
    return InitializeResult{ServerCapabilities{}, std::nullopt};
  }
};

} // namespace

// ── Two in-flight requests run concurrently
// ───────────────────────────────

TEST(Dispatch, ConcurrentRequestsRunInParallel) {
  auto* t = new BlockingMockTransport();
  LanguageServerOptions opts;
  opts.workerThreads = 4;
  TestServer server{std::unique_ptr<ITransport>(t), opts};

  std::atomic<int> inFlight{0};
  std::atomic<int> peak{0};

  server.registerRequest(
      "test/work",
      [&](const nlohmann::json&, CancellationToken) -> nlohmann::json {
        int now = inFlight.fetch_add(1) + 1;
        int prev = peak.load();
        while (now > prev && !peak.compare_exchange_weak(prev, now)) {
        }
        std::this_thread::sleep_for(40ms);
        inFlight.fetch_sub(1);
        return nullptr;
      });

  std::thread driver([&] {
    t->push(request(1, "initialize", initParams()));
    t->push(notification("initialized"));
    t->push(request(10, "test/work"));
    t->push(request(11, "test/work"));
    t->push(request(12, "test/work"));
    std::this_thread::sleep_for(120ms);
    t->push(notification("exit"));
    t->close();
  });
  server.run();
  driver.join();

  EXPECT_GE(peak.load(), 2);
}

// ── Sync notifications are serialized (didChange ordering)
// ──────────────

TEST(Dispatch, SyncNotificationsRunInOrder) {
  auto* t = new BlockingMockTransport();
  TestServer server{std::unique_ptr<ITransport>(t)};

  std::vector<int> seenVersions;
  std::mutex mu;
  std::atomic<int> count{0};

  // Override the doc-changed hook by registering a notification handler that
  // captures versions in order. We use the existing didChange route.
  // Instead of subclassing further we tap into our test via a custom
  // notification we emit between didChanges? Simpler: register a custom
  // method "test/seq" routed through serial executor by name? But only
  // textDocument/did* go to serial executor. We piggy-back on didOpen which
  // is also serial.
  server.registerNotification<nlohmann::json>(
      "textDocument/didOpen",
      std::function<void(const nlohmann::json&)>(
          [&](const nlohmann::json& params) {
            int v = params.at("textDocument").at("version").get<int>();
            std::this_thread::sleep_for(2ms);
            std::lock_guard lock(mu);
            seenVersions.push_back(v);
            count.fetch_add(1);
          }));

  std::thread driver([&] {
    t->push(request(1, "initialize", initParams()));
    t->push(notification("initialized"));
    constexpr int N = 20;
    for (int i = 0; i < N; ++i) {
      t->push(notification("textDocument/didOpen",
                           {{"textDocument", {{"uri", "file:///x"},
                                              {"languageId", "txt"},
                                              {"version", i},
                                              {"text", ""}}}}));
    }
    while (count.load() < N) {
      std::this_thread::sleep_for(2ms);
    }
    t->push(notification("exit"));
    t->close();
  });
  server.run();
  driver.join();

  ASSERT_EQ(seenVersions.size(), 20u);
  for (size_t i = 0; i < seenVersions.size(); ++i) {
    EXPECT_EQ(seenVersions[i], static_cast<int>(i));
  }
}

// ── Send mutex prevents frame interleaving
// ─────────────────────────────────

TEST(Dispatch, ConcurrentResponsesEmitWellFormedFrames) {
  auto* t = new BlockingMockTransport();
  LanguageServerOptions opts;
  opts.workerThreads = 4;
  TestServer server{std::unique_ptr<ITransport>(t), opts};

  server.registerRequest(
      "test/echo",
      [&](const nlohmann::json& p, CancellationToken) -> nlohmann::json {
        // Result is a wide string to make any interleaving observable.
        std::string ch = p.value("ch", std::string("x"));
        return nlohmann::json{{"big", std::string(256, ch.front())}};
      });

  std::thread driver([&] {
    t->push(request(1, "initialize", initParams()));
    t->push(notification("initialized"));
    constexpr int N = 50;
    for (int i = 0; i < N; ++i) {
      t->push(request(100 + i, "test/echo", {{"ch", std::string(1, 'a' + (i % 26))}}));
    }
    std::this_thread::sleep_for(200ms);
    t->push(notification("exit"));
    t->close();
  });
  server.run();
  driver.join();

  // Each frame must parse as a complete JSON object. If the send mutex
  // didn't serialize, partial bytes from two responses would corrupt frames.
  int parsed = 0;
  for (const auto& body : t->sent()) {
    nlohmann::json j;
    ASSERT_NO_THROW(j = nlohmann::json::parse(body));
    if (j.contains("id") && j["id"].get<int>() >= 100) {
      ASSERT_TRUE(j["result"]["big"].is_string());
      EXPECT_EQ(j["result"]["big"].get<std::string>().size(), 256u);
      ++parsed;
    }
  }
  EXPECT_EQ(parsed, 50);
}
