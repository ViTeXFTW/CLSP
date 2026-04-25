#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace lsp {

enum class TextDocumentSyncKind { None = 0, Full = 1, Incremental = 2 };

struct CompletionOptions {
  std::optional<std::vector<std::string>> triggerCharacters;
  std::optional<std::vector<std::string>> allCommitCharacters;
  std::optional<bool> resolveProvider;
};

inline void to_json(nlohmann::json& j, const CompletionOptions& o) {
  j = nlohmann::json::object();
  if (o.triggerCharacters) {
    j["triggerCharacters"] = *o.triggerCharacters;
  }
  if (o.allCommitCharacters) {
    j["allCommitCharacters"] = *o.allCommitCharacters;
  }
  if (o.resolveProvider) {
    j["resolveProvider"] = *o.resolveProvider;
  }
}

struct SignatureHelpOptions {
  std::optional<std::vector<std::string>> triggerCharacters;
  std::optional<std::vector<std::string>> retriggerCharacters;
};

inline void to_json(nlohmann::json& j, const SignatureHelpOptions& o) {
  j = nlohmann::json::object();
  if (o.triggerCharacters) {
    j["triggerCharacters"] = *o.triggerCharacters;
  }
  if (o.retriggerCharacters) {
    j["retriggerCharacters"] = *o.retriggerCharacters;
  }
}

struct CodeActionOptions {
  std::optional<std::vector<std::string>> codeActionKinds;
  std::optional<bool> resolveProvider;
};

inline void to_json(nlohmann::json& j, const CodeActionOptions& o) {
  j = nlohmann::json::object();
  if (o.codeActionKinds) {
    j["codeActionKinds"] = *o.codeActionKinds;
  }
  if (o.resolveProvider) {
    j["resolveProvider"] = *o.resolveProvider;
  }
}

struct RenameOptions {
  std::optional<bool> prepareProvider;
};

inline void to_json(nlohmann::json& j, const RenameOptions& o) {
  j = nlohmann::json::object();
  if (o.prepareProvider) {
    j["prepareProvider"] = *o.prepareProvider;
  }
}

struct DiagnosticOptions {
  std::optional<std::string> identifier;
  bool interFileDependencies = false;
  bool workspaceDiagnostics = false;
};

inline void to_json(nlohmann::json& j, const DiagnosticOptions& o) {
  j = nlohmann::json::object();
  if (o.identifier) {
    j["identifier"] = *o.identifier;
  }
  j["interFileDependencies"] = o.interFileDependencies;
  j["workspaceDiagnostics"] = o.workspaceDiagnostics;
}

struct ServerCapabilities {
  std::optional<TextDocumentSyncKind> textDocumentSync;
  std::optional<bool> hoverProvider;
  std::optional<CompletionOptions> completionProvider;
  std::optional<bool> definitionProvider;
  std::optional<bool> referencesProvider;
  std::optional<bool> documentSymbolProvider;
  std::optional<bool> documentFormattingProvider;
  std::optional<SignatureHelpOptions> signatureHelpProvider;
  std::optional<CodeActionOptions> codeActionProvider;
  std::optional<RenameOptions> renameProvider;
  std::optional<DiagnosticOptions> diagnosticProvider;
};

inline void to_json(nlohmann::json& j, const ServerCapabilities& c) {
  j = nlohmann::json::object();
  if (c.textDocumentSync) {
    j["textDocumentSync"] = static_cast<int>(*c.textDocumentSync);
  }
  if (c.hoverProvider) {
    j["hoverProvider"] = *c.hoverProvider;
  }
  if (c.completionProvider) {
    j["completionProvider"] = *c.completionProvider;
  }
  if (c.definitionProvider) {
    j["definitionProvider"] = *c.definitionProvider;
  }
  if (c.referencesProvider) {
    j["referencesProvider"] = *c.referencesProvider;
  }
  if (c.documentSymbolProvider) {
    j["documentSymbolProvider"] = *c.documentSymbolProvider;
  }
  if (c.documentFormattingProvider) {
    j["documentFormattingProvider"] = *c.documentFormattingProvider;
  }
  if (c.signatureHelpProvider) {
    j["signatureHelpProvider"] = *c.signatureHelpProvider;
  }
  if (c.codeActionProvider) {
    j["codeActionProvider"] = *c.codeActionProvider;
  }
  if (c.renameProvider) {
    j["renameProvider"] = *c.renameProvider;
  }
  if (c.diagnosticProvider) {
    j["diagnosticProvider"] = *c.diagnosticProvider;
  }
}

struct ClientCapabilities {};

} // namespace lsp
