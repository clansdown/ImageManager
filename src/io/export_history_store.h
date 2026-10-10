// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "core/error.h"
#include "core/export_history.h"

namespace imagemgr {

/// The user's export-history root: $XDG_STATE_HOME/imagemgr/exports (usually
/// ~/.local/state/imagemgr/exports).
[[nodiscard]] std::string default_export_history_root();

/// The JSON file name stem for a source path:
/// md5_hex(canonical_file_uri(source_path)).
[[nodiscard]] std::string export_history_key(const std::string& source_path);

/// Serialises one source's history to a JSON string.
[[nodiscard]] std::string export_history_to_json_string(const ExportHistory& history);

/// Parses a JSON string produced by export_history_to_json_string(). Returns an
/// Error for malformed JSON or an unsupported schema version.
[[nodiscard]] std::expected<ExportHistory, Error> export_history_from_json_string(
    std::string_view json);

/// Reads and writes per-source export history under a root.
///
/// One JSON file per source image, named by export_history_key(), holding every
/// region exported from that source and where it went. The root is injectable
/// so tests can point at a temporary directory.
///
/// Thread-safety: all operations are const and share only the immutable root_;
/// writes are atomic (temporary file + rename), so a concurrent reader sees
/// either the old file or the new one, never a partial write.
class ExportHistoryStore {
public:
    explicit ExportHistoryStore(std::string root = default_export_history_root());

    /// The on-disk JSON path for source_path.
    [[nodiscard]] std::string file_path_for(const std::string& source_path) const;

    /// Whether a history file exists for source_path.
    [[nodiscard]] bool contains(const std::string& source_path) const;

    /// Full history for source_path. A missing file yields an empty optional;
    /// an unreadable or malformed file yields an Error.
    [[nodiscard]] std::expected<std::optional<ExportHistory>, Error> load(
        const std::string& source_path) const;

    /// Number of recorded exports (0 when there is no file or it is unreadable).
    [[nodiscard]] std::size_t count(const std::string& source_path) const;

    /// Appends record to source_path's history, creating the file if needed.
    /// source_modified_unix_seconds is the source file's mtime at export time.
    /// Refuses to overwrite an existing unparsable file (returns Error).
    [[nodiscard]] std::expected<void, Error> append(const std::string& source_path,
                                                    std::int64_t source_modified_unix_seconds,
                                                    const ExportRecord& record) const;

    /// Overwrites history.source_path's file atomically, without reading the
    /// existing one. Used when a rotation rewrites every stored region.
    [[nodiscard]] std::expected<void, Error> save(const ExportHistory& history) const;

private:
    std::string root_;
};

} // namespace imagemgr
