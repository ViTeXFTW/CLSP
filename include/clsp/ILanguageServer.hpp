#pragma once

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
  explicit ILanguageServer(std::unique_ptr<ITransport> transport);
  virtual ~ILanguageServer() = default;

  ILanguageServer(const ILanguageServer &) = delete;
  ILanguageServer &operator=(const ILanguageServer) = delete;

  int run();

protected:
  using RequestHandler = std::function<nlohmann::json(const nlohmann::json &)>;
  using NotificationHandler = std::function<void(const nlohmann::json &)>;

  void registerRequest(const std::string &method, RequestHandler handler);
  void registerNotification(const std::string &method,
                            NotificationHandler handler);

  virtual InitializeResult onInitialize(const InitializeParams &) = 0;

  virtual void onInitialized();
  virtual void onShutdown();
  virtual void onExit();

  virtual void onDocumentOpened(const TextDocumentItem &document);
  virtual void
  onDocumentChanged(const TextDocumentItem &document,
                    const std::vector<TextDocumentContentChangeEvent> &changes);
  virtual void onDocumentClosed(const DocumentUri &uri);
  virtual void onDocumentSaved(const TextDocumentItem &document,
                               const std::optional<std::string> &text);

private:
  void registerLifecycleHandlers();
  void handleRequest(const rpc::ParsedRequest &req);
  void handleNotification(const rpc::ParsedNotification &notif);

  std::unique_ptr<ITransport> transport_;
  ServerState state_ = ServerState::Uninitialized;
  bool shutdownRequested_ = false;
  TextDocumentSyncKind syncKind_ = TextDocumentSyncKind::None;

  std::unique_ptr<IDocumentStore> documents_;
  std::unordered_map<std::string, RequestHandler> requestHandlers_;
  std::unordered_map<std::string, NotificationHandler> notificationHandlers_;
};

} // namespace lsp
