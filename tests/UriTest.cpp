#include <clsp/Uri.hpp>
#include <gtest/gtest.h>

using namespace lsp;

// ── percentEncode / percentDecode
// ────────────────────────────────────────

TEST(UriPercentEncode, UnreservedPassThrough) {
  EXPECT_EQ(uri::percentEncode("abcXYZ-._~123"), "abcXYZ-._~123");
}

TEST(UriPercentEncode, SlashIsPreserved) {
  EXPECT_EQ(uri::percentEncode("a/b/c"), "a/b/c");
}

TEST(UriPercentEncode, SpaceEncoded) {
  EXPECT_EQ(uri::percentEncode("hello world"), "hello%20world");
}

TEST(UriPercentEncode, ReservedEncoded) {
  EXPECT_EQ(uri::percentEncode("a?b#c"), "a%3Fb%23c");
}

TEST(UriPercentDecode, RoundTrip) {
  std::string original = "spaces and #tags?";
  std::string encoded = uri::percentEncode(original);
  EXPECT_EQ(uri::percentDecode(encoded), original);
}

TEST(UriPercentDecode, InvalidEscapePassesThrough) {
  EXPECT_EQ(uri::percentDecode("a%ZZb"), "a%ZZb");
}

TEST(UriPercentDecode, TruncatedEscapePassesThrough) {
  EXPECT_EQ(uri::percentDecode("trail%4"), "trail%4");
}

TEST(UriPercentDecode, LowercaseHex) {
  EXPECT_EQ(uri::percentDecode("a%20b"), "a b");
  EXPECT_EQ(uri::percentDecode("a%2fb"), "a/b");
}

// ── fromPath / toPath
// ──────────────────────────────────────────────────────────

TEST(UriFromPath, WindowsDriveLetter) {
  auto u = uri::fromPath("C:/Users/me/file.cpp");
  EXPECT_EQ(u, "file:///C:/Users/me/file.cpp");
}

TEST(UriFromPath, UnixAbsolutePath) {
  auto u = uri::fromPath("/home/me/file.cpp");
  EXPECT_EQ(u, "file:///home/me/file.cpp");
}

TEST(UriFromPath, BackslashesNormalized) {
  auto u = uri::fromPath(std::filesystem::path("C:\\Users\\me\\file.cpp"));
  EXPECT_EQ(u, "file:///C:/Users/me/file.cpp");
}

TEST(UriFromPath, SpacesEncoded) {
  auto u = uri::fromPath("/home/me/Some File.cpp");
  EXPECT_EQ(u, "file:///home/me/Some%20File.cpp");
}

TEST(UriToPath, NonFileSchemeReturnsEmpty) {
  EXPECT_TRUE(uri::toPath("https://example.com/x").empty());
}

TEST(UriToPath, WindowsDriveLetter) {
  auto p = uri::toPath("file:///C:/Users/me/file.cpp");
  EXPECT_EQ(p.generic_string(), "C:/Users/me/file.cpp");
}

TEST(UriToPath, UnixAbsolutePath) {
  auto p = uri::toPath("file:///home/me/file.cpp");
  EXPECT_EQ(p.generic_string(), "/home/me/file.cpp");
}

TEST(UriToPath, PercentDecoded) {
  auto p = uri::toPath("file:///home/me/Some%20File.cpp");
  EXPECT_EQ(p.generic_string(), "/home/me/Some File.cpp");
}

TEST(UriRoundTrip, WindowsPath) {
  std::filesystem::path p = "C:/Users/Test User/main.cpp";
  auto u = uri::fromPath(p);
  auto p2 = uri::toPath(u);
  EXPECT_EQ(p2.generic_string(), p.generic_string());
}

TEST(UriRoundTrip, UnixPath) {
  std::filesystem::path p = "/usr/local/share/My File.txt";
  auto u = uri::fromPath(p);
  auto p2 = uri::toPath(u);
  EXPECT_EQ(p2.generic_string(), p.generic_string());
}
