#include "DocumentStore.hpp"
#include <clsp/PositionEncoding.hpp>
#include <clsp/protocol/Basic.hpp>
#include <clsp/protocol/Documents.hpp>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace lsp;

namespace {

// Test shim exposing the protected positionToOffset for verification.
class TestableStore : public DocumentStore {
public:
  size_t offset(const std::string& text, const Position& pos) {
    return DocumentStore::positionToOffset(text, pos);
  }
};

const std::string kAscii = "hello\nworld\n";
// "héllo" — 'é' is U+00E9, UTF-8 bytes 0xC3 0xA9 (1 UTF-16 cu, 1 UTF-32 cp,
// 2 UTF-8 bytes)
const std::string kBmp = "h\xC3\xA9llo\nworld\n";
// "🙂world" — 🙂 is U+1F642, UTF-8 bytes F0 9F 99 82 (2 UTF-16 cu, 1 UTF-32
// cp, 4 UTF-8 bytes)
const std::string kAstral = "\xF0\x9F\x99\x82world\n";

} // namespace

// ── Enum string conversion
// ────────────────────────────────────────────────

TEST(PositionEncodingString, ToAndFrom) {
  EXPECT_EQ(to_string(PositionEncoding::UTF8), "utf-8");
  EXPECT_EQ(to_string(PositionEncoding::UTF16), "utf-16");
  EXPECT_EQ(to_string(PositionEncoding::UTF32), "utf-32");

  EXPECT_EQ(*positionEncodingFromString("utf-8"), PositionEncoding::UTF8);
  EXPECT_EQ(*positionEncodingFromString("utf-16"), PositionEncoding::UTF16);
  EXPECT_EQ(*positionEncodingFromString("utf-32"), PositionEncoding::UTF32);
  EXPECT_FALSE(positionEncodingFromString("utf-7").has_value());
}

// ── Default encoding is UTF-16
// ────────────────────────────────────────────

TEST(PositionEncodingDefault, IsUtf16) {
  TestableStore s;
  EXPECT_EQ(s.positionEncoding(), PositionEncoding::UTF16);
}

// ── ASCII text behaves the same in every encoding
// ────────────────────────

TEST(PositionToOffset, AsciiUtf8) {
  TestableStore s;
  s.setPositionEncoding(PositionEncoding::UTF8);
  EXPECT_EQ(s.offset(kAscii, {0, 0}), 0u);
  EXPECT_EQ(s.offset(kAscii, {0, 5}), 5u);
  EXPECT_EQ(s.offset(kAscii, {1, 0}), 6u);
  EXPECT_EQ(s.offset(kAscii, {1, 3}), 9u);
}

TEST(PositionToOffset, AsciiUtf16) {
  TestableStore s;
  s.setPositionEncoding(PositionEncoding::UTF16);
  EXPECT_EQ(s.offset(kAscii, {0, 5}), 5u);
  EXPECT_EQ(s.offset(kAscii, {1, 3}), 9u);
}

TEST(PositionToOffset, AsciiUtf32) {
  TestableStore s;
  s.setPositionEncoding(PositionEncoding::UTF32);
  EXPECT_EQ(s.offset(kAscii, {0, 5}), 5u);
  EXPECT_EQ(s.offset(kAscii, {1, 3}), 9u);
}

// ── BMP-only multi-byte ('é' = 1 UTF-16 cu, 2 bytes)
// ──────────────────────

TEST(PositionToOffset, BmpUtf8) {
  TestableStore s;
  s.setPositionEncoding(PositionEncoding::UTF8);
  // After 3 UTF-8 bytes (h, é first byte, é second byte) we point at 'l'
  EXPECT_EQ(s.offset(kBmp, {0, 3}), 3u);
}

TEST(PositionToOffset, BmpUtf16) {
  TestableStore s;
  s.setPositionEncoding(PositionEncoding::UTF16);
  // 2 UTF-16 code units (h, é) → byte offset 3 (h=1 byte, é=2 bytes)
  EXPECT_EQ(s.offset(kBmp, {0, 2}), 3u);
}

TEST(PositionToOffset, BmpUtf32) {
  TestableStore s;
  s.setPositionEncoding(PositionEncoding::UTF32);
  // 2 code points (h, é) → byte offset 3
  EXPECT_EQ(s.offset(kBmp, {0, 2}), 3u);
}

// ── Astral plane ('🙂' = 2 UTF-16 cu, 1 UTF-32 cp, 4 bytes)
// ───────────────

TEST(PositionToOffset, AstralUtf8) {
  TestableStore s;
  s.setPositionEncoding(PositionEncoding::UTF8);
  EXPECT_EQ(s.offset(kAstral, {0, 4}), 4u);
}

TEST(PositionToOffset, AstralUtf16) {
  TestableStore s;
  s.setPositionEncoding(PositionEncoding::UTF16);
  // Position character=2 means past the surrogate pair → 4 bytes consumed
  EXPECT_EQ(s.offset(kAstral, {0, 2}), 4u);
}

TEST(PositionToOffset, AstralUtf32) {
  TestableStore s;
  s.setPositionEncoding(PositionEncoding::UTF32);
  // 1 code point past = past the 4 UTF-8 bytes
  EXPECT_EQ(s.offset(kAstral, {0, 1}), 4u);
}

// ── Character offset past line end clamps at newline
// ──────────────────────

TEST(PositionToOffset, CharacterPastEndStopsAtNewline) {
  TestableStore s;
  s.setPositionEncoding(PositionEncoding::UTF16);
  // Line 0 of kAscii is "hello" (5 chars) followed by '\n' at offset 5.
  // Asking for character=99 should not advance past the newline.
  EXPECT_EQ(s.offset(kAscii, {0, 99}), 5u);
}

// ── ClientCapabilities deserialization picks up positionEncodings
// ────────

TEST(ClientCapabilitiesPositionEncodings, ParsedFromJson) {
  auto j = nlohmann::json::parse(R"({
    "general": {"positionEncodings": ["utf-8", "utf-16"]}
  })");
  auto c = j.get<ClientCapabilities>();
  ASSERT_TRUE(c.general.has_value());
  ASSERT_TRUE(c.general->positionEncodings.has_value());
  EXPECT_EQ((*c.general->positionEncodings)[0], "utf-8");
  EXPECT_EQ((*c.general->positionEncodings)[1], "utf-16");
}

// ── ServerCapabilities serializes positionEncoding
// ──────────────────────────

TEST(ServerCapabilitiesPositionEncoding, Serialized) {
  ServerCapabilities c{};
  c.positionEncoding = "utf-8";
  auto j = nlohmann::json(c);
  EXPECT_EQ(j["positionEncoding"], "utf-8");
}
