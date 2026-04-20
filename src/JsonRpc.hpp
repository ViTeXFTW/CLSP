#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <variant>

namespace lsp {

namespace rpc {

const std::string JSONRPC_VERSION = "2.0";

enum class ErrorCodes {
  ParseError = -32700,
  InvalidRequest = -32600,
  MethodNotFound = -32601,
  InvalidParams = -32602,
  InternalError = 32603,

  ServerNotInitialized = -32002,
  RequestFailed = -32803,
  ServerCancelled = -32802,
  ContentModified = -32801,
  RequestCancelled = -32800
};

class JsonRpcException : public std::runtime_error {
public:
  JsonRpcException(ErrorCodes code, const std::string& msg)
      : std::runtime_error(msg), code_(code) {}
  ErrorCodes code() const noexcept { return code_; };

private:
  ErrorCodes code_;
};

struct ParsedRequest {
  std::variant<int, std::string> id;
  std::string method;
  nlohmann::json params;
};

struct ParsedNotification {
  std::string method;
  nlohmann::json params;
};

struct ParsedError {
  ErrorCodes code;
  std::string message;
};

using ParsedMessage =
    std::variant<ParsedRequest, ParsedNotification, ParsedError>;

ParsedMessage parseMessage(const std::string& rawJson);

std::string serializeResult(const std::variant<int, std::string>& id,
                            const nlohmann::json& result);

std::string serializeNotification(const std::string& method,
                                  const nlohmann::json& params);

std::string serializeError(const std::variant<int, std::string>& id,
                           ErrorCodes code, const std::string& message);

std::string serializeError(std::nullptr_t id, ErrorCodes code,
                           const std::string& message);

} // namespace rpc

} // namespace lsp
