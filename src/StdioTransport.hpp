#pragma once

#include <clsp/ITransport.hpp>
#include <cstdint>
#include <iostream>
#include <istream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>

namespace lsp {

class StdioTransport : public ITransport {
public:
  StdioTransport(std::istream* in, std::ostream* out);

  std::optional<std::string> readMessage() override;
  void sendMessage(const std::string& body) override;

private:
  std::optional<int32_t> readHeaders();

  std::istream* in_;
  std::ostream* out_;
};

inline std::unique_ptr<StdioTransport> makeStdioTransport() {
  return std::make_unique<StdioTransport>(&std::cin, &std::cout);
}

} // namespace lsp
