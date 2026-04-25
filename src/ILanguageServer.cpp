#include "DocumentStore.hpp"
#include "JsonRpc.hpp"
#include "Overloaded.hpp"
#include "StdioTransport.hpp"
#include "ThreadPool.hpp"
#include "clsp/ITransport.hpp"
#include "clsp/protocol/Documents.hpp"
#include <clsp/ILanguageServer.hpp>
#include <clsp/PositionEncoding.hpp>
#include <clsp/protocol/Sync.hpp>
#include <iostream>

namespace lsp {

ILanguageServer::ILanguageServer(std::unique_ptr<ITransport> transport,
                                 LanguageServerOptions options)
    : ILanguageServer(std::move(transport), std::make_unique<DocumentStore>(),
                      options) {}

ILanguageServer::ILanguageServer(std::unique_ptr<ITransport> transport,
                                 std::unique_ptr<IDocumentStore> documents,
                                 LanguageServerOptions options)
    : transport_(std::move(transport)), documents_(std::move(documents)),
      workerPool_(std::make_unique<ThreadPool>(
          options.workerThreads != 0 ? options.workerThreads
                                     : ThreadPool::defaultWorkerCount())),
      syncExecutor_(std::make_unique<ThreadPool>(1)) {
  registerLifecycleHandlers();
  registerSyncHandlers();
  registerCancellationHandler();
}

ILanguageServer::ILanguageServer()
    : ILanguageServer(std::make_unique<StdioTransport>(&std::cin, &std::cout),
                      std::make_unique<DocumentStore>(),
                      LanguageServerOptions{}) {}

ILanguageServer::~ILanguageServer() {
  if (workerPool_) {
    workerPool_->shutdown();
  }
  if (syncExecutor_) {
    syncExecutor_->shutdown();
  }
}

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
              sendFrame(rpc::serializeError(nullptr, err.code, err.message));
            }},
        parsed);
  }

  // Drain any in-flight tasks before returning so callbacks/responses fire.
  syncExecutor_->shutdown();
  workerPool_->shutdown();

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
      [this](const nlohmann::json& params, CancellationToken) -> nlohmann::json {
    state_ = ServerState::Initializing;
    auto result = onInitialize(params.get<InitializeParams>());
    if (result.capabilities.textDocumentSync) {
      syncKind_ = *result.capabilities.textDocumentSync;
    }
    if (result.capabilities.positionEncoding) {
      if (auto e = positionEncodingFromString(
              *result.capabilities.positionEncoding)) {
        documents_->setPositionEncoding(*e);
      }
    }
    return result;
  };

  requestHandlers_["shutdown"] =
      [this](const nlohmann::json&, CancellationToken) -> nlohmann::json {
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

void ILanguageServer::registerCancellationHandler() {
  notificationHandlers_["$/cancelRequest"] =
      [this](const nlohmann::json& params) {
        if (!params.is_object() || !params.contains("id")) {
          return;
        }
        RequestId id;
        if (params["id"].is_number_integer()) {
          id = params["id"].get<int>();
        } else if (params["id"].is_string()) {
          id = params["id"].get<std::string>();
        } else {
          return;
        }
        std::lock_guard lock(cancellationMutex_);
        auto it = cancellationTokens_.find(id);
        if (it != cancellationTokens_.end()) {
          it->second.cancel();
        }
      };
}

void ILanguageServer::sendFrame(const std::string& body) {
  std::lock_guard lock(sendMutex_);
  transport_->sendMessage(body);
}

void ILanguageServer::sendNotification(const std::string& method,
                                       const nlohmann::json& params) {
  sendFrame(rpc::serializeNotification(method, params));
}

void ILanguageServer::publishDiagnostics(const DocumentUri& uri,
                                         std::vector<Diagnostic> diagnostics,
                                         std::optional<int32_t> version) {
  PublishDiagnosticsParams p;
  p.uri = uri;
  p.version = version;
  p.diagnostics = std::move(diagnostics);
  sendNotification("textDocument/publishDiagnostics", nlohmann::json(p));
}

void ILanguageServer::sendRequest(const std::string& method,
                                  const nlohmann::json& params,
                                  ResponseCallback callback) {
  int id = nextOutgoingId_.fetch_add(1, std::memory_order_relaxed);
  {
    std::lock_guard lock(pendingMutex_);
    pendingRequests_.emplace(id, std::move(callback));
  }
  sendFrame(rpc::serializeRequest(id, method, params));
}

void ILanguageServer::handleResponse(const rpc::ParsedResponse& resp) {
  if (!std::holds_alternative<int>(resp.id)) {
    return;
  }
  int id = std::get<int>(resp.id);

  ResponseCallback cb;
  {
    std::lock_guard lock(pendingMutex_);
    auto it = pendingRequests_.find(id);
    if (it == pendingRequests_.end()) {
      return;
    }
    cb = std::move(it->second);
    pendingRequests_.erase(it);
  }
  workerPool_->submit(
      [cb = std::move(cb), payload = resp.payload, isError = resp.isError] {
        cb(payload, isError);
      });
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

bool ILanguageServer::isLifecycleRequest(const std::string& method) {
  return method == "initialize" || method == "shutdown";
}

bool ILanguageServer::isInlineNotification(const std::string& method) {
  // These must run on the read thread to keep state transitions and
  // cancellation flips ordered with subsequent dispatch decisions.
  return method == "initialized" || method == "exit" ||
         method == "$/cancelRequest";
}

bool ILanguageServer::isSyncNotification(const std::string& method) {
  return method == "textDocument/didOpen" ||
         method == "textDocument/didChange" ||
         method == "textDocument/didClose" ||
         method == "textDocument/didSave";
}

void ILanguageServer::runRequestHandler(const RequestId& id,
                                        const std::string& /*method*/,
                                        const nlohmann::json& params,
                                        const RequestHandler& handler,
                                        CancellationToken token) {
  try {
    if (token.isCancelled()) {
      sendFrame(rpc::serializeError(id, rpc::ErrorCodes::RequestCancelled,
                                    "Request cancelled"));
      return;
    }
    nlohmann::json result = handler(params, token);
    sendFrame(rpc::serializeResult(id, result));
  } catch (const RequestCancelled&) {
    sendFrame(rpc::serializeError(id, rpc::ErrorCodes::RequestCancelled,
                                  "Request cancelled"));
  } catch (const rpc::JsonRpcException& e) {
    sendFrame(rpc::serializeError(id, e.code(), e.what()));
  } catch (const std::exception& e) {
    sendFrame(
        rpc::serializeError(id, rpc::ErrorCodes::InternalError, e.what()));
  }
}

void ILanguageServer::handleRequest(const rpc::ParsedRequest& req) {
  if (state_ == ServerState::Uninitialized && req.method != "initialize") {
    sendFrame(rpc::serializeError(
        req.id, rpc::ErrorCodes::ServerNotInitialized,
        "Server not initialized"));
    return;
  }

  if (state_ == ServerState::ShuttingDown) {
    sendFrame(rpc::serializeError(req.id, rpc::ErrorCodes::InvalidRequest,
                                  "Server is shutting down"));
    return;
  }

  if (state_ != ServerState::Uninitialized && req.method == "initialize") {
    sendFrame(rpc::serializeError(req.id, rpc::ErrorCodes::InvalidRequest,
                                  "Server already initialized"));
    return;
  }

  auto reqIter = requestHandlers_.find(req.method);
  if (reqIter == requestHandlers_.end()) {
    sendFrame(rpc::serializeError(req.id, rpc::ErrorCodes::MethodNotFound,
                                  "Method not found: " + req.method));
    return;
  }

  if (isLifecycleRequest(req.method)) {
    // Lifecycle requests run inline on the read thread so state transitions
    // are visible before the next message is classified.
    runRequestHandler(req.id, req.method, req.params, reqIter->second,
                      CancellationToken{});
    return;
  }

  CancellationToken token;
  {
    std::lock_guard lock(cancellationMutex_);
    cancellationTokens_[req.id] = token;
  }
  workerPool_->submit([this, id = req.id, method = req.method,
                       params = req.params, handler = reqIter->second,
                       token]() mutable {
    runRequestHandler(id, method, params, handler, token);
    std::lock_guard lock(cancellationMutex_);
    cancellationTokens_.erase(id);
  });
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

  auto runHandler = [handler = notifIter->second, params = notif.params]() {
    try {
      handler(params);
    } catch (const std::exception&) {
      // Swallow error
    }
  };

  if (isInlineNotification(notif.method)) {
    runHandler();
    return;
  }

  if (isSyncNotification(notif.method)) {
    syncExecutor_->submit(std::move(runHandler));
    return;
  }

  workerPool_->submit(std::move(runHandler));
}

} // namespace lsp
