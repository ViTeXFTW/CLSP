#include <clsp/protocol/Completion.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace lsp;

TEST(CompletionParams, DeserializationMinimal) {
  auto j = nlohmann::json::parse(R"({
        "textDocument": {"uri": "file:///a.cpp"},
        "position": {"line": 4, "character": 7}
    })");
  auto p = j.get<CompletionParams>();
  EXPECT_EQ(p.textDocument.uri, "file:///a.cpp");
  EXPECT_EQ(p.position.line, 4u);
  EXPECT_EQ(p.position.character, 7u);
  EXPECT_FALSE(p.context.has_value());
}

TEST(CompletionParams, DeserializationWithContext) {
  auto j = nlohmann::json::parse(R"({
        "textDocument": {"uri": "file:///a.cpp"},
        "position": {"line": 0, "character": 0},
        "context": {"triggerKind": 2, "triggerCharacter": "."}
    })");
  auto p = j.get<CompletionParams>();
  ASSERT_TRUE(p.context.has_value());
  EXPECT_EQ(p.context->triggerKind, CompletionTriggerKind::TriggerCharacter);
  ASSERT_TRUE(p.context->triggerCharacter.has_value());
  EXPECT_EQ(*p.context->triggerCharacter, ".");
}

TEST(CompletionParams, RoundTrip) {
  CompletionParams p;
  p.textDocument.uri = "file:///b.hpp";
  p.position = Position{3, 9};
  CompletionContext ctx;
  ctx.triggerKind = CompletionTriggerKind::TriggerForIncompleteCompletions;
  ctx.triggerCharacter = "::";
  p.context = ctx;

  auto r = nlohmann::json(p).get<CompletionParams>();
  EXPECT_EQ(r.textDocument.uri, "file:///b.hpp");
  EXPECT_EQ(r.position.line, 3u);
  EXPECT_EQ(r.position.character, 9u);
  ASSERT_TRUE(r.context.has_value());
  EXPECT_EQ(r.context->triggerKind,
            CompletionTriggerKind::TriggerForIncompleteCompletions);
  EXPECT_EQ(*r.context->triggerCharacter, "::");
}

TEST(CompletionItem, MinimalSerialization) {
  CompletionItem i;
  i.label = "foo";
  auto j = nlohmann::json(i);
  EXPECT_EQ(j["label"], "foo");
  EXPECT_FALSE(j.contains("kind"));
  EXPECT_FALSE(j.contains("documentation"));
  EXPECT_FALSE(j.contains("textEdit"));
}

TEST(CompletionItem, FullRoundTrip) {
  CompletionItem i;
  i.label = "vector";
  i.kind = CompletionItemKind::Class;
  i.tags = std::vector<CompletionItemTag>{CompletionItemTag::Deprecated};
  i.detail = "std::vector<T>";
  i.documentation =
      MarkupContent{MarkupKind::Markdown, "**dynamic** array"};
  i.preselect = true;
  i.sortText = "00_vector";
  i.filterText = "vec";
  i.insertText = "vector<$1>$0";
  i.insertTextFormat = InsertTextFormat::Snippet;
  i.textEdit = TextEdit{Range{{1, 0}, {1, 3}}, "vector"};
  i.additionalTextEdits = std::vector<TextEdit>{
      TextEdit{Range{{0, 0}, {0, 0}}, "#include <vector>\n"}};
  i.commitCharacters = std::vector<std::string>{"<", "("};
  i.data = nlohmann::json{{"id", 42}};

  auto j = nlohmann::json(i);
  EXPECT_EQ(j["kind"], static_cast<int>(CompletionItemKind::Class));
  EXPECT_EQ(j["tags"][0],
            static_cast<int>(CompletionItemTag::Deprecated));
  EXPECT_EQ(j["documentation"]["kind"], "markdown");
  EXPECT_EQ(j["insertTextFormat"],
            static_cast<int>(InsertTextFormat::Snippet));
  EXPECT_EQ(j["textEdit"]["newText"], "vector");
  EXPECT_EQ(j["additionalTextEdits"][0]["newText"], "#include <vector>\n");
  EXPECT_EQ(j["commitCharacters"][0], "<");
  EXPECT_EQ(j["data"]["id"], 42);

  auto r = j.get<CompletionItem>();
  EXPECT_EQ(r.label, "vector");
  ASSERT_TRUE(r.kind.has_value());
  EXPECT_EQ(*r.kind, CompletionItemKind::Class);
  ASSERT_TRUE(r.tags.has_value());
  ASSERT_EQ(r.tags->size(), 1u);
  EXPECT_EQ((*r.tags)[0], CompletionItemTag::Deprecated);
  ASSERT_TRUE(r.documentation.has_value());
  ASSERT_TRUE(std::holds_alternative<MarkupContent>(*r.documentation));
  EXPECT_EQ(std::get<MarkupContent>(*r.documentation).kind,
            MarkupKind::Markdown);
  EXPECT_EQ(*r.preselect, true);
  EXPECT_EQ(*r.insertTextFormat, InsertTextFormat::Snippet);
  ASSERT_TRUE(r.textEdit.has_value());
  EXPECT_EQ(r.textEdit->newText, "vector");
  ASSERT_TRUE(r.additionalTextEdits.has_value());
  EXPECT_EQ(r.additionalTextEdits->size(), 1u);
  ASSERT_TRUE(r.commitCharacters.has_value());
  EXPECT_EQ((*r.commitCharacters)[1], "(");
  ASSERT_TRUE(r.data.has_value());
  EXPECT_EQ((*r.data)["id"], 42);
}

TEST(CompletionItem, StringDocumentation) {
  CompletionItem i;
  i.label = "foo";
  i.documentation = std::string{"plain docs"};
  auto j = nlohmann::json(i);
  EXPECT_TRUE(j["documentation"].is_string());
  EXPECT_EQ(j["documentation"], "plain docs");

  auto r = j.get<CompletionItem>();
  ASSERT_TRUE(r.documentation.has_value());
  ASSERT_TRUE(std::holds_alternative<std::string>(*r.documentation));
  EXPECT_EQ(std::get<std::string>(*r.documentation), "plain docs");
}

TEST(CompletionList, RoundTrip) {
  CompletionList l;
  l.isIncomplete = true;
  CompletionItem a;
  a.label = "alpha";
  CompletionItem b;
  b.label = "beta";
  b.kind = CompletionItemKind::Function;
  l.items = {a, b};

  auto j = nlohmann::json(l);
  EXPECT_EQ(j["isIncomplete"], true);
  EXPECT_EQ(j["items"].size(), 2u);

  auto r = j.get<CompletionList>();
  EXPECT_TRUE(r.isIncomplete);
  ASSERT_EQ(r.items.size(), 2u);
  EXPECT_EQ(r.items[0].label, "alpha");
  EXPECT_EQ(r.items[1].label, "beta");
  ASSERT_TRUE(r.items[1].kind.has_value());
  EXPECT_EQ(*r.items[1].kind, CompletionItemKind::Function);
}

TEST(TextEdit, RoundTrip) {
  TextEdit e{Range{{2, 1}, {2, 4}}, "foo"};
  auto j = nlohmann::json(e);
  auto r = j.get<TextEdit>();
  EXPECT_EQ(r.range.start.line, 2u);
  EXPECT_EQ(r.range.start.character, 1u);
  EXPECT_EQ(r.range.end.character, 4u);
  EXPECT_EQ(r.newText, "foo");
}
