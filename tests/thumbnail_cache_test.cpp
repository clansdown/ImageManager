// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>

#include <gdkmm/pixbuf.h>

#include "io/canonical_path.h"
#include "io/thumbnail_cache.h"

using namespace imagemgr;

namespace {

// A cache root under the system temp directory, removed by the caller.
std::string unique_cache_root() {
    static std::atomic<int> counter{0};
    const int index = counter.fetch_add(1);
    return (std::filesystem::temp_directory_path()
            / std::format("imagemgr-thumb-cache-{}-{}", ::getpid(), index))
        .string();
}

Glib::RefPtr<Gdk::Pixbuf> make_thumbnail() {
    auto pixbuf = Gdk::Pixbuf::create(Gdk::Colorspace::RGB, true, 8, 16, 12);
    pixbuf->fill(0x3366ccff);
    return pixbuf;
}

} // namespace

TEST_CASE("thumbnail cache: size buckets follow the standard", "[thumbnail-cache]") {
    REQUIRE(bucket_for_thumbnail_size(16).directory_name == "normal");
    REQUIRE(bucket_for_thumbnail_size(128).directory_name == "normal");
    REQUIRE(bucket_for_thumbnail_size(129).directory_name == "large");
    REQUIRE(bucket_for_thumbnail_size(256).directory_name == "large");
    REQUIRE(bucket_for_thumbnail_size(257).directory_name == "x-large");
    REQUIRE(bucket_for_thumbnail_size(512).directory_name == "x-large");
    REQUIRE(bucket_for_thumbnail_size(513).directory_name == "xx-large");
    REQUIRE(bucket_for_thumbnail_size(1024).directory_name == "xx-large");
    REQUIRE(bucket_for_thumbnail_size(4096).directory_name == "xx-large");

    REQUIRE(bucket_for_thumbnail_size(128).dimension == 128);
    REQUIRE(bucket_for_thumbnail_size(1024).dimension == 1024);
}

TEST_CASE("thumbnail cache: keys use the canonical URI", "[thumbnail-cache]") {
    // The standard's own worked example.
    REQUIRE(canonical_file_uri("/home/jens/photos/me.png") ==
            "file:///home/jens/photos/me.png");
    REQUIRE(md5_hex("file:///home/jens/photos/me.png") ==
            "c6ee772d9e49320e97ec29a7eb5b1697");
}

TEST_CASE("thumbnail cache: URIs are percent-encoded", "[thumbnail-cache]") {
    const std::string uri = canonical_file_uri("/tmp/a picture.png");
    REQUIRE(uri == "file:///tmp/a%20picture.png");
    // The encoded URI and the raw path must hash differently.
    REQUIRE(md5_hex(uri) != md5_hex("file:///tmp/a picture.png"));
}

TEST_CASE("thumbnail cache: store then load round-trips", "[thumbnail-cache]") {
    const std::string root = unique_cache_root();
    const ThumbnailCache cache(root);
    const ThumbnailBucket bucket = bucket_for_thumbnail_size(128);
    const std::string uri = "file:///tmp/imagemgr-thumb-roundtrip.png";
    const std::int64_t mtime = 1700000000;
    const std::uint64_t file_size = 4096;

    REQUIRE(cache.store(bucket, uri, mtime, file_size, "image/png", make_thumbnail()));

    const auto loaded = cache.load(bucket, uri, mtime, file_size);
    REQUIRE(loaded.has_value());
    REQUIRE((*loaded)->get_width() == 16);
    REQUIRE((*loaded)->get_height() == 12);

    // A changed mtime or size invalidates the entry.
    REQUIRE_FALSE(cache.load(bucket, uri, mtime + 1, file_size).has_value());
    REQUIRE_FALSE(cache.load(bucket, uri, mtime, file_size + 1).has_value());
    // A different URI is a different entry.
    REQUIRE_FALSE(cache.load(bucket, uri + "x", mtime, file_size).has_value());

    std::error_code error;
    std::filesystem::remove_all(root, error);
}

TEST_CASE("thumbnail cache: contains recognises cache paths", "[thumbnail-cache]") {
    const ThumbnailCache cache("/tmp/imagemgr-cache-root");
    REQUIRE(cache.contains("/tmp/imagemgr-cache-root/normal/abc.png"));
    REQUIRE(cache.contains("/tmp/imagemgr-cache-root"));
    REQUIRE_FALSE(cache.contains("/tmp/imagemgr-cache-root-other/normal/abc.png"));
    REQUIRE_FALSE(cache.contains("/tmp/elsewhere/photo.png"));
}
