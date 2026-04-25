#pragma once

#include <filesystem>
#include <string>

namespace lsp {

/**
 * Helpers for converting between LSP `file://` URIs and filesystem paths.
 *
 * The LSP spec uses RFC 3986 URIs. Paths are percent-encoded; characters in
 * the unreserved set are left as-is, plus `/` is preserved as a path
 * separator. On Windows, drive letters are emitted as `file:///C:/...`.
 */
namespace uri {

/**
 * Percent-encode a string for use inside a URI path. Unreserved characters
 * (A-Z, a-z, 0-9, '-', '.', '_', '~') and '/' are passed through; everything
 * else is encoded as %XX.
 */
std::string percentEncode(std::string_view in);

/**
 * Percent-decode a URI-encoded string. Invalid `%` escapes are passed
 * through verbatim.
 */
std::string percentDecode(std::string_view in);

/**
 * Convert a filesystem path to a `file://` URI. Backslashes are normalized
 * to forward slashes; on Windows, drive letters become `file:///C:/...`.
 */
std::string fromPath(const std::filesystem::path& path);

/**
 * Convert a `file://` URI back to a filesystem path. Returns an empty path
 * if the URI is not a `file://` URI.
 */
std::filesystem::path toPath(std::string_view uri);

} // namespace uri

} // namespace lsp
