#pragma once

#include <atomic>
#include <clsp/IDocumentStore.hpp>
#include <clsp/ITransport.hpp>
#include <clsp/protocol/Documents.hpp>
#include <clsp/protocol/Lifecycle.hpp>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace lsp {

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

class ILanguageServer {
public:
  explicit ILanguageServer();
  explicit ILanguageServer(std::unique_ptr<ITransport> transport,
                           std::unique_ptr<IDocumentStore> documents);
  explicit ILanguageServer(std::unique_ptr<ITransport> transport);
  virtual ~ILanguageServer() = default;

  ILanguageServer(const ILanguageServer&) = delete;
  ILanguageServer& operator=(const ILanguageServer) = delete;

  int run();

protected:
  using RequestHandler = std::function<nlohmann::json(const nlohmann::json&)>;
  using NotificationHandler = std::function<void(const nlohmann::json&)>;
  using ResponseCallback =
      std::function<void(const nlohmann::json& payload, bool isError)>;

  void registerRequest(const std::string& method, RequestHandler handler);
  void registerNotification(const std::string& method,
                            NotificationHandler handler);

  // Typed overloads: params serialized via to_json, result deserialized via
  // from_json.
  template <class Params>
  void registerNotification(const std::string& method,
                            std::function<void(const Params&)> handler) {
    registerNotification(
        method, [h = std::move(handler)](const nlohmann::json& params) {
          h(params.get<Params>());
        });
  }

  template <class Params, class Result>
  void registerRequest(const std::string& method,
                       std::function<Result(const Params&)> handler) {
    registerRequest(method,
                    [h = std::move(handler)](
                        const nlohmann::json& params) -> nlohmann::json {
                      return nlohmann::json(h(params.get<Params>()));
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
  void registerLifecycleHandlers();
  void registerSyncHandlers();
  void handleRequest(const rpc::ParsedRequest& req);
  void handleNotification(const rpc::ParsedNotification& notif);
  void handleResponse(const rpc::ParsedResponse& resp);

  std::unique_ptr<ITransport> transport_;
  ServerState state_ = ServerState::Uninitialized;
  bool shutdownRequested_ = false;
  TextDocumentSyncKind syncKind_ = TextDocumentSyncKind::None;

  std::unique_ptr<IDocumentStore> documents_;
  std::unordered_map<std::string, RequestHandler> requestHandlers_;
  std::unordered_map<std::string, NotificationHandler> notificationHandlers_;

  std::atomic<int> nextOutgoingId_{1};
  std::unordered_map<int, ResponseCallback> pendingRequests_;
};

} // namespace lsp
