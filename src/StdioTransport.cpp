#include "StdioTransport.hpp"

namespace lsp {

StdioTransport::StdioTransport(std::istream *in, std::ostream *out)
    : in_(in), out_(out) {}

std::optional<int32_t> StdioTransport::readHeaders() {
  if (in_->eof() || in_->fail()) {
    return std::nullopt;
  }

  std::string headers;
  headers.reserve(64);

  char c;
  while (true) {
    in_->read(&c, 1);
    headers += c;
    if (headers.size() >= 4 && headers[headers.size() - 4] == '\r' &&
        headers[headers.size() - 3] == '\n' &&
        headers[headers.size() - 2] == '\r' &&
        headers[headers.size() - 1] == '\n') {
      break;
    }
  }

  constexpr std::string_view key = "Content-Length: ";
  auto pos = headers.find(key);
  if (pos == std::string_view::npos) {
    return std::nullopt;
  }

  auto valueStart = pos + key.size();
  auto valueEnd = headers.find('\r', valueStart);
  if (valueEnd == std::string_view::npos) {
    valueEnd = headers.size();
  }

  std::string valueStr = headers.substr(valueStart, valueEnd - valueStart);
  if (valueStr.empty()) {
    return std::nullopt;
  }

  int32_t contentLength = 0;
  for (char c : valueStr) {
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
    contentLength = contentLength * 10 + static_cast<size_t>(c - '0');
  }

  return contentLength;
}

std::optional<std::string> StdioTransport::readMessage() {
  auto contentLength = readHeaders();
  if (contentLength == std::nullopt) {
    return std::nullopt;
  }

  std::string body(contentLength.value(), '\0');
  in_->read(body.data(), contentLength.value());

  if (in_->fail()) {
    return std::nullopt;
  }

  return body;
}

void StdioTransport::sendMessage(const std::string &body) {
  *out_ << "Content-Length: " << body.size() << "\r\n\r\n" << body;
  out_->flush();
}

} // namespace lsp
