#pragma once

namespace lsp {

class ILanguageServer {
public:
  explicit ILanguageServer() {}
  virtual ~ILanguageServer() = default;

  ILanguageServer(const ILanguageServer&) = delete;
  ILanguageServer& operator=(const ILanguageServer) = delete;

  int run() {
    return 0;
  }
};

} // lsp
