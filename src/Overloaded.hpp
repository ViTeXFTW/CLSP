#pragma once

namespace lsp {

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

} // namespace lsp
