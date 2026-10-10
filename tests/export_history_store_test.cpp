// SPDX-License-Identifier: GPL-3.0-only

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

#include "core/export_history.h"
#include "io/export_history_store.h"

using namespace imagemgr;

namespace {

// A history root under the system temp directory, removed by the caller.
std::string unique_history_root() {
    static std::atomic<int> counter{0};
    const int index = counter.fetch_add(1);
    return (std::filesystem::temp_directory_path()
            / std::format("imagemgr-export-history-{}-{}", ::getpid(), index))
        .string();
}

ExportRecord make_record(std::string id, ImageRectangle region, std::string output_path,
                         ImageFormat format, std::int64_t exported_at) {
    return ExportRecord{.id = std::move(id),
                        .region = region,
                        .output_path = std::move(output_path),
                        .format = format,
                        .exported_unix_seconds = exported_at};
}

} // namespace

TEST_CASE("export history: keys use the canonical URI", "[export-history]") {
    REQUIRE(export_history_key("/home/jens/photos/me.png") ==
            "c6ee772d9e49320e97ec29a7eb5b1697");
}

TEST_CASE("export history: JSON round-trips", "[export-history]") {
    ExportHistory history;
    history.source_path = "/home/ctl/photos/me.png";
    history.source_modified_unix_seconds = 1728489000;
    history.records.push_back(
        make_record("id-1", ImageRectangle{.x = 120, .y = 80, .width = 300, .height = 200},
                    "/home/ctl/out/me-001.png", ImageFormat::Png, 1728490000));
    history.records.push_back(
        make_record("id-2", ImageRectangle{.x = 0, .y = 0, .width = 64, .height = 48},
                    "/home/ctl/out/me-002.jpg", ImageFormat::Jpeg, 1728490100));

    const std::string json = export_history_to_json_string(history);
    const auto parsed = export_history_from_json_string(json);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->source_path == history.source_path);
    REQUIRE(parsed->source_modified_unix_seconds == history.source_modified_unix_seconds);
    REQUIRE(parsed->records.size() == 2);

    REQUIRE(parsed->records[0].id == "id-1");
    REQUIRE(parsed->records[0].region == history.records[0].region);
    REQUIRE(parsed->records[0].output_path == "/home/ctl/out/me-001.png");
    REQUIRE(parsed->records[0].format == ImageFormat::Png);
    REQUIRE(parsed->records[0].exported_unix_seconds == 1728490000);

    REQUIRE(parsed->records[1].format == ImageFormat::Jpeg);
    REQUIRE(parsed->records[1].region == history.records[1].region);
}

TEST_CASE("export history: malformed JSON is an error", "[export-history]") {
    REQUIRE_FALSE(export_history_from_json_string("not json at all").has_value());
    REQUIRE_FALSE(export_history_from_json_string("[]").has_value());
    REQUIRE_FALSE(export_history_from_json_string(R"({"version": 2, "exports": []})").has_value());
    REQUIRE_FALSE(export_history_from_json_string(R"({"version": 1})").has_value());
}

TEST_CASE("export history: append merges and counts", "[export-history]") {
    const std::string root = unique_history_root();
    const ExportHistoryStore store(root);
    const std::string source = "/home/ctl/photos/me.png";

    REQUIRE_FALSE(store.contains(source));
    REQUIRE(store.count(source) == 0);
    const auto missing = store.load(source);
    REQUIRE(missing.has_value());
    REQUIRE_FALSE(missing->has_value());

    REQUIRE(store.append(source, 1728489000,
                         make_record("id-1", ImageRectangle{.x = 1, .y = 2, .width = 3,
                                                            .height = 4},
                                     "/home/ctl/out/me-001.png", ImageFormat::Png,
                                     1728490000)));
    REQUIRE(store.contains(source));
    REQUIRE(store.count(source) == 1);

    REQUIRE(store.append(source, 1728489100,
                         make_record("id-2", ImageRectangle{.x = 10, .y = 20, .width = 30,
                                                            .height = 40},
                                     "/home/ctl/out/me-002.png", ImageFormat::Png,
                                     1728490100)));
    REQUIRE(store.count(source) == 2);

    const auto loaded = store.load(source);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->has_value());
    REQUIRE((*loaded)->source_path == source);
    REQUIRE((*loaded)->source_modified_unix_seconds == 1728489100);
    REQUIRE((*loaded)->records.size() == 2);
    REQUIRE((*loaded)->records[1].id == "id-2");
    REQUIRE((*loaded)->records[1].output_path == "/home/ctl/out/me-002.png");

    // The file lives where file_path_for() says.
    REQUIRE(std::filesystem::exists(store.file_path_for(source)));

    std::error_code error;
    std::filesystem::remove_all(root, error);
}
