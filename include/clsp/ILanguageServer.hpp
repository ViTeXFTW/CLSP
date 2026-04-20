#pragma once

#include <clsp/ITransport.hpp>
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
  Initialized,
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

  virtual void onInitialized();
  virtual void onShutdown();
  virtual void onExit();

private:
  void handleRequest(const rpc::ParsedRequest &req);
  void handleNotification(const rpc::ParsedNotification &notif);

  std::unique_ptr<ITransport> transport_;
  ServerState state_ = ServerState::Uninitialized;
  bool shutdownRequested_ = false;

  std::unordered_map<std::string, RequestHandler> requestHandlers_;
  std::unordered_map<std::string, NotificationHandler> notificationHandlers_;
};

} // namespace lsp
