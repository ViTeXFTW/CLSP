#include <clsp/protocol/Capabilities.hpp>
#include <clsp/protocol/SemanticTokens.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace lsp;

TEST(SemanticTokensLegend, RoundTrip) {
  SemanticTokensLegend legend;
  legend.tokenTypes = {semantic_token_types::kKeyword,
                       semantic_token_types::kVariable};
  legend.tokenModifiers = {semantic_token_modifiers::kReadonly};

  auto j = nlohmann::json(legend);
  EXPECT_EQ(j["tokenTypes"][0], "keyword");
  EXPECT_EQ(j["tokenTypes"][1], "variable");
  EXPECT_EQ(j["tokenModifiers"][0], "readonly");

  auto r = j.get<SemanticTokensLegend>();
  EXPECT_EQ(r.tokenTypes.size(), 2u);
  EXPECT_EQ(r.tokenTypes[1], "variable");
  EXPECT_EQ(r.tokenModifiers[0], "readonly");
}

TEST(SemanticTokensOptions, MinimalSerialization) {
  SemanticTokensOptions o;
  o.legend.tokenTypes = {"keyword"};
  o.legend.tokenModifiers = {};
  auto j = nlohmann::json(o);
  EXPECT_TRUE(j.contains("legend"));
  EXPECT_FALSE(j.contains("range"));
  EXPECT_FALSE(j.contains("full"));
}

TEST(SemanticTokensOptions, FlagsSerialized) {
  SemanticTokensOptions o;
  o.legend.tokenTypes = {"keyword"};
  o.legend.tokenModifiers = {};
  o.range = false;
  o.full = true;
  auto j = nlohmann::json(o);
  EXPECT_EQ(j["range"], false);
  EXPECT_EQ(j["full"], true);
}

TEST(SemanticTokensParams, Deserialization) {
  auto j = nlohmann::json::parse(R"({
        "textDocument": {"uri": "file:///a.cpp"}
    })");
  auto p = j.get<SemanticTokensParams>();
  EXPECT_EQ(p.textDocument.uri, "file:///a.cpp");
}

TEST(SemanticTokensRangeParams, Deserialization) {
  auto j = nlohmann::json::parse(R"({
        "textDocument": {"uri": "file:///a.cpp"},
        "range": {"start": {"line": 1, "character": 0},
                  "end":   {"line": 2, "character": 5}}
    })");
  auto p = j.get<SemanticTokensRangeParams>();
  EXPECT_EQ(p.textDocument.uri, "file:///a.cpp");
  EXPECT_EQ(p.range.start.line, 1u);
  EXPECT_EQ(p.range.end.character, 5u);
}

TEST(SemanticTokens, RoundTripWithResultId) {
  SemanticTokens t;
  t.resultId = "v1";
  t.data = {0, 0, 3, 1, 0};
  auto j = nlohmann::json(t);
  EXPECT_EQ(j["resultId"], "v1");
  EXPECT_EQ(j["data"][2], 3u);

  auto r = j.get<SemanticTokens>();
  ASSERT_TRUE(r.resultId.has_value());
  EXPECT_EQ(*r.resultId, "v1");
  EXPECT_EQ(r.data.size(), 5u);
}

TEST(SemanticTokens, OmitsResultIdWhenAbsent) {
  SemanticTokens t;
  t.data = {0, 0, 1, 0, 0};
  auto j = nlohmann::json(t);
  EXPECT_FALSE(j.contains("resultId"));
}

TEST(ServerCapabilities, SemanticTokensProviderSerialized) {
  ServerCapabilities c;
  SemanticTokensOptions o;
  o.legend.tokenTypes = {"keyword"};
  o.legend.tokenModifiers = {};
  o.full = true;
  c.semanticTokensProvider = o;

  auto j = nlohmann::json(c);
  ASSERT_TRUE(j.contains("semanticTokensProvider"));
  EXPECT_EQ(j["semanticTokensProvider"]["full"], true);
  EXPECT_EQ(j["semanticTokensProvider"]["legend"]["tokenTypes"][0], "keyword");
}

// ---- SemanticTokensBuilder -----------------------------------------------

TEST(SemanticTokensBuilder, EmptyProducesEmptyData) {
  SemanticTokensBuilder b;
  auto t = b.build();
  EXPECT_TRUE(t.data.empty());
  EXPECT_FALSE(t.resultId.has_value());
}

TEST(SemanticTokensBuilder, SkipsZeroLengthTokens) {
  SemanticTokensBuilder b;
  b.push(0, 0, 0, 1);
  EXPECT_EQ(b.size(), 0u);
  EXPECT_TRUE(b.build().data.empty());
}

TEST(SemanticTokensBuilder, SingleTokenAbsoluteEncoded) {
  SemanticTokensBuilder b;
  b.push(2, 4, 3, 7, 5);
  auto t = b.build();
  ASSERT_EQ(t.data.size(), 5u);
  EXPECT_EQ(t.data[0], 2u); // deltaLine = absolute line for first token
  EXPECT_EQ(t.data[1], 4u); // deltaStart = absolute startChar for first token
  EXPECT_EQ(t.data[2], 3u);
  EXPECT_EQ(t.data[3], 7u);
  EXPECT_EQ(t.data[4], 5u);
}

TEST(SemanticTokensBuilder, DeltaEncodesSameLine) {
  SemanticTokensBuilder b;
  b.push(0, 0, 3, 1);
  b.push(0, 5, 4, 2);
  auto t = b.build();
  ASSERT_EQ(t.data.size(), 10u);
  // first token: (0, 0, 3, 1, 0)
  EXPECT_EQ(t.data[0], 0u);
  EXPECT_EQ(t.data[1], 0u);
  // second token on same line: deltaLine=0, deltaStart=5
  EXPECT_EQ(t.data[5], 0u);
  EXPECT_EQ(t.data[6], 5u);
  EXPECT_EQ(t.data[7], 4u);
  EXPECT_EQ(t.data[8], 2u);
}

TEST(SemanticTokensBuilder, DeltaResetsCharOnNewLine) {
  SemanticTokensBuilder b;
  b.push(0, 10, 3, 1);
  b.push(2, 4, 5, 2);
  auto t = b.build();
  ASSERT_EQ(t.data.size(), 10u);
  // second token: deltaLine=2, deltaStart should be absolute (4) not 4-10
  EXPECT_EQ(t.data[5], 2u);
  EXPECT_EQ(t.data[6], 4u);
}

TEST(SemanticTokensBuilder, SortsOutOfOrderInput) {
  SemanticTokensBuilder b;
  b.push(3, 0, 2, 9);
  b.push(1, 8, 1, 8);
  b.push(1, 0, 4, 7);
  auto t = b.build();
  ASSERT_EQ(t.data.size(), 15u);
  // Sorted order: (1,0), (1,8), (3,0)
  // First: deltaLine=1, deltaStart=0
  EXPECT_EQ(t.data[0], 1u);
  EXPECT_EQ(t.data[1], 0u);
  EXPECT_EQ(t.data[3], 7u);
  // Second: same line, deltaStart=8
  EXPECT_EQ(t.data[5], 0u);
  EXPECT_EQ(t.data[6], 8u);
  EXPECT_EQ(t.data[8], 8u);
  // Third: deltaLine=2, deltaStart=0 (absolute, since deltaLine != 0)
  EXPECT_EQ(t.data[10], 2u);
  EXPECT_EQ(t.data[11], 0u);
  EXPECT_EQ(t.data[13], 9u);
}

TEST(SemanticTokensBuilder, ModifierBitmask) {
  SemanticTokensBuilder b;
  uint32_t mods = (1u << 0) | (1u << 3); // declaration + static, e.g.
  b.push(0, 0, 4, 12, mods);
  auto t = b.build();
  ASSERT_EQ(t.data.size(), 5u);
  EXPECT_EQ(t.data[4], mods);
}

TEST(SemanticTokensBuilder, ClearResetsState) {
  SemanticTokensBuilder b;
  b.push(0, 0, 3, 1);
  b.clear();
  EXPECT_TRUE(b.empty());
  b.push(5, 0, 2, 4);
  auto t = b.build();
  ASSERT_EQ(t.data.size(), 5u);
  EXPECT_EQ(t.data[0], 5u); // first-token absolute encoding restored
}