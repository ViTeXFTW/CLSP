#include <clsp/protocol/Hover.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace lsp;

TEST(MarkupKind, StringConversion) {
  EXPECT_STREQ(to_string(MarkupKind::PlainText), "plaintext");
  EXPECT_STREQ(to_string(MarkupKind::Markdown), "markdown");
  EXPECT_EQ(markupKindFromString("plaintext").value(), MarkupKind::PlainText);
  EXPECT_EQ(markupKindFromString("markdown").value(), MarkupKind::Markdown);
  EXPECT_FALSE(markupKindFromString("html").has_value());
}

TEST(MarkupContent, RoundTripMarkdown) {
  MarkupContent m{MarkupKind::Markdown, "**bold**"};
  auto j = nlohmann::json(m);
  EXPECT_EQ(j["kind"], "markdown");
  EXPECT_EQ(j["value"], "**bold**");

  auto r = j.get<MarkupContent>();
  EXPECT_EQ(r.kind, MarkupKind::Markdown);
  EXPECT_EQ(r.value, "**bold**");
}

TEST(MarkupContent, RoundTripPlaintext) {
  MarkupContent m{MarkupKind::PlainText, "x"};
  auto j = nlohmann::json(m);
  EXPECT_EQ(j["kind"], "plaintext");
  auto r = j.get<MarkupContent>();
  EXPECT_EQ(r.kind, MarkupKind::PlainText);
}

TEST(HoverParams, Deserialization) {
  auto j = nlohmann::json::parse(R"({
        "textDocument": {"uri": "file:///a.cpp"},
        "position": {"line": 4, "character": 7}
    })");
  auto p = j.get<HoverParams>();
  EXPECT_EQ(p.textDocument.uri, "file:///a.cpp");
  EXPECT_EQ(p.position.line, 4u);
  EXPECT_EQ(p.position.character, 7u);
}

TEST(HoverParams, RoundTrip) {
  HoverParams p;
  p.textDocument.uri = "file:///b.hpp";
  p.position = Position{1, 2};
  auto r = nlohmann::json(p).get<HoverParams>();
  EXPECT_EQ(r.textDocument.uri, "file:///b.hpp");
  EXPECT_EQ(r.position.line, 1u);
  EXPECT_EQ(r.position.character, 2u);
}

TEST(Hover, MinimalSerialization) {
  Hover h;
  h.contents = MarkupContent{MarkupKind::PlainText, "int"};
  auto j = nlohmann::json(h);
  EXPECT_EQ(j["contents"]["kind"], "plaintext");
  EXPECT_EQ(j["contents"]["value"], "int");
  EXPECT_FALSE(j.contains("range"));
}

TEST(Hover, WithRangeRoundTrip) {
  Hover h;
  h.contents = MarkupContent{MarkupKind::Markdown, "`foo`"};
  h.range = Range{{2, 0}, {2, 3}};
  auto j = nlohmann::json(h);
  ASSERT_TRUE(j.contains("range"));

  auto r = j.get<Hover>();
  EXPECT_EQ(r.contents.kind, MarkupKind::Markdown);
  EXPECT_EQ(r.contents.value, "`foo`");
  ASSERT_TRUE(r.range.has_value());
  EXPECT_EQ(r.range->start.line, 2u);
  EXPECT_EQ(r.range->end.character, 3u);
}
