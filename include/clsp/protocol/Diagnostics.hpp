#pragma once

#include <clsp/protocol/Basic.hpp>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace lsp {

enum class DiagnosticSeverity {
  Error = 1,
  Warning = 2,
  Information = 3,
  Hint = 4
};

enum class DiagnosticTag { Unnecessary = 1, Deprecated = 2 };

struct Location {
  DocumentUri uri;
  Range range;
};

inline void to_json(nlohmann::json& j, const Location& l) {
  j = nlohmann::json{{"uri", l.uri}, {"range", l.range}};
}

inline void from_json(const nlohmann::json& j, Location& l) {
  j.at("uri").get_to(l.uri);
  j.at("range").get_to(l.range);
}

struct DiagnosticRelatedInformation {
  Location location;
  std::string message;
};

inline void to_json(nlohmann::json& j, const DiagnosticRelatedInformation& r) {
  j = nlohmann::json{{"location", r.location}, {"message", r.message}};
}

inline void from_json(const nlohmann::json& j,
                      DiagnosticRelatedInformation& r) {
  j.at("location").get_to(r.location);
  j.at("message").get_to(r.message);
}

struct CodeDescription {
  std::string href;
};

inline void to_json(nlohmann::json& j, const CodeDescription& c) {
  j = nlohmann::json{{"href", c.href}};
}

inline void from_json(const nlohmann::json& j, CodeDescription& c) {
  j.at("href").get_to(c.href);
}

struct Diagnostic {
  Range range;
  std::optional<DiagnosticSeverity> severity;
  std::optional<std::variant<int32_t, std::string>> code;
  std::optional<CodeDescription> codeDescription;
  std::optional<std::string> source;
  std::string message;
  std::optional<std::vector<DiagnosticTag>> tags;
  std::optional<std::vector<DiagnosticRelatedInformation>> relatedInformation;
  std::optional<nlohmann::json> data;
};

inline void to_json(nlohmann::json& j, const Diagnostic& d) {
  j = nlohmann::json::object();
  j["range"] = d.range;
  j["message"] = d.message;
  if (d.severity) {
    j["severity"] = static_cast<int>(*d.severity);
  }
  if (d.code) {
    std::visit([&j](const auto& v) { j["code"] = v; }, *d.code);
  }
  if (d.codeDescription) {
    j["codeDescription"] = *d.codeDescription;
  }
  if (d.source) {
    j["source"] = *d.source;
  }
  if (d.tags) {
    auto arr = nlohmann::json::array();
    for (auto t : *d.tags) {
      arr.push_back(static_cast<int>(t));
    }
    j["tags"] = std::move(arr);
  }
  if (d.relatedInformation) {
    j["relatedInformation"] = *d.relatedInformation;
  }
  if (d.data) {
    j["data"] = *d.data;
  }
}

inline void from_json(const nlohmann::json& j, Diagnostic& d) {
  j.at("range").get_to(d.range);
  j.at("message").get_to(d.message);
  if (j.contains("severity") && !j["severity"].is_null()) {
    d.severity = static_cast<DiagnosticSeverity>(j["severity"].get<int>());
  }
  if (j.contains("code") && !j["code"].is_null()) {
    if (j["code"].is_number_integer()) {
      d.code = j["code"].get<int32_t>();
    } else if (j["code"].is_string()) {
      d.code = j["code"].get<std::string>();
    }
  }
  if (j.contains("codeDescription") && !j["codeDescription"].is_null()) {
    d.codeDescription = j["codeDescription"].get<CodeDescription>();
  }
  if (j.contains("source") && !j["source"].is_null()) {
    d.source = j["source"].get<std::string>();
  }
  if (j.contains("tags") && !j["tags"].is_null()) {
    std::vector<DiagnosticTag> tags;
    for (const auto& t : j["tags"]) {
      tags.push_back(static_cast<DiagnosticTag>(t.get<int>()));
    }
    d.tags = std::move(tags);
  }
  if (j.contains("relatedInformation") && !j["relatedInformation"].is_null()) {
    d.relatedInformation =
        j["relatedInformation"].get<std::vector<DiagnosticRelatedInformation>>();
  }
  if (j.contains("data") && !j["data"].is_null()) {
    d.data = j["data"];
  }
}

struct PublishDiagnosticsParams {
  DocumentUri uri;
  std::optional<int32_t> version;
  std::vector<Diagnostic> diagnostics;
};

inline void to_json(nlohmann::json& j, const PublishDiagnosticsParams& p) {
  j = nlohmann::json::object();
  j["uri"] = p.uri;
  j["diagnostics"] = p.diagnostics;
  if (p.version) {
    j["version"] = *p.version;
  }
}

inline void from_json(const nlohmann::json& j, PublishDiagnosticsParams& p) {
  j.at("uri").get_to(p.uri);
  j.at("diagnostics").get_to(p.diagnostics);
  if (j.contains("version") && !j["version"].is_null()) {
    p.version = j["version"].get<int32_t>();
  }
}

} // namespace lsp
