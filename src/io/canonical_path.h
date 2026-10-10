// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>
#include <string_view>

namespace imagemgr {

/// Canonical, percent-encoded file: URI for a local path.
///
/// This is the exact string the freedesktop Thumbnail Managing Standard hashes
/// for its cache file names, and what the export history hashes too, so both
/// caches agree on the identity of a file. Uses Gio's canonical form.
///
/// Thread-safety: pure function; safe to call from any thread.
[[nodiscard]] std::string canonical_file_uri(const std::string& file_path);

/// Lowercase MD5 hex digest of text.
///
/// text may be any byte range (embedded NULs included); its full length is
/// hashed.
///
/// Thread-safety: pure function; safe to call from any thread.
[[nodiscard]] std::string md5_hex(std::string_view text);

} // namespace imagemgr
