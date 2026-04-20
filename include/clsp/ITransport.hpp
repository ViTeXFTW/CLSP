#pragma once

#include <optional>
#include <string>

namespace lsp {

/**
 * Pure byte transport - reads and writes LSP-framed messages.
 */
class ITransport {
public:
  virtual ~ITransport() = default;

  /**
   * Blocks until a complete LSP message is available.
   * Returns the raw JSON body, or nullptr on EOF/error.
   */
  virtual std::optional<std::string> readMessage() = 0;

  /**
   * Sends a raw JSON string, wrapping it with Content-Length header.
   */
  virtual void sendMessage(const std::string &body) = 0;
};

} // namespace lsp
