# CLSP

A modern C++ framework for building Language Server Protocol (LSP) servers.

CLSP gives you the wiring — JSON-RPC framing, message dispatch, document
synchronization, cancellation, threading — so you only have to write the
language-specific bits (diagnostics, hover, completion, …).

- **C++17**, header-friendly, single static library
- **stdio transport** out of the box (the LSP default)
- **Strongly-typed protocol structs** with `nlohmann::json` (de)serialization
- **Threaded dispatch** with cooperative cancellation
- **Document store** that handles `Full` and `Incremental` text sync for you

---

## Requirements

- A C++17 compiler
- CMake 3.14+
- [nlohmann/json](https://github.com/nlohmann/json) — fetched automatically if
  not already present on your system

---

## Consuming CLSP

### FetchContent (no install required)

The simplest path. CMake downloads and builds CLSP as part of your own
configure step. `nlohmann/json` is pulled in automatically if it is not already
available.

```cmake
include(FetchContent)

FetchContent_Declare(
    CLSP
    GIT_REPOSITORY https://github.com/ViTeXFTW/CLSP
    GIT_TAG        v1.0.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(CLSP)

add_executable(my-language-server main.cpp)
target_link_libraries(my-language-server PRIVATE CLSP::clsp)
```

### find_package (system install)

Build and install CLSP once, then reference it from any project via
`find_package`.

```bash
# Clone and install
git clone https://github.com/ViTeXFTW/CLSP
cmake -S CLSP -B CLSP/build -DCMAKE_BUILD_TYPE=Release
cmake --build CLSP/build
cmake --install CLSP/build --prefix /your/install/prefix
```

```cmake
# In your own CMakeLists.txt
find_package(CLSP CONFIG REQUIRED)

add_executable(my-language-server main.cpp)
target_link_libraries(my-language-server PRIVATE CLSP::clsp)
```

If you installed to a non-standard prefix, pass
`-DCMAKE_PREFIX_PATH=/your/install/prefix` when configuring your project.

### pkg-config (Meson, Make, or manual builds)

After installing, a `clsp.pc` file is written to `lib/pkgconfig/`. Point
`PKG_CONFIG_PATH` at it:

```bash
export PKG_CONFIG_PATH=/your/install/prefix/lib/pkgconfig

pkg-config --cflags clsp   # → -I/your/install/prefix/include -std=c++23
pkg-config --libs   clsp   # → -L/your/install/prefix/lib -lclsp
```

In a `meson.build`:

```python
clsp_dep = dependency('clsp')
executable('my-language-server', 'main.cpp', dependencies: clsp_dep)
```

---

## Hello, server

The smallest possible language server. It accepts an `initialize` request,
declares no capabilities, and runs over stdio.

```cpp
#include <clsp/ILanguageServer.hpp>
#include <clsp/protocol/Lifecycle.hpp>

class MyServer : public lsp::ILanguageServer {
public:
  using lsp::ILanguageServer::ILanguageServer; // inherit constructors

  lsp::InitializeResult onInitialize(const lsp::InitializeParams&) override {
    lsp::InitializeResult result;
    result.serverInfo = lsp::InitializeResult::ServerInfo{"my-server", "0.1.0"};
    return result;
  }
};

int main() {
  MyServer server; // default ctor → stdio transport, default thread pool
  return server.run();
}
```

`run()` blocks, reads framed messages from stdin, writes responses to stdout,
and returns when the client sends `shutdown` followed by `exit`.

---

## Declaring capabilities

Tell the client what your server can do by populating `ServerCapabilities` in
`onInitialize`. CLSP only enables features you ask for — for example, document
synchronization is off until you turn it on.

```cpp
lsp::InitializeResult onInitialize(const lsp::InitializeParams&) override {
  lsp::InitializeResult result;
  result.serverInfo = lsp::InitializeResult::ServerInfo{"my-server", "0.1.0"};

  auto& caps = result.capabilities;
  caps.textDocumentSync = lsp::TextDocumentSyncKind::Incremental;
  caps.hoverProvider    = true;

  caps.completionProvider = lsp::CompletionOptions{};
  caps.completionProvider->triggerCharacters = std::vector<std::string>{".", "::"};

  return result;
}
```

---

## Reacting to documents

Override the document hooks to react to text the editor sends you. The
built-in `DocumentStore` already keeps a synchronized in-memory copy — you
just consume it.

```cpp
void onDocumentOpened(const lsp::TextDocumentItem& doc) override {
  // Always call the base to keep the document store in sync.
  lsp::ILanguageServer::onDocumentOpened(doc);
  analyze(doc);
}

void onDocumentChanged(
    const lsp::TextDocumentItem& doc,
    const std::vector<lsp::TextDocumentChangeEvent>& changes) override {
  lsp::ILanguageServer::onDocumentChanged(doc, changes);
  analyze(doc);
}

void onDocumentClosed(const lsp::DocumentUri& uri) override {
  lsp::ILanguageServer::onDocumentClosed(uri);
}
```

---

## Publishing diagnostics

`publishDiagnostics` is a thin convenience over the
`textDocument/publishDiagnostics` notification.

```cpp
#include <clsp/protocol/Diagnostics.hpp>

void analyze(const lsp::TextDocumentItem& doc) {
  std::vector<lsp::Diagnostic> diags;

  lsp::Diagnostic d;
  d.range    = lsp::Range{{0, 0}, {0, 5}};
  d.severity = lsp::DiagnosticSeverity::Warning;
  d.source   = "my-server";
  d.message  = "TODO: implement";
  diags.push_back(std::move(d));

  publishDiagnostics(doc.uri, std::move(diags), doc.version);
}
```

---

## Custom requests and notifications

For anything beyond the built-in lifecycle and sync handlers, register your
own. Both raw-JSON and typed overloads are available.

### Typed (recommended)

```cpp
#include <clsp/protocol/Hover.hpp>

MyServer() {
  registerRequest<lsp::HoverParams, lsp::Hover>(
      "textDocument/hover",
      [this](const lsp::HoverParams& p, lsp::CancellationToken token) {
        // Long work? Poll the token and bail out cleanly.
        if (token.isCancelled()) throw lsp::RequestCancelled{};

        lsp::Hover h;
        h.contents = lsp::MarkupContent{
            lsp::MarkupKind::Markdown,
            "**" + p.textDocument.uri + "** at line " +
                std::to_string(p.position.line)};
        return h;
      });
}
```

> Register your handlers from the **derived class constructor** — `registerRequest`
> and `registerNotification` are protected members of `ILanguageServer`.

### Raw JSON

```cpp
registerNotification("$/setTrace",
                     [](const nlohmann::json& params) {
                       // do something with params["value"]
                     });
```

### Sending requests to the client

```cpp
sendRequest("workspace/applyEdit",
            { {"edit", nlohmann::json::object()} },
            [](const nlohmann::json& payload, bool isError) {
              if (isError) { /* handle */ }
            });
```

---

## URI helpers

LSP exchanges paths as `file://` URIs. Use `lsp::uri` to convert.

```cpp
#include <clsp/Uri.hpp>

auto uri  = lsp::uri::fromPath("C:/projects/foo/main.cpp");
// "file:///C:/projects/foo/main.cpp"
auto path = lsp::uri::toPath(uri);
// std::filesystem::path
```

---

## Configuring the runtime

Pass `LanguageServerOptions` to the constructor to tune the worker pool, or
plug in your own transport / document store.

```cpp
lsp::LanguageServerOptions opts;
opts.workerThreads = 4; // 0 → hardware_concurrency

MyServer server{lsp::makeStdioTransport(), opts};
```

The `ITransport` and `IDocumentStore` interfaces are public, so you can
substitute a socket transport or a custom store (for testing, virtual file
systems, etc.) without touching the dispatch layer.

---

## Testing your server

`ITransport` is trivial to mock — push raw JSON-RPC frames in, inspect what
the server sends out. See `tests/ServerMessagingTest.cpp` for a fully worked
example using a `MockTransport`.

To build and run the test suite:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCLSP_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

---

## What's next

- `clsp/protocol/Lifecycle.hpp`, `Documents.hpp`, `Diagnostics.hpp`,
  `Hover.hpp`, `Capabilities.hpp` — start here for available types.
- `clsp/ILanguageServer.hpp` — full list of overridable hooks and the
  request/notification API.
- `clsp/ITransport.hpp`, `clsp/IDocumentStore.hpp` — interfaces to implement
  if you need to customize transport or storage.
