// SPDX-License-Identifier: GPL-3.0-only

#include "io/export_history_store.h"

#include <utility>

#include <glib.h>
#include <glibmm/miscutils.h>
#include <json-glib/json-glib.h>

#include "io/canonical_path.h"

namespace imagemgr {

namespace {

// Schema version written into and required by every history file.
constexpr int kSchemaVersion = 1;

Error make_error(std::string message) {
    return Error{.message = std::move(message)};
}

// --- Building -----------------------------------------------------------------

void add_region(JsonBuilder* builder, ImageRectangle region) {
    json_builder_set_member_name(builder, "region");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "x");
    json_builder_add_int_value(builder, region.x);
    json_builder_set_member_name(builder, "y");
    json_builder_add_int_value(builder, region.y);
    json_builder_set_member_name(builder, "width");
    json_builder_add_int_value(builder, region.width);
    json_builder_set_member_name(builder, "height");
    json_builder_add_int_value(builder, region.height);
    json_builder_end_object(builder);
}

void add_record(JsonBuilder* builder, const ExportRecord& record) {
    const std::string format(extension_for_format(record.format));

    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "id");
    json_builder_add_string_value(builder, record.id.c_str());
    add_region(builder, record.region);
    json_builder_set_member_name(builder, "output_path");
    json_builder_add_string_value(builder, record.output_path.c_str());
    json_builder_set_member_name(builder, "format");
    json_builder_add_string_value(builder, format.c_str());
    json_builder_set_member_name(builder, "exported_at");
    json_builder_add_int_value(builder, record.exported_unix_seconds);
    json_builder_end_object(builder);
}

// --- Parsing helpers ----------------------------------------------------------

std::optional<std::string> member_string(JsonObject* object, const char* name) {
    if (!json_object_has_member(object, name)) {
        return std::nullopt;
    }
    JsonNode* node = json_object_get_member(object, name);
    if (node == nullptr || !JSON_NODE_HOLDS_VALUE(node) ||
        json_node_get_value_type(node) != G_TYPE_STRING) {
        return std::nullopt;
    }
    return std::string(json_node_get_string(node));
}

std::optional<gint64> member_int(JsonObject* object, const char* name) {
    if (!json_object_has_member(object, name)) {
        return std::nullopt;
    }
    JsonNode* node = json_object_get_member(object, name);
    if (node == nullptr || !JSON_NODE_HOLDS_VALUE(node) ||
        json_node_get_value_type(node) != G_TYPE_INT64) {
        return std::nullopt;
    }
    return json_node_get_int(node);
}

JsonObject* member_object(JsonObject* object, const char* name) {
    if (!json_object_has_member(object, name)) {
        return nullptr;
    }
    JsonNode* node = json_object_get_member(object, name);
    if (node == nullptr || !JSON_NODE_HOLDS_OBJECT(node)) {
        return nullptr;
    }
    return json_node_get_object(node);
}

JsonArray* member_array(JsonObject* object, const char* name) {
    if (!json_object_has_member(object, name)) {
        return nullptr;
    }
    JsonNode* node = json_object_get_member(object, name);
    if (node == nullptr || !JSON_NODE_HOLDS_ARRAY(node)) {
        return nullptr;
    }
    return json_node_get_array(node);
}

std::expected<ImageRectangle, Error> parse_region(JsonObject* object) {
    const auto x = member_int(object, "x");
    const auto y = member_int(object, "y");
    const auto width = member_int(object, "width");
    const auto height = member_int(object, "height");
    if (!x || !y || !width || !height) {
        return std::unexpected(make_error("export record region is missing or malformed"));
    }
    return ImageRectangle{.x = static_cast<int>(*x),
                          .y = static_cast<int>(*y),
                          .width = static_cast<int>(*width),
                          .height = static_cast<int>(*height)};
}

std::expected<ExportRecord, Error> parse_record(JsonObject* object) {
    ExportRecord record;

    if (const auto id = member_string(object, "id")) {
        record.id = *id;
    }
    if (const auto output_path = member_string(object, "output_path")) {
        record.output_path = *output_path;
    } else {
        return std::unexpected(make_error("export record is missing output_path"));
    }

    JsonObject* region = member_object(object, "region");
    if (region == nullptr) {
        return std::unexpected(make_error("export record is missing its region"));
    }
    const auto parsed_region = parse_region(region);
    if (!parsed_region) {
        return std::unexpected(parsed_region.error());
    }
    record.region = *parsed_region;

    if (const auto format = member_string(object, "format")) {
        record.format = format_from_extension(*format).value_or(ImageFormat::Png);
    }
    if (const auto exported_at = member_int(object, "exported_at")) {
        record.exported_unix_seconds = *exported_at;
    }
    return record;
}

} // namespace

std::string default_export_history_root() {
    return Glib::build_filename(g_get_user_state_dir(), "imagemgr", "exports");
}

std::string export_history_key(const std::string& source_path) {
    return md5_hex(canonical_file_uri(source_path));
}

std::string export_history_to_json_string(const ExportHistory& history) {
    g_autoptr(JsonBuilder) builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "version");
    json_builder_add_int_value(builder, kSchemaVersion);
    json_builder_set_member_name(builder, "source_path");
    json_builder_add_string_value(builder, history.source_path.c_str());
    json_builder_set_member_name(builder, "source_mtime");
    json_builder_add_int_value(builder, history.source_modified_unix_seconds);
    json_builder_set_member_name(builder, "exports");
    json_builder_begin_array(builder);
    for (const auto& record : history.records) {
        add_record(builder, record);
    }
    json_builder_end_array(builder);
    json_builder_end_object(builder);

    g_autoptr(JsonGenerator) generator = json_generator_new();
    // set_root takes its own reference, so the builder's root is released here.
    g_autoptr(JsonNode) root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    json_generator_set_pretty(generator, TRUE);
    g_autofree gchar* data = json_generator_to_data(generator, nullptr);
    return data != nullptr ? std::string(data) : std::string{};
}

std::expected<ExportHistory, Error> export_history_from_json_string(std::string_view json) {
    g_autoptr(JsonParser) parser = json_parser_new();
    GError* error = nullptr;
    if (!json_parser_load_from_data(parser, json.data(), static_cast<gssize>(json.size()),
                                    &error)) {
        std::string message = error != nullptr ? error->message : std::string("invalid JSON");
        g_clear_error(&error);
        return std::unexpected(make_error(std::move(message)));
    }

    JsonNode* root = json_parser_get_root(parser);
    if (root == nullptr || !JSON_NODE_HOLDS_OBJECT(root)) {
        return std::unexpected(make_error("export history root is not a JSON object"));
    }
    JsonObject* object = json_node_get_object(root);

    const auto version = member_int(object, "version");
    if (!version || *version != kSchemaVersion) {
        return std::unexpected(make_error("unsupported export history version"));
    }

    ExportHistory history;
    if (const auto source_path = member_string(object, "source_path")) {
        history.source_path = *source_path;
    }
    if (const auto source_mtime = member_int(object, "source_mtime")) {
        history.source_modified_unix_seconds = *source_mtime;
    }

    JsonArray* exports = member_array(object, "exports");
    if (exports == nullptr) {
        return std::unexpected(make_error("export history is missing the exports array"));
    }
    const guint length = json_array_get_length(exports);
    history.records.reserve(length);
    for (guint index = 0; index < length; ++index) {
        JsonObject* record_object = json_array_get_object_element(exports, index);
        if (record_object == nullptr) {
            return std::unexpected(make_error("export record is not a JSON object"));
        }
        auto record = parse_record(record_object);
        if (!record) {
            return std::unexpected(record.error());
        }
        history.records.push_back(std::move(*record));
    }
    return history;
}

ExportHistoryStore::ExportHistoryStore(std::string root) : root_(std::move(root)) {}

std::string ExportHistoryStore::file_path_for(const std::string& source_path) const {
    return Glib::build_filename(root_, export_history_key(source_path) + ".json");
}

bool ExportHistoryStore::contains(const std::string& source_path) const {
    return g_file_test(file_path_for(source_path).c_str(), G_FILE_TEST_EXISTS);
}

std::expected<std::optional<ExportHistory>, Error> ExportHistoryStore::load(
    const std::string& source_path) const {
    const std::string path = file_path_for(source_path);
    if (!g_file_test(path.c_str(), G_FILE_TEST_EXISTS)) {
        return std::optional<ExportHistory>{};
    }

    g_autofree gchar* contents = nullptr;
    gsize length = 0;
    GError* error = nullptr;
    if (!g_file_get_contents(path.c_str(), &contents, &length, &error)) {
        std::string message =
            error != nullptr ? error->message : std::string("could not read file");
        g_clear_error(&error);
        return std::unexpected(make_error(std::move(message)));
    }

    auto parsed = export_history_from_json_string(std::string_view(contents, length));
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    return std::optional<ExportHistory>{std::move(*parsed)};
}

std::size_t ExportHistoryStore::count(const std::string& source_path) const {
    const auto loaded = load(source_path);
    if (!loaded || !*loaded) {
        return 0;
    }
    return (*loaded)->records.size();
}

std::expected<void, Error> ExportHistoryStore::append(
    const std::string& source_path, std::int64_t source_modified_unix_seconds,
    const ExportRecord& record) const {
    ExportHistory history;
    history.source_path = source_path;

    const auto existing = load(source_path);
    if (!existing) {
        return std::unexpected(existing.error());
    }
    if (*existing) {
        history = std::move(**existing);
        if (history.source_path.empty()) {
            history.source_path = source_path;
        }
    }
    history.source_modified_unix_seconds = source_modified_unix_seconds;
    history.records.push_back(record);

    return save(history);
}

std::expected<void, Error> ExportHistoryStore::save(const ExportHistory& history) const {
    if (history.source_path.empty()) {
        return std::unexpected(make_error("export history has no source path"));
    }

    const std::string json = export_history_to_json_string(history);

    if (g_mkdir_with_parents(root_.c_str(), 0700) != 0) {
        return std::unexpected(make_error("could not create the export history directory"));
    }

    GError* error = nullptr;
    const gboolean written = g_file_set_contents_full(
        file_path_for(history.source_path).c_str(), json.data(),
        static_cast<gssize>(json.size()), G_FILE_SET_CONTENTS_CONSISTENT, 0600, &error);
    if (!written) {
        std::string message =
            error != nullptr ? error->message : std::string("could not write file");
        g_clear_error(&error);
        return std::unexpected(make_error(std::move(message)));
    }
    return {};
}

} // namespace imagemgr
