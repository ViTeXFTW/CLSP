#include <cctype>
#include <clsp/Uri.hpp>
#include <string>
#include <string_view>

namespace lsp::uri {

namespace {

constexpr std::string_view kFileScheme = "file://";

bool isUnreserved(unsigned char c) {
  return std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~';
}

// RFC 3986 `pchar` = unreserved / pct-encoded / sub-delims / ":" / "@".
// Path-passthrough preserves these plus '/' as the segment separator so that
// drive-letter colons and other path-legal characters survive encoding.
bool isPathPassThrough(unsigned char c) {
  if (isUnreserved(c) || c == '/' || c == ':' || c == '@')
    return true;
  switch (c) {
  case '!': case '$': case '&': case '\'':
  case '(': case ')': case '*': case '+':
  case ',': case ';': case '=':
    return true;
  default:
    return false;
  }
}

int hexValue(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F')
    return 10 + (c - 'A');
  return -1;
}

} // namespace

std::string percentEncode(std::string_view in) {
  std::string out;
  out.reserve(in.size());
  static constexpr char hex[] = "0123456789ABCDEF";
  for (unsigned char c : in) {
    if (isPathPassThrough(c)) {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(hex[(c >> 4) & 0xF]);
      out.push_back(hex[c & 0xF]);
    }
  }
  return out;
}

std::string percentDecode(std::string_view in) {
  std::string out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size(); ++i) {
    if (in[i] == '%' && i + 2 < in.size()) {
      int hi = hexValue(in[i + 1]);
      int lo = hexValue(in[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    out.push_back(in[i]);
  }
  return out;
}

std::string fromPath(const std::filesystem::path& path) {
  std::string generic = path.generic_string();
  std::string out = std::string(kFileScheme);
  if (!generic.empty() && generic[0] != '/') {
    // Windows-style absolute path: C:/foo → file:///C:/foo
    out += '/';
  }
  out += percentEncode(generic);
  return out;
}

std::filesystem::path toPath(std::string_view uri) {
  if (uri.substr(0, kFileScheme.size()) != kFileScheme) {
    return {};
  }
  std::string_view rest = uri.substr(kFileScheme.size());
  // Strip leading slash before a drive letter: /C:/foo → C:/foo
  if (rest.size() >= 3 && rest[0] == '/' && std::isalpha(static_cast<unsigned char>(rest[1])) &&
      rest[2] == ':') {
    rest.remove_prefix(1);
  }
  return std::filesystem::path(percentDecode(rest));
}

} // namespace lsp::uri
