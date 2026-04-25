#pragma once

#include <atomic>
#include <clsp/IDocumentStore.hpp>
#include <clsp/ITransport.hpp>
#include <clsp/protocol/Diagnostics.hpp>
#include <clsp/protocol/Documents.hpp>
#include <clsp/protocol/Lifecycle.hpp>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>

namespace lsp {

class ThreadPool;

namespace rpc {
struct ParsedRequest;
struct ParsedNotification;
struct ParsedResponse;
} // namespace rpc

enum class ServerState {
  Uninitialized,
  Initializing,
  Running,
  ShuttingDown,
  Exited
};

/**
 * Cooperative cancellation flag shared between the dispatcher and a request
 * handler. Handlers should poll `isCancelled()` at checkpoints and throw
 * `RequestCancelled` to abort.
 */
class CancellationToken {
public:
  CancellationToken()
      : state_(std::make_shared<std::atomic<bool>>(false)) {}

  bool isCancelled() const noexcept {
    return state_->load(std::memory_order_acquire);
  }

  void cancel() noexcept {
    state_->store(true, std::memory_order_release);
  }

private:
  std::shared_ptr<std::atomic<bool>> state_;
};

/**
 * Throw from a request handler to short-circuit with the LSP-defined
 * `RequestCancelled` error code.
 */
class RequestCancelled : public std::runtime_error {
public:
  RequestCancelled() : std::runtime_error("Request cancelled") {}
};

struct LanguageServerOptions {
  std::size_t workerThreads = 0; // 0 → ThreadPool::defaultWorkerCount()
};

class ILanguageServer {
public:
  using Options = LanguageServerOptions;

  explicit ILanguageServer();
  explicit ILanguageServer(std::unique_ptr<ITransport> transport,
                           std::unique_ptr<IDocumentStore> documents,
                           LanguageServerOptions options = {});
  explicit ILanguageServer(std::unique_ptr<ITransport> transport,
                           LanguageServerOptions options = {});
  virtual ~ILanguageServer();

  ILanguageServer(const ILanguageServer&) = delete;
  ILanguageServer& operator=(const ILanguageServer) = delete;

  int run();

protected:
  using RequestHandler =
      std::function<nlohmann::json(const nlohmann::json&, CancellationToken)>;
  using NotificationHandler = std::function<void(const nlohmann::json&)>;
  using ResponseCallback =
      std::function<void(const nlohmann::json& payload, bool isError)>;

  void registerRequest(const std::string& method, RequestHandler handler);
  void registerNotification(const std::string& method,
                            NotificationHandler handler);

  // Typed overloads: params serialized via to_json, result deserialized via
  // from_json. `void` results return JSON null.
  template <class Params>
  void registerNotification(const std::string& method,
                            std::function<void(const Params&)> handler) {
    registerNotification(
        method, [h = std::move(handler)](const nlohmann::json& params) {
          h(params.get<Params>());
        });
  }

  template <class Params, class Result>
  void registerRequest(
      const std::string& method,
      std::function<Result(const Params&, CancellationToken)> handler) {
    registerRequest(method,
                    [h = std::move(handler)](const nlohmann::json& params,
                                             CancellationToken token)
                        -> nlohmann::json {
                      if constexpr (std::is_void_v<Result>) {
                        h(params.get<Params>(), token);
                        return nullptr;
                      } else {
                        return nlohmann::json(h(params.get<Params>(), token));
                      }
                    });
  }

  void sendNotification(const std::string& method,
                        const nlohmann::json& params = nullptr);
  void sendRequest(const std::string& method, const nlohmann::json& params,
                   ResponseCallback callback);

  template <class Params>
  void sendNotification(const std::string& method, const Params& params) {
    sendNotification(method, nlohmann::json(params));
  }

  template <class Params, class Result>
  void sendRequest(const std::string& method, const Params& params,
                   std::function<void(const Result&)> callback) {
    sendRequest(method, nlohmann::json(params),
                [cb = std::move(callback)](const nlohmann::json& payload,
                                           bool isError) {
                  if (!isError) {
                    cb(payload.get<Result>());
                  }
                });
  }

  void publishDiagnostics(const DocumentUri& uri,
                          std::vector<Diagnostic> diagnostics,
                          std::optional<int32_t> version = std::nullopt);

  virtual InitializeResult onInitialize(const InitializeParams&) = 0;

  virtual void onInitialized();
  virtual void onShutdown();
  virtual void onExit();

  virtual void onDocumentOpened(const TextDocumentItem& document);
  virtual void
  onDocumentChanged(const TextDocumentItem& document,
                    const std::vector<TextDocumentChangeEvent>& changes);
  virtual void onDocumentClosed(const DocumentUri& uri);
  virtual void onDocumentSaved(const TextDocumentItem& document,
                               const std::optional<std::string>& text);

private:
  using RequestId = std::variant<int, std::string>;

  void registerLifecycleHandlers();
  void registerSyncHandlers();
  void registerCancellationHandler();

  void handleRequest(const rpc::ParsedRequest& req);
  void handleNotification(const rpc::ParsedNotification& notif);
  void handleResponse(const rpc::ParsedResponse& resp);

  void runRequestHandler(const RequestId& id, const std::string& method,
                         const nlohmann::json& params,
                         const RequestHandler& handler,
                         CancellationToken token);
  void sendFrame(const std::string& body);

  static bool isLifecycleRequest(const std::string& method);
  static bool isInlineNotification(const std::string& method);
  static bool isSyncNotification(const std::string& method);

  std::unique_ptr<ITransport> transport_;
  ServerState state_ = ServerState::Uninitialized;
  bool shutdownRequested_ = false;
  TextDocumentSyncKind syncKind_ = TextDocumentSyncKind::None;

  std::unique_ptr<IDocumentStore> documents_;
  std::unordered_map<std::string, RequestHandler> requestHandlers_;
  std::unordered_map<std::string, NotificationHandler> notificationHandlers_;

  std::unique_ptr<ThreadPool> workerPool_;
  std::unique_ptr<ThreadPool> syncExecutor_;

  std::mutex sendMutex_;

  std::mutex pendingMutex_;
  std::atomic<int> nextOutgoingId_{1};
  std::unordered_map<int, ResponseCallback> pendingRequests_;

  std::mutex cancellationMutex_;
  std::map<RequestId, CancellationToken> cancellationTokens_;
};

} // namespace lsp
