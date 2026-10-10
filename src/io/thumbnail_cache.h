// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <gdkmm/pixbuf.h>
#include <glibmm/refptr.h>

namespace imagemgr {

/// One size class of the freedesktop Thumbnail Managing Standard: the cache
/// subdirectory and the edge length its thumbnails must fit within.
struct ThumbnailBucket {
    std::string_view directory_name; // "normal", "large", "x-large", "xx-large"
    int dimension;                   // 128, 256, 512, 1024
};

/// The bucket a requested maximum edge length maps to: the smallest bucket that
/// fits, clamped to xx-large for sizes beyond 1024.
[[nodiscard]] ThumbnailBucket bucket_for_thumbnail_size(int size);

/// The canonical file: URI used as the cache key for a local path. The standard
/// hashes this exact string, so it must be the canonical (percent-encoded) form
/// that Gio produces.
[[nodiscard]] std::string thumbnail_source_uri(const std::string& file_path);

/// Lowercase MD5 hex of a canonical URI: the thumbnail's file name stem.
[[nodiscard]] std::string thumbnail_key(const std::string& uri);

/// The user's global thumbnail cache root, $XDG_CACHE_HOME/thumbnails (or
/// $HOME/.cache/thumbnails).
[[nodiscard]] std::string default_thumbnail_cache_root();

/// Reads and writes freedesktop thumbnails under a cache root.
///
/// load() returns a thumbnail only when its embedded Thumb::URI and Thumb::MTime
/// match the source (and Thumb::Size when the cached file carries it). store()
/// writes the required PNG text chunks atomically, with the directory at mode
/// 0700 and the file at 0600, as the standard requires. The root is injectable
/// so tests can point at a temporary directory.
///
/// Thread-safety: all operations are const and share only the immutable root_;
/// concurrent writers of the same entry are made safe by the atomic rename.
class ThumbnailCache {
public:
    explicit ThumbnailCache(std::string root = default_thumbnail_cache_root());

    /// Whether path lies inside the cache. The standard forbids thumbnailing
    /// cache files themselves, so callers skip the cache for such paths.
    [[nodiscard]] bool contains(const std::string& file_path) const;

    /// Returns the cached thumbnail for uri when it exists and is still valid
    /// for (mtime, file_size); nullopt when missing, undecodable or stale.
    [[nodiscard]] std::optional<Glib::RefPtr<Gdk::Pixbuf>> load(
        ThumbnailBucket bucket, const std::string& uri, std::int64_t mtime,
        std::uint64_t file_size) const;

    /// Writes thumbnail as <root>/<bucket>/<md5(uri)>.png with the required text
    /// chunks. mimetype is stored as Thumb::Mimetype when non-empty. Returns
    /// false on I/O failure; the caller's in-memory thumbnail is unaffected.
    [[nodiscard]] bool store(ThumbnailBucket bucket, const std::string& uri,
                             std::int64_t mtime, std::uint64_t file_size,
                             const std::string& mimetype,
                             const Glib::RefPtr<Gdk::Pixbuf>& thumbnail) const;

private:
    /// The on-disk path of the thumbnail for uri in the given bucket.
    [[nodiscard]] std::string file_path_for(ThumbnailBucket bucket,
                                            const std::string& uri) const;

    std::string root_;
};

} // namespace imagemgr
