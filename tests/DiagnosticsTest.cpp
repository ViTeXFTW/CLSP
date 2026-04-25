#include <clsp/protocol/Diagnostics.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace lsp;

TEST(Diagnostic, MinimalSerialization) {
  Diagnostic d;
  d.range = Range{{0, 0}, {0, 5}};
  d.message = "oops";

  auto j = nlohmann::json(d);
  EXPECT_EQ(j["message"], "oops");
  EXPECT_EQ(j["range"]["start"]["line"], 0u);
  EXPECT_EQ(j["range"]["end"]["character"], 5u);
  EXPECT_FALSE(j.contains("severity"));
  EXPECT_FALSE(j.contains("code"));
  EXPECT_FALSE(j.contains("source"));
  EXPECT_FALSE(j.contains("tags"));
  EXPECT_FALSE(j.contains("relatedInformation"));
}

TEST(Diagnostic, SeverityEncodedAsInt) {
  Diagnostic d;
  d.range = Range{{1, 2}, {1, 4}};
  d.message = "warn";
  d.severity = DiagnosticSeverity::Warning;

  auto j = nlohmann::json(d);
  EXPECT_EQ(j["severity"].get<int>(),
            static_cast<int>(DiagnosticSeverity::Warning));
}

TEST(Diagnostic, IntCode) {
  Diagnostic d;
  d.range = Range{{0, 0}, {0, 1}};
  d.message = "x";
  d.code = int32_t{42};

  auto j = nlohmann::json(d);
  EXPECT_TRUE(j["code"].is_number_integer());
  EXPECT_EQ(j["code"].get<int>(), 42);
}

TEST(Diagnostic, StringCode) {
  Diagnostic d;
  d.range = Range{{0, 0}, {0, 1}};
  d.message = "x";
  d.code = std::string{"E0001"};

  auto j = nlohmann::json(d);
  EXPECT_TRUE(j["code"].is_string());
  EXPECT_EQ(j["code"].get<std::string>(), "E0001");
}

TEST(Diagnostic, TagsAsIntArray) {
  Diagnostic d;
  d.range = Range{{0, 0}, {0, 1}};
  d.message = "x";
  d.tags = std::vector<DiagnosticTag>{DiagnosticTag::Unnecessary,
                                      DiagnosticTag::Deprecated};

  auto j = nlohmann::json(d);
  ASSERT_TRUE(j["tags"].is_array());
  EXPECT_EQ(j["tags"][0].get<int>(),
            static_cast<int>(DiagnosticTag::Unnecessary));
  EXPECT_EQ(j["tags"][1].get<int>(),
            static_cast<int>(DiagnosticTag::Deprecated));
}

TEST(Diagnostic, RelatedInformationRoundTrip) {
  Diagnostic d;
  d.range = Range{{0, 0}, {0, 1}};
  d.message = "x";
  DiagnosticRelatedInformation rel;
  rel.location =
      Location{"file:///other.cpp", Range{{1, 0}, {1, 4}}};
  rel.message = "see here";
  d.relatedInformation = std::vector{rel};

  auto j = nlohmann::json(d);
  auto round = j.get<Diagnostic>();
  ASSERT_TRUE(round.relatedInformation.has_value());
  ASSERT_EQ(round.relatedInformation->size(), 1u);
  EXPECT_EQ((*round.relatedInformation)[0].location.uri, "file:///other.cpp");
  EXPECT_EQ((*round.relatedInformation)[0].message, "see here");
}

TEST(Diagnostic, FullRoundTrip) {
  Diagnostic d;
  d.range = Range{{2, 4}, {2, 9}};
  d.severity = DiagnosticSeverity::Error;
  d.code = std::string{"E001"};
  d.codeDescription = CodeDescription{"https://example.com/E001"};
  d.source = "clsp";
  d.message = "boom";
  d.tags = std::vector<DiagnosticTag>{DiagnosticTag::Deprecated};

  auto j = nlohmann::json(d);
  auto r = j.get<Diagnostic>();
  EXPECT_EQ(r.range.start.line, 2u);
  EXPECT_EQ(r.range.end.character, 9u);
  ASSERT_TRUE(r.severity.has_value());
  EXPECT_EQ(*r.severity, DiagnosticSeverity::Error);
  ASSERT_TRUE(r.code.has_value());
  EXPECT_EQ(std::get<std::string>(*r.code), "E001");
  ASSERT_TRUE(r.codeDescription.has_value());
  EXPECT_EQ(r.codeDescription->href, "https://example.com/E001");
  EXPECT_EQ(r.source.value_or(""), "clsp");
  EXPECT_EQ(r.message, "boom");
  ASSERT_TRUE(r.tags.has_value());
  EXPECT_EQ((*r.tags)[0], DiagnosticTag::Deprecated);
}

TEST(PublishDiagnosticsParams, EmptyArraySerializes) {
  PublishDiagnosticsParams p;
  p.uri = "file:///a.cpp";

  auto j = nlohmann::json(p);
  EXPECT_EQ(j["uri"], "file:///a.cpp");
  ASSERT_TRUE(j["diagnostics"].is_array());
  EXPECT_TRUE(j["diagnostics"].empty());
  EXPECT_FALSE(j.contains("version"));
}

TEST(PublishDiagnosticsParams, WithVersionAndDiagnostics) {
  PublishDiagnosticsParams p;
  p.uri = "file:///a.cpp";
  p.version = 7;
  Diagnostic d;
  d.range = Range{{0, 0}, {0, 3}};
  d.message = "bad";
  d.severity = DiagnosticSeverity::Error;
  p.diagnostics.push_back(d);

  auto j = nlohmann::json(p);
  EXPECT_EQ(j["version"].get<int>(), 7);
  ASSERT_EQ(j["diagnostics"].size(), 1u);
  EXPECT_EQ(j["diagnostics"][0]["message"], "bad");

  auto r = j.get<PublishDiagnosticsParams>();
  EXPECT_EQ(r.uri, "file:///a.cpp");
  EXPECT_EQ(r.version.value_or(-1), 7);
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(r.diagnostics[0].message, "bad");
}
