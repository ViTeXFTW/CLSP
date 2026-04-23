#include "JsonRpc.hpp"

namespace lsp {

namespace rpc {

nlohmann::json idToJson(const std::variant<int, std::string>& id) {
  return std::visit([](const auto& v) -> nlohmann::json { return v; }, id);
}

ParsedMessage parseMessage(const std::string& rawJson) {
  nlohmann::json j;
  try {
    j = nlohmann::json::parse(rawJson);
  } catch (const nlohmann::json::parse_error&) {
    return ParsedError{ErrorCodes::ParseError, "Invalid JSON"};
  }

  if (!j.is_object()) {
    return ParsedError{ErrorCodes::InvalidRequest, "Message must be an object"};
  }

  auto jsonrpcIter = j.find("jsonrpc");
  if (jsonrpcIter == j.end() || !jsonrpcIter->is_string() ||
      jsonrpcIter->get<std::string>() != JSONRPC_VERSION) {
    return ParsedError{ErrorCodes::InvalidRequest,
                       "Missing or invalid jsonrpc version"};
  }

  auto methodIter = j.find("method");

  // No method → server-initiated response coming back to us
  if (methodIter == j.end() || !methodIter->is_string()) {
    auto idIter = j.find("id");
    if (idIter == j.end()) {
      return ParsedError{ErrorCodes::InvalidRequest, "Missing method and id"};
    }
    std::variant<int, std::string> id;
    if (idIter->is_number_integer()) {
      id = idIter->get<int>();
    } else if (idIter->is_string()) {
      id = idIter->get<std::string>();
    } else {
      return ParsedError{ErrorCodes::InvalidRequest, "Invalid id type"};
    }
    if (j.contains("error")) {
      return ParsedResponse{std::move(id), j["error"], true};
    }
    if (j.contains("result")) {
      return ParsedResponse{std::move(id), j["result"], false};
    }
    return ParsedError{ErrorCodes::InvalidRequest,
                       "Response missing both result and error"};
  }

  std::string method = methodIter->get<std::string>();
  nlohmann::json params =
      j.contains("params") ? j["params"] : nlohmann::json(nullptr);

  auto idIter = j.find("id");
  if (idIter != j.end()) {
    std::variant<int, std::string> id;
    if (idIter->is_number_integer()) {
      id = idIter->get<int>();
    } else if (idIter->is_string()) {
      id = idIter->get<std::string>();
    } else {
      return ParsedError{ErrorCodes::InvalidRequest, "Invalid id type"};
    }

    return ParsedRequest{std::move(id), std::move(method), std::move(params)};
  }

  return ParsedNotification{std::move(method), std::move(params)};
}

std::string serializeResult(const std::variant<int, std::string>& id,
                            const nlohmann::json& result) {
  nlohmann::json j;
  j["jsonrpc"] = JSONRPC_VERSION;
  j["id"] = idToJson(id);
  j["result"] = result;
  return j.dump();
}

std::string serializeError(const std::variant<int, std::string>& id,
                           ErrorCodes code, const std::string& message) {
  nlohmann::json j;
  j["jsonrpc"] = JSONRPC_VERSION;
  j["id"] = idToJson(id);
  j["error"] = {{"code", static_cast<int>(code)}, {"message", message}};
  return j.dump();
}

std::string serializeError(std::nullptr_t, ErrorCodes code,
                           const std::string& message) {
  nlohmann::json j;
  j["jsonrpc"] = JSONRPC_VERSION;
  j["id"] = nullptr;
  j["error"] = {{"code", static_cast<int>(code)}, {"message", message}};
  return j.dump();
}

std::string serializeRequest(int id, const std::string& method,
                             const nlohmann::json& params) {
  nlohmann::json j;
  j["jsonrpc"] = JSONRPC_VERSION;
  j["id"] = id;
  j["method"] = method;
  j["params"] = params;
  return j.dump();
}

std::string serializeNotification(const std::string& method,
                                  const nlohmann::json& params) {
  nlohmann::json j;
  j["jsonrpc"] = JSONRPC_VERSION;
  j["method"] = method;
  j["params"] = params;
  return j.dump();
}

} // namespace rpc

} // namespace lsp
