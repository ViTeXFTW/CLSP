#include "JsonRpc.hpp"
#include "Overloaded.hpp"
#include <clsp/ILanguageServer.hpp>

namespace lsp {

ILanguageServer::ILanguageServer(std::unique_ptr<ITransport> transport)
    : transport_(std::move(transport)) {}

int ILanguageServer::run() {
  while (state_ != ServerState::Exited) {
    auto raw = transport_->readMessage();
    if (!raw) {
      break;
    }

    auto parsed = rpc::parseMessage(*raw);

    std::visit(overloaded{[this](const rpc::ParsedRequest &req) {
                            handleRequest(req);
                          },
                          [this](const rpc::ParsedNotification &notif) {
                            handleNotification(notif);
                          },
                          [this](const rpc::ParsedError &err) {
                            transport_->sendMessage(
                                serializeError(nullptr, err.code, err.message));
                          }},
               parsed);
  }

  return shutdownRequested_ ? 0 : 1;
}

void ILanguageServer::registerLifecycleHandlers() {
  requestHandlers_["initialize"] =
      [this](const nlohmann::json &params) -> nlohmann::json {
    state_ = ServerState::Initializing;
    auto result = onInitialize(params.get<InitializeParams>());
    if (result.capabilities.textDocumentSync) {
      syncKind_ = *result.capabilities.textDocumentSync;
    }
    return result;
  };

  requestHandlers_["shutdown"] =
      [this](const nlohmann::json &) -> nlohmann::json {
    shutdownRequested_ = true;
    state_ = ServerState::ShuttingDown;
    onShutdown();
    return nullptr;
  };

  notificationHandlers_["initialized"] =
      [this](const nlohmann::json &) -> void {
    state_ = ServerState::Running;
    onInitialized();
  };

  notificationHandlers_["exit"] = [this](const nlohmann::json &) -> void {
    state_ = ServerState::Exited;
    onExit();
  };
}

void ILanguageServer::registerRequest(const std::string &method,
                                      RequestHandler handler) {
  requestHandlers_[method] = std::move(handler);
}

void ILanguageServer::registerNotification(const std::string &method,
                                           NotificationHandler handler) {
  notificationHandlers_[method] = std::move(handler);
}

void ILanguageServer::handleRequest(const rpc::ParsedRequest &req) {
  if (state_ == ServerState::Uninitialized && req.method != "initialize") {
    transport_->sendMessage(
        rpc::serializeError(req.id, rpc::ErrorCodes::ServerNotInitialized,
                            "Server not initialized"));
    return;
  }

  if (state_ == ServerState::ShuttingDown) {
    transport_->sendMessage(rpc::serializeError(
        req.id, rpc::ErrorCodes::InvalidRequest, "Server is huttting down"));
    return;
  }

  if (state_ != ServerState::Uninitialized && req.method == "initialize") {
    transport_->sendMessage(rpc::serializeError(
        req.id, rpc::ErrorCodes::InvalidRequest, "Server already initialized"));
    return;
  }

  auto reqIter = requestHandlers_.find(req.method);
  if (reqIter == requestHandlers_.end()) {
    transport_->sendMessage(
        rpc::serializeError(req.id, rpc::ErrorCodes::MethodNotFound,
                            "Method not found: " + req.method));
    return;
  }

  try {
    nlohmann::json result = reqIter->second(req.params);
    transport_->sendMessage(rpc::serializeResult(req.id, result));
  } catch (const rpc::JsonRpcException &e) {
    transport_->sendMessage(rpc::serializeError(req.id, e.code(), e.what()));
  } catch (const std::exception &e) {
    transport_->sendMessage(
        rpc::serializeError(req.id, rpc::ErrorCodes::InternalError, e.what()));
  }
}

void ILanguageServer::handleNotification(const rpc::ParsedNotification &notif) {
  if (notif.method == "exit") {
    state_ = ServerState::Exited;
    onExit();
    return;
  }

  if (state_ == ServerState::Uninitialized) {
    return;
  }

  auto notifIter = notificationHandlers_.find(notif.method);
  if (notifIter == notificationHandlers_.end()) {
    return;
  }

  try {
    notifIter->second(notif.params);
  } catch (const std::exception &) {
    // Swallow error
  }
}

} // namespace lsp
