#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace lsp {

/**
 * Code-unit interpretation for `Position.character`.
 *
 * LSP defaults to UTF-16. Servers can advertise alternatives via
 * `general.positionEncodings` negotiation in initialize.
 */
enum class PositionEncoding {
  UTF8,  // Position.character is a UTF-8 byte offset
  UTF16, // Position.character is a UTF-16 code-unit offset (LSP default)
  UTF32, // Position.character is a Unicode code-point offset
};

inline std::string to_string(PositionEncoding e) {
  switch (e) {
  case PositionEncoding::UTF8:
    return "utf-8";
  case PositionEncoding::UTF16:
    return "utf-16";
  case PositionEncoding::UTF32:
    return "utf-32";
  }
  return "utf-16";
}

inline std::optional<PositionEncoding>
positionEncodingFromString(std::string_view s) {
  if (s == "utf-8")
    return PositionEncoding::UTF8;
  if (s == "utf-16")
    return PositionEncoding::UTF16;
  if (s == "utf-32")
    return PositionEncoding::UTF32;
  return std::nullopt;
}

} // namespace lsp
