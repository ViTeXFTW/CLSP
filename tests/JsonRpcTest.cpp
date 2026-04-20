#include "JsonRpc.hpp"
#include <gtest/gtest.h>

using namespace lsp::rpc;

// ── parseMessage
// ──────────────────────────────────────────────────────────────

TEST(ParseMessage, ValidRequest) {
  auto msg = parseMessage(
      R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
  ASSERT_TRUE(std::holds_alternative<ParsedRequest>(msg));
  auto& req = std::get<ParsedRequest>(msg);
  EXPECT_EQ(req.method, "initialize");
  EXPECT_EQ(std::get<int>(req.id), 1);
}

TEST(ParseMessage, ValidRequestWithStringId) {
  auto msg = parseMessage(
      R"({"jsonrpc":"2.0","id":"abc","method":"hover","params":{}})");
  ASSERT_TRUE(std::holds_alternative<ParsedRequest>(msg));
  EXPECT_EQ(std::get<std::string>(std::get<ParsedRequest>(msg).id), "abc");
}

TEST(ParseMessage, ValidNotification) {
  auto msg =
      parseMessage(R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
  ASSERT_TRUE(std::holds_alternative<ParsedNotification>(msg));
  EXPECT_EQ(std::get<ParsedNotification>(msg).method, "initialized");
}

TEST(ParseMessage, InvalidJson) {
  auto msg = parseMessage("not json {{{");
  ASSERT_TRUE(std::holds_alternative<ParsedError>(msg));
  EXPECT_EQ(std::get<ParsedError>(msg).code, ErrorCodes::ParseError);
}

TEST(ParseMessage, WrongJsonRpcVersion) {
  auto msg = parseMessage(R"({"jsonrpc":"1.0","method":"test","params":{}})");
  ASSERT_TRUE(std::holds_alternative<ParsedError>(msg));
  EXPECT_EQ(std::get<ParsedError>(msg).code, ErrorCodes::InvalidRequest);
}

TEST(ParseMessage, MissingMethod) {
  auto msg = parseMessage(R"({"jsonrpc":"2.0","id":1,"params":{}})");
  ASSERT_TRUE(std::holds_alternative<ParsedError>(msg));
  EXPECT_EQ(std::get<ParsedError>(msg).code, ErrorCodes::InvalidRequest);
}

TEST(ParseMessage, NotAnObject) {
  auto msg = parseMessage(R"([1,2,3])");
  ASSERT_TRUE(std::holds_alternative<ParsedError>(msg));
  EXPECT_EQ(std::get<ParsedError>(msg).code, ErrorCodes::InvalidRequest);
}

// ── serialization
// ─────────────────────────────────────────────────────────────

TEST(SerializeResult, IntId) {
  auto s = serializeResult(1, nlohmann::json{{"key", "val"}});
  auto j = nlohmann::json::parse(s);
  EXPECT_EQ(j["jsonrpc"], "2.0");
  EXPECT_EQ(j["id"], 1);
  EXPECT_EQ(j["result"]["key"], "val");
}

TEST(SerializeResult, StringId) {
  auto s = serializeResult(std::string{"req-1"}, nlohmann::json(nullptr));
  auto j = nlohmann::json::parse(s);
  EXPECT_EQ(j["id"], "req-1");
  EXPECT_TRUE(j["result"].is_null());
}

TEST(SerializeError, WithIntId) {
  auto s = serializeError(1, ErrorCodes::MethodNotFound, "not found");
  auto j = nlohmann::json::parse(s);
  EXPECT_EQ(j["id"], 1);
  EXPECT_EQ(j["error"]["code"], static_cast<int>(ErrorCodes::MethodNotFound));
  EXPECT_EQ(j["error"]["message"], "not found");
}

TEST(SerializeError, WithNullId) {
  auto s = serializeError(nullptr, ErrorCodes::ParseError, "bad json");
  auto j = nlohmann::json::parse(s);
  EXPECT_TRUE(j["id"].is_null());
  EXPECT_EQ(j["error"]["code"], static_cast<int>(ErrorCodes::ParseError));
}
