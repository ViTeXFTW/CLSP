#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <variant>
#include <vector>

namespace lsp {

using DocumentUri = std::string;

struct Position {
  uint32_t line;
  uint32_t character;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Position, line, character)

struct Range {
  Position start;
  Position end;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Range, start, end)

struct LSPAny;
using LSPObject = std::map<std::string, LSPAny>;
using LSPArray = std::vector<LSPAny>;
struct LSPAny {
  std::variant<LSPObject, LSPArray, std::string, int32_t, uint32_t, double,
               bool, std::nullptr_t>
      value;
};

} // namespace lsp
