# CLSP — MVP Feature Roadmap

## Context

CLSP is a C++23 foundation library for building LSP servers. The transport, JSON-RPC, lifecycle state machine, document store, and handler registry are already in place (see [ILanguageServer.cpp](src/ILanguageServer.cpp), [JsonRpc.cpp](src/JsonRpc.cpp), [StdioTransport.cpp](src/StdioTransport.cpp), [DocumentStore.cpp](src/DocumentStore.cpp)). What is missing is the glue between those primitives and the things an IDE actually needs from a language server: document sync wiring, server→client messaging (diagnostics, log messages, applyEdit), cancellation, a real capability set, and typed helpers so that derived servers don't have to push and pull raw JSON.

The goal of this plan is to enumerate the gaps and recommend an MVP scope — the smallest set of additions that turns CLSP from "JSON-RPC plumbing" into "a library you can actually use to build a working VSCode/Neovim language server that publishes diagnostics and answers hover/definition/completion".

---

## What already exists (no work needed)

- Stdio transport with Content-Length framing — [StdioTransport.cpp](src/StdioTransport.cpp)
- JSON-RPC 2.0 parsing/serialization with standard + LSP error codes — [JsonRpc.hpp](src/JsonRpc.hpp), [JsonRpc.cpp](src/JsonRpc.cpp)
- Request/notification dispatch with state validation — [ILanguageServer.cpp:90-150](src/ILanguageServer.cpp#L90-L150)
- Lifecycle state machine: initialize / initialized / shutdown / exit — [ILanguageServer.cpp:50-78](src/ILanguageServer.cpp#L50-L78)
- DocumentStore with full + incremental sync and position→offset — [DocumentStore.cpp](src/DocumentStore.cpp)
- `InitializeParams` / `InitializeResult` / `Position` / `Range` / `TextDocumentItem` / `TextDocumentContentChangeEvent` — [include/clsp/protocol/](include/clsp/protocol/)
- GoogleTest harness with coverage of JsonRpc, Protocol serialization, and Lifecycle flow — [tests/](tests/)

---

## Gap analysis

### Tier 1 — MVP blockers (infrastructure)

These are preconditions for any real language server. Without them, the library can speak JSON-RPC but cannot do anything useful.

**1.1 Document sync wiring**
`textDocument/didOpen`, `didChange`, `didClose`, `didSave` are *not* registered as handlers. The `DocumentStore` and `onDocument*` virtual hooks exist in [ILanguageServer.hpp:52-58](include/clsp/ILanguageServer.hpp#L52-L58) but nothing plugs them together. Also, `DidOpenParams` / `DidChangeParams` / `DidCloseParams` in [protocol/Sync.hpp](include/clsp/protocol/Sync.hpp) are empty structs.
- Flesh out the sync param types with JSON (de)serialization.
- Register the four notifications inside `registerLifecycleHandlers()` (or a new `registerSyncHandlers()`), routing to `documents_->open/applyChange/close` and the `onDocument*` hooks.

**1.2 Server → client messaging**
Currently the transport is used only to respond to incoming messages. A language server must be able to *initiate* messages — most critically `textDocument/publishDiagnostics`. This is the single most important missing feature.
- Add `sendNotification(method, params)` on `ILanguageServer`.
- Add `sendRequest(method, params, callback)` with a pending-request map keyed by outgoing id, so that server→client requests (`workspace/applyEdit`, `window/showMessageRequest`, `client/registerCapability`) get their responses correlated.
- Generate outgoing ids from a monotonic counter.

**1.3 Cancellation**
`$/cancelRequest` (notification from client) must at minimum be accepted and acknowledged. For a synchronous server the handler can flip a cancellation flag associated with the in-flight request id; handlers can check it and throw `RequestCancelled`.
- Add a `CancellationToken` passed into request handlers.
- Register `$/cancelRequest` as a built-in notification handler.

**1.4 Expanded `ServerCapabilities`**
[Capabilities.hpp](include/clsp/protocol/Capabilities.hpp) only exposes `textDocumentSync`. For any feature a server provides it must advertise a capability, or the client will never call it.
- Add optional fields for: `hoverProvider`, `completionProvider` (with `triggerCharacters`, `resolveProvider`), `definitionProvider`, `referencesProvider`, `documentSymbolProvider`, `documentFormattingProvider`, `signatureHelpProvider`, `codeActionProvider`, `renameProvider`, `diagnosticProvider` (or rely on pushed diagnostics).
- Extend `to_json` accordingly.

**1.5 Typed handler helpers**
Right now handlers are `std::function<json(const json&)>`. Every derived server has to call `params.get<T>()` and wrap a result in `json`. A template helper hides this:
```cpp
template<class Params, class Result>
void registerRequest(const std::string& method,
                     std::function<Result(const Params&, CancellationToken)>);
```
- Add typed overloads of `registerRequest` / `registerNotification` on [ILanguageServer.hpp](include/clsp/ILanguageServer.hpp).
- Keep the raw-json overloads for flexibility.

**1.6 URI and position-encoding utilities**
LSP uses `file://` URIs and UTF-16 code-unit offsets by default. [DocumentStore.cpp:33-44](src/DocumentStore.cpp) currently treats `Position.character` as a byte offset — correct only for ASCII.
- Add `Uri` helpers: `fromPath(std::filesystem::path)`, `toPath()`, percent-encoding.
- Add a `PositionEncoding` enum (UTF-8, UTF-16, UTF-32) and honor the client's `general.positionEncodings` capability; default to UTF-16. Update `DocumentStore::positionToOffset`.

### Tier 2 — MVP language features (protocol types + capability scaffolding)

These are the features IDEs actually care about. The library should define the types and capability flags so derived servers can fill in the logic. No server-side intelligence is implemented — just types + registration paths + examples.

**2.1 Diagnostics** (most important — push model)
- `Diagnostic`, `DiagnosticSeverity`, `DiagnosticTag`, `DiagnosticRelatedInformation`, `CodeDescription`
- `PublishDiagnosticsParams`
- Convenience method `publishDiagnostics(uri, diagnostics)` on `ILanguageServer`.

**2.2 Hover**
- `HoverParams`, `Hover`, `MarkupContent`, `MarkupKind`

**2.3 Completion**
- `CompletionParams`, `CompletionContext`, `CompletionTriggerKind`
- `CompletionItem`, `CompletionItemKind`, `CompletionList`, `InsertTextFormat`, `TextEdit`

**2.4 Definition / References / DocumentSymbol**
- `DefinitionParams`, `Location`, `LocationLink`
- `ReferenceParams`, `ReferenceContext`
- `DocumentSymbolParams`, `DocumentSymbol`, `SymbolKind`, `SymbolInformation`

### Tier 3 — Post-MVP (explicitly deferred)

Documented here so they don't creep into MVP scope: progress reporting (`$/progress`, `window/workDoneProgress/create`), workspace folders & configuration (`workspace/configuration`, `workspace/didChangeConfiguration`, `workspace/didChangeWatchedFiles`), formatting / rangeFormatting / onTypeFormatting, `textDocument/codeAction` + `workspace/applyEdit`, `textDocument/rename` + `prepareRename`, `textDocument/signatureHelp`, semantic tokens, inlay hints, call/type hierarchy, pull-model diagnostics (`textDocument/diagnostic`), structured logging / tracing (`$/setTrace`, `$/logTrace`, `window/logMessage`), thread-pool request execution.

### Testing gaps (MVP coverage must-haves)

- `DocumentStore` — open/change/close/positionToOffset including multi-byte chars.
- `StdioTransport` — Content-Length framing, partial reads, EOF handling (via an in-memory `iostream` pair).
- End-to-end: feed raw bytes through transport → server → expect diagnostics published.
- Typed handler helpers — compile-time and round-trip.
- Cancellation — handler observes token flipped by `$/cancelRequest`.

---

## MVP scope (decided)

**In MVP:** Tier 1 (all of it), plus Tier 2.1 (diagnostics) and Tier 2.2 (hover). That's the threshold where CLSP can be plugged into VSCode with a toy checker and produce squigglies + hover tooltips — a demonstrable end-to-end language server.

**Deferred (next milestone):** Tier 2.3 (completion) and Tier 2.4 (definition / references / documentSymbol) protocol types. The thread-pool and typed-handler machinery built in MVP makes adding them a mechanical follow-up.

**Deferred indefinitely:** everything in Tier 3.

## Decided design choices

### Concurrency: thread-pool dispatch *(implemented)*
The `run()` loop is no longer strictly serial. The shipped wiring:

- The read loop stays on the main thread: it reads one framed message and classifies it (request / notification / response / malformed).
- A **fixed-size worker pool** (configurable via `LanguageServerOptions::workerThreads`, default `ThreadPool::defaultWorkerCount()` ≈ `hardware_concurrency()`) executes most request and notification handlers.
- A second **single-worker `ThreadPool` instance** acts as the serial executor for document-sync notifications (`textDocument/didOpen` / `didChange` / `didClose` / `didSave`) so that document state mutations are ordered and never race. Requests that read document state hold the `DocumentStore` shared_mutex read lock.
- **Lifecycle messages run inline on the read thread** so state transitions are visible before the next message is classified: `initialize` and `shutdown` requests, plus the `initialized` / `exit` / `$/cancelRequest` notifications. Everything else dispatches asynchronously. (This is a refinement over the original plan, which left dispatch policy unspecified for non-sync notifications.)
- A `std::mutex` (`sendMutex_`) guards `transport_->sendMessage(...)` via `sendFrame()`, so worker threads cannot interleave frames on stdout.
- `pendingRequests_` (outgoing-id → callback) and `cancellationTokens_` (incoming-id → token) are each protected by their own `std::mutex`. Request- and notification-handler maps are written only at construction time and read after.
- `DocumentStore` got an internal `std::shared_mutex`: mutators take a unique lock, observers a shared lock.
- `CancellationToken` wraps a `std::shared_ptr<std::atomic<bool>>` shared between the dispatcher and the handler thread; `$/cancelRequest` flips it. Handlers poll `token.isCancelled()` at checkpoints and throw `lsp::RequestCancelled` (or any other `JsonRpcException`); the dispatcher converts it into a `RequestCancelled` (-32800) error response. The dispatcher also short-circuits with the same error if the token was already cancelled before the worker dequeued the request.

`ILanguageServer` owns two `ThreadPool` instances (`workerPool_`, `syncExecutor_`). `run()`'s main loop dispatches asynchronously and loops back to `readMessage()`. After the loop exits (on `exit` or EOF), `run()` shuts down both pools so queued response callbacks and handlers drain before returning.

### Handler API: typed + raw overloads *(implemented)*
The raw `RequestHandler` signature is `std::function<json(const json&, CancellationToken)>` — the cancellation token is now part of the contract. Typed templates layer on top:

```cpp
template <class Params, class Result>
void registerRequest(const std::string& method,
                     std::function<Result(const Params&, CancellationToken)> h);

template <class Params>
void registerNotification(const std::string& method,
                          std::function<void(const Params&)> h);
```

The typed overload wraps `params.get<Params>()` on input and `json(result)` on output, and delegates to the raw overload. The `void` `Result` case is handled with `if constexpr (std::is_void_v<Result>)` returning `nullptr`. The typed API leans on the existing `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` machinery in [protocol/](include/clsp/protocol/) so no extra serialization plumbing is needed.

---

## Critical files (MVP changes will touch these)

- [include/clsp/ILanguageServer.hpp](include/clsp/ILanguageServer.hpp) — outgoing `sendRequest`/`sendNotification`, typed `registerRequest`/`registerNotification`, `CancellationToken`, thread-pool size option, `publishDiagnostics` convenience
- [src/ILanguageServer.cpp](src/ILanguageServer.cpp) — dispatch loop rewrite: worker pool + serial sync executor + send mutex + pending-outgoing map + cancellation map, sync notification wiring, `$/cancelRequest` handler, shutdown drain
- [include/clsp/protocol/Capabilities.hpp](include/clsp/protocol/Capabilities.hpp) — expanded `ServerCapabilities` (hover/completion/definition/references/documentSymbol/etc.) even though only hover is exercised in MVP, so the field set is in place
- [include/clsp/protocol/Sync.hpp](include/clsp/protocol/Sync.hpp) — fill in `DidOpenParams` / `DidChangeParams` / `DidCloseParams` / `DidSaveParams`
- [include/clsp/protocol/Diagnostics.hpp](include/clsp/protocol/Diagnostics.hpp) — new: `Diagnostic`, `DiagnosticSeverity`, `DiagnosticTag`, `PublishDiagnosticsParams`
- [include/clsp/protocol/Hover.hpp](include/clsp/protocol/Hover.hpp) — new: `HoverParams`, `Hover`, `MarkupContent`, `MarkupKind`
- [include/clsp/Uri.hpp](include/clsp/Uri.hpp) — new: `file://` ↔ path conversion, percent-encoding
- [include/clsp/PositionEncoding.hpp](include/clsp/PositionEncoding.hpp) — new: `PositionEncoding` enum
- [src/DocumentStore.hpp](src/DocumentStore.hpp) / [src/DocumentStore.cpp](src/DocumentStore.cpp) — thread-safety (shared_mutex), encoding-aware `positionToOffset`
- [src/ThreadPool.hpp](src/ThreadPool.hpp) — new internal worker pool + serial executor (std-library only, no new deps)
- [tests/](tests/) — new suites:
  - **landed in Tier 1**: `DocumentStoreTest.cpp`, `DispatchTest.cpp`, `CancellationTest.cpp`, `UriTest.cpp`, `PositionEncodingTest.cpp`, `ThreadPoolTest.cpp`
  - **still to come (Tier 2)**: `DiagnosticsTest.cpp`, `HoverTest.cpp`, `EndToEndTest.cpp`, `StdioTransportTest.cpp`
- [examples/demo-server/](examples/demo-server/) — new: tiny server that opens documents, publishes a fake diagnostic, answers hover, usable as a VSCode extension target

## Verification

- `cmake --build build && ctest --test-dir build` passes with all new suites.
- `DocumentStoreTest` covers multi-byte `positionToOffset`, full vs incremental change application, concurrent reader/writer smoke test.
- `DispatchTest` drives a fake transport through the full pipeline — verifies that (a) two in-flight requests really run on different threads, (b) the send mutex prevents frame interleaving, (c) the serial sync executor orders `didChange` events.
- `CancellationTest` fires a slow handler, sends `$/cancelRequest`, asserts a `RequestCancelled` error response comes back and that the handler's token observed the flip.
- `EndToEndTest` feeds an initialize / didOpen / hover / shutdown / exit sequence through a stdio-equivalent stream pair and asserts the diagnostics + hover frames emitted.
- Manual smoke test: run `examples/demo-server` as a VSCode LSP extension; open a file, confirm squigglies appear and hovering produces a tooltip. This is the bar for calling MVP "done".
