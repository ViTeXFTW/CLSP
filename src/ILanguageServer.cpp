#include "DocumentStore.hpp"
#include "JsonRpc.hpp"
#include "Overloaded.hpp"
#include "StdioTransport.hpp"
#include "clsp/ITransport.hpp"
#include "clsp/protocol/Documents.hpp"
#include <clsp/ILanguageServer.hpp>
#include <clsp/protocol/Sync.hpp>
#include <iostream>

namespace lsp {

ILanguageServer::ILanguageServer(std::unique_ptr<ITransport> transport,
                                 std::unique_ptr<IDocumentStore> documents)
    : transport_(std::move(transport)), documents_(std::move(documents)) {
  registerLifecycleHandlers();
  registerSyncHandlers();
}

ILanguageServer::ILanguageServer(std::unique_ptr<ITransport> transport)
    : ILanguageServer(std::move(transport), std::make_unique<DocumentStore>()) {
}

ILanguageServer::ILanguageServer()
    : ILanguageServer(std::make_unique<StdioTransport>(&std::cin, &std::cout),
                      std::make_unique<DocumentStore>()) {}

int ILanguageServer::run() {
  while (state_ != ServerState::Exited) {
    auto raw = transport_->readMessage();
    if (!raw) {
      break;
    }

    auto parsed = rpc::parseMessage(*raw);

    std::visit(
        overloaded{
            [this](const rpc::ParsedRequest& req) { handleRequest(req); },
            [this](const rpc::ParsedNotification& notif) {
              handleNotification(notif);
            },
            [this](const rpc::ParsedResponse& resp) { handleResponse(resp); },
            [this](const rpc::ParsedError& err) {
              transport_->sendMessage(
                  serializeError(nullptr, err.code, err.message));
            }},
        parsed);
  }

  return shutdownRequested_ ? 0 : 1;
}

void ILanguageServer::onInitialized() {}
void ILanguageServer::onShutdown() {}
void ILanguageServer::onExit() {}

void ILanguageServer::onDocumentOpened(const TextDocumentItem& document) {
  documents_->open(document);
}

void ILanguageServer::onDocumentChanged(
    const TextDocumentItem& document,
    const std::vector<TextDocumentChangeEvent>& changes) {
  documents_->applyChange(
      VersionedTextDocumentIdentifier{document.uri, document.version}, changes);
}
void ILanguageServer::onDocumentClosed(const DocumentUri& uri) {
  documents_->close(uri);
}
void ILanguageServer::onDocumentSaved(const TextDocumentItem&,
                                      const std::optional<std::string>&) {}

void ILanguageServer::registerLifecycleHandlers() {
  requestHandlers_["initialize"] =
      [this](const nlohmann::json& params) -> nlohmann::json {
    state_ = ServerState::Initializing;
    auto result = onInitialize(params.get<InitializeParams>());
    if (result.capabilities.textDocumentSync) {
      syncKind_ = *result.capabilities.textDocumentSync;
    }
    return result;
  };

  requestHandlers_["shutdown"] =
      [this](const nlohmann::json&) -> nlohmann::json {
    shutdownRequested_ = true;
    state_ = ServerState::ShuttingDown;
    onShutdown();
    return nullptr;
  };

  notificationHandlers_["initialized"] = [this](const nlohmann::json&) -> void {
    state_ = ServerState::Running;
    onInitialized();
  };

  notificationHandlers_["exit"] = [this](const nlohmann::json&) -> void {
    state_ = ServerState::Exited;
    onExit();
  };
}

void ILanguageServer::sendNotification(const std::string& method,
                                       const nlohmann::json& params) {
  transport_->sendMessage(rpc::serializeNotification(method, params));
}

void ILanguageServer::sendRequest(const std::string& method,
                                  const nlohmann::json& params,
                                  ResponseCallback callback) {
  int id = nextOutgoingId_.fetch_add(1, std::memory_order_relaxed);
  pendingRequests_.emplace(id, std::move(callback));
  transport_->sendMessage(rpc::serializeRequest(id, method, params));
}

void ILanguageServer::handleResponse(const rpc::ParsedResponse& resp) {
  if (!std::holds_alternative<int>(resp.id)) {
    return;
  }
  int id = std::get<int>(resp.id);
  auto it = pendingRequests_.find(id);
  if (it == pendingRequests_.end()) {
    return;
  }
  ResponseCallback cb = std::move(it->second);
  pendingRequests_.erase(it);
  cb(resp.payload, resp.isError);
}

void ILanguageServer::registerSyncHandlers() {
  notificationHandlers_["textDocument/didOpen"] =
      [this](const nlohmann::json& params) {
        auto p = params.get<DidOpenParams>();
        onDocumentOpened(p.textDocument);
      };

  notificationHandlers_["textDocument/didChange"] =
      [this](const nlohmann::json& params) {
        auto p = params.get<DidChangeParams>();
        TextDocumentItem doc;
        doc.uri = p.textDocument.uri;
        doc.version = p.textDocument.version;
        onDocumentChanged(doc, p.contentChanges);
      };

  notificationHandlers_["textDocument/didClose"] =
      [this](const nlohmann::json& params) {
        auto p = params.get<DidCloseParams>();
        onDocumentClosed(p.uri);
      };

  notificationHandlers_["textDocument/didSave"] =
      [this](const nlohmann::json& params) {
        auto p = params.get<DidSaveParams>();
        const auto* doc = documents_->get(p.uri);
        if (doc) {
          onDocumentSaved(*doc, p.text);
        }
      };
}

void ILanguageServer::registerRequest(const std::string& method,
                                      RequestHandler handler) {
  requestHandlers_[method] = std::move(handler);
}

void ILanguageServer::registerNotification(const std::string& method,
                                           NotificationHandler handler) {
  notificationHandlers_[method] = std::move(handler);
}

void ILanguageServer::handleRequest(const rpc::ParsedRequest& req) {
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
  } catch (const rpc::JsonRpcException& e) {
    transport_->sendMessage(rpc::serializeError(req.id, e.code(), e.what()));
  } catch (const std::exception& e) {
    transport_->sendMessage(
        rpc::serializeError(req.id, rpc::ErrorCodes::InternalError, e.what()));
  }
}

void ILanguageServer::handleNotification(const rpc::ParsedNotification& notif) {
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
  } catch (const std::exception&) {
    // Swallow error
  }
}

} // namespace lsp
