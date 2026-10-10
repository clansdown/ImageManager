// SPDX-License-Identifier: GPL-3.0-only

#include "io/thumbnail_cache.h"

#include <array>
#include <cstddef>
#include <exception>
#include <format>
#include <utility>
#include <vector>

#include <glib.h>
#include <glibmm/miscutils.h>

#include "core/version.h"
#include "io/canonical_path.h"

namespace imagemgr {

namespace {

// The four size classes defined by the standard, ordered by increasing edge
// length; bucket_for_thumbnail_size() picks the first that fits.
constexpr std::array<ThumbnailBucket, 4> kThumbnailBuckets{{
    {"normal", 128},
    {"large", 256},
    {"x-large", 512},
    {"xx-large", 1024},
}};

// PNG text-chunk keys. "Thumb::" is the standard's namespace; Software is a
// predefined PNG key.
constexpr auto kUriKey = "Thumb::URI";
constexpr auto kMtimeKey = "Thumb::MTime";
constexpr auto kSizeKey = "Thumb::Size";
constexpr auto kMimetypeKey = "Thumb::Mimetype";
constexpr auto kSoftwareKey = "Software";

// gdk-pixbuf attaches PNG text chunks through options prefixed with "tEXt::".
std::string text_option(std::string_view key) {
    return std::format("tEXt::{}", key);
}

// Reads one PNG text chunk. Returns nullopt when the chunk is absent (an empty
// value is treated as absent). gdk-pixbuf exposes PNG text chunks under the
// same "tEXt::" prefix it accepts on save, so look up the prefixed key.
std::optional<std::string> read_option(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf,
                                       std::string_view key) {
    const std::string value = pixbuf->get_option(text_option(key));
    if (value.empty()) {
        return std::nullopt;
    }
    return value;
}

// Parses a decimal integer chunk (Thumb::MTime / Thumb::Size). Returns nullopt
// when absent or malformed.
std::optional<std::uint64_t> read_unsigned_option(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf,
                                                  std::string_view key) {
    const auto text = read_option(pixbuf, key);
    if (!text) {
        return std::nullopt;
    }
    try {
        std::size_t consumed = 0;
        const auto value = std::stoull(*text, &consumed);
        if (consumed != text->size()) {
            return std::nullopt;
        }
        return value;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace

ThumbnailBucket bucket_for_thumbnail_size(int size) {
    for (const auto& bucket : kThumbnailBuckets) {
        if (size <= bucket.dimension) {
            return bucket;
        }
    }
    return kThumbnailBuckets.back();
}

std::string default_thumbnail_cache_root() {
    return Glib::build_filename(g_get_user_cache_dir(), "thumbnails");
}

ThumbnailCache::ThumbnailCache(std::string root) : root_(std::move(root)) {}

bool ThumbnailCache::contains(const std::string& file_path) const {
    if (root_.empty()) {
        return false;
    }
    // Compare with the separator so a sibling like "/cache/thumbnails-other" is
    // not mistaken for an entry inside the cache.
    const std::string prefix = root_ + G_DIR_SEPARATOR_S;
    return file_path == root_ || file_path.starts_with(prefix);
}

std::string ThumbnailCache::file_path_for(ThumbnailBucket bucket, const std::string& uri) const {
    return Glib::build_filename(root_, std::string(bucket.directory_name),
                                md5_hex(uri) + ".png");
}

std::optional<Glib::RefPtr<Gdk::Pixbuf>> ThumbnailCache::load(ThumbnailBucket bucket,
                                                              const std::string& uri,
                                                              std::int64_t mtime,
                                                              std::uint64_t file_size) const {
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;
    try {
        pixbuf = Gdk::Pixbuf::create_from_file(file_path_for(bucket, uri));
    } catch (const Glib::Error&) {
        return std::nullopt; // Missing or undecodable: the caller regenerates.
    }
    if (!pixbuf) {
        return std::nullopt;
    }

    // Thumb::URI and Thumb::MTime must match; Thumb::Size is only checked when
    // the cached file carries it.
    const auto cached_uri = read_option(pixbuf, kUriKey);
    if (!cached_uri || *cached_uri != uri) {
        return std::nullopt;
    }
    const auto cached_mtime = read_unsigned_option(pixbuf, kMtimeKey);
    if (!cached_mtime || *cached_mtime != static_cast<std::uint64_t>(mtime)) {
        return std::nullopt;
    }
    if (const auto cached_size = read_unsigned_option(pixbuf, kSizeKey);
        cached_size && *cached_size != file_size) {
        return std::nullopt;
    }

    return pixbuf;
}

bool ThumbnailCache::store(ThumbnailBucket bucket, const std::string& uri,
                           std::int64_t mtime, std::uint64_t file_size,
                           const std::string& mimetype,
                           const Glib::RefPtr<Gdk::Pixbuf>& thumbnail) const {
    if (!thumbnail) {
        return false;
    }

    const std::string directory = Glib::build_filename(root_, std::string(bucket.directory_name));
    if (g_mkdir_with_parents(directory.c_str(), 0700) != 0) {
        g_warning("could not create thumbnail directory %s", directory.c_str());
        return false;
    }

    // The standard asks for a PNG with full alpha; add an opaque alpha channel
    // when the decoded thumbnail has none.
    const Glib::RefPtr<Gdk::Pixbuf> rgba =
        thumbnail->get_has_alpha() ? thumbnail : thumbnail->add_alpha(true, 0, 0, 0);

    std::vector<Glib::ustring> keys;
    std::vector<Glib::ustring> values;
    const auto add_chunk = [&keys, &values](const char* key, std::string value) {
        keys.emplace_back(text_option(key));
        values.emplace_back(std::move(value));
    };
    add_chunk(kUriKey, uri);
    add_chunk(kMtimeKey, std::to_string(mtime));
    add_chunk(kSizeKey, std::to_string(file_size));
    if (!mimetype.empty()) {
        add_chunk(kMimetypeKey, mimetype);
    }
    add_chunk(kSoftwareKey, std::format("{} {}", kApplicationName, kApplicationVersion));

    gchar* buffer = nullptr;
    gsize buffer_size = 0;
    try {
        rgba->save_to_buffer(buffer, buffer_size, "png", keys, values);
    } catch (const Glib::Error& error) {
        g_warning("could not encode thumbnail for %s: %s", uri.c_str(), error.what());
        return false;
    }

    // Atomic write (temporary file in the same directory, then rename) with the
    // standard's 0600 file mode.
    GError* error = nullptr;
    const gboolean written = g_file_set_contents_full(
        file_path_for(bucket, uri).c_str(), buffer, static_cast<gssize>(buffer_size),
        G_FILE_SET_CONTENTS_CONSISTENT, 0600, &error);
    g_free(buffer);

    if (!written) {
        g_warning("could not write thumbnail for %s: %s", uri.c_str(),
                  error != nullptr ? error->message : "unknown error");
        g_clear_error(&error);
        return false;
    }
    return true;
}

} // namespace imagemgr
