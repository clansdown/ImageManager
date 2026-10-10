// SPDX-License-Identifier: GPL-3.0-only

#include "io/exif_orientation.h"

#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>

#include <giomm/file.h>
#include <glib.h>
#include <jpeglib.h>
#include <libexif/exif-data.h>
#include <libexif/exif-entry.h>
#include <libexif/exif-ifd.h>
#include <libexif/exif-tag.h>
#include <libexif/exif-utils.h>

namespace imagemgr {

namespace {

// libjpeg's default error handler calls exit(); this one longjmps back so the
// operation can fail gracefully and clean up.
struct JpegErrorManager {
    jpeg_error_mgr base;
    std::jmp_buf jump;
    char message[JMSG_LENGTH_MAX] = {};
};

void on_jpeg_error(j_common_ptr info) {
    auto* manager = reinterpret_cast<JpegErrorManager*>(info->err);
    (*info->err->format_message)(info, manager->message);
    longjmp(manager->jump, 1);
}

// True when an APP1 marker payload is an Exif block.
bool is_exif_app1(const jpeg_saved_marker_ptr marker) {
    return marker->marker == JPEG_APP0 + 1 && marker->data_length >= 6 &&
           std::memcmp(marker->data, "Exif\0\0", 6) == 0;
}

// True when an APP0 marker payload is JFIF; jpeg_write_coefficients writes it
// automatically when write_JFIF_header is set.
bool is_jfif_app0(const jpeg_saved_marker_ptr marker) {
    return marker->marker == JPEG_APP0 && marker->data_length >= 5 &&
           std::memcmp(marker->data, "JFIF\0", 5) == 0;
}

// True when an APP14 marker payload is Adobe; jpeg_write_coefficients writes it
// automatically when write_Adobe_marker is set.
bool is_adobe_app14(const jpeg_saved_marker_ptr marker) {
    return marker->marker == JPEG_APP0 + 14 && marker->data_length >= 5 &&
           std::memcmp(marker->data, "Adobe", 5) == 0;
}

// Builds the Exif APP1 payload (including the "Exif\0\0" header) carrying the
// given orientation, reusing the file's existing Exif block when present.
std::expected<std::string, Error> build_exif_block(const std::string& path,
                                                   ExifOrientation orientation) {
    ExifData* exif = exif_data_new_from_file(path.c_str());
    if (exif == nullptr) {
        exif = exif_data_new();
        if (exif == nullptr) {
            return std::unexpected(Error{"could not allocate EXIF data"});
        }
        exif_data_set_option(exif, EXIF_DATA_OPTION_FOLLOW_SPECIFICATION);
        exif_data_set_data_type(exif, EXIF_DATA_TYPE_COMPRESSED);
    }

    ExifEntry* entry = exif_data_get_entry(exif, EXIF_TAG_ORIENTATION);
    if (entry == nullptr) {
        entry = exif_entry_new();
        exif_content_add_entry(exif->ifd[EXIF_IFD_0], entry);
        exif_entry_initialize(entry, EXIF_TAG_ORIENTATION);
        exif_entry_unref(entry);
        entry = exif_data_get_entry(exif, EXIF_TAG_ORIENTATION);
    }
    if (entry == nullptr || entry->data == nullptr) {
        exif_data_unref(exif);
        return std::unexpected(Error{"could not prepare the EXIF orientation entry"});
    }
    exif_set_short(entry->data, exif_data_get_byte_order(exif),
                   static_cast<ExifShort>(orientation));
    entry->size = 2;

    unsigned char* buffer = nullptr;
    unsigned int size = 0;
    exif_data_save_data(exif, &buffer, &size);
    exif_data_unref(exif);
    if (buffer == nullptr || size == 0) {
        std::free(buffer);
        return std::unexpected(Error{"could not serialise the EXIF data"});
    }
    std::string result(reinterpret_cast<const char*>(buffer), size);
    std::free(buffer);
    return result;
}

// Reads the file, copies its DCT coefficients and metadata markers verbatim,
// replaces the Exif APP1 marker, and returns the new JPEG bytes.
std::expected<std::string, Error> rewrite_jpeg_with_exif(const std::string& path,
                                                         const std::string& exif_block) {
    jpeg_decompress_struct source{};
    jpeg_compress_struct destination{};
    JpegErrorManager source_error{};
    JpegErrorManager destination_error{};
    bool source_created = false;
    bool destination_created = false;
    unsigned char* output = nullptr;
    unsigned long output_size = 0;
    FILE* input = nullptr;
    jvirt_barray_ptr* coefficients = nullptr;
    std::string failure;

    source.err = jpeg_std_error(&source_error.base);
    source_error.base.error_exit = on_jpeg_error;
    destination.err = jpeg_std_error(&destination_error.base);
    destination_error.base.error_exit = on_jpeg_error;

    if (setjmp(source_error.jump) != 0) {
        failure = source_error.message;
        goto cleanup;
    }
    jpeg_create_decompress(&source);
    source_created = true;

    input = std::fopen(path.c_str(), "rb");
    if (input == nullptr) {
        failure = "could not open the file for reading";
        goto cleanup;
    }
    jpeg_stdio_src(&source, input);
    for (int marker = 0; marker <= 15; ++marker) {
        jpeg_save_markers(&source, JPEG_APP0 + marker, 0xFFFF);
    }
    jpeg_save_markers(&source, JPEG_COM, 0xFFFF);
    jpeg_read_header(&source, TRUE);
    coefficients = jpeg_read_coefficients(&source);

    if (setjmp(destination_error.jump) != 0) {
        failure = destination_error.message;
        goto cleanup;
    }
    jpeg_create_compress(&destination);
    destination_created = true;
    jpeg_mem_dest(&destination, &output, &output_size);
    jpeg_copy_critical_parameters(&source, &destination);
    jpeg_write_coefficients(&destination, coefficients);

    for (jpeg_saved_marker_ptr marker = source.marker_list; marker != nullptr;
         marker = marker->next) {
        if (is_exif_app1(marker)) {
            continue; // Replaced below.
        }
        if (destination.write_JFIF_header && is_jfif_app0(marker)) {
            continue; // Written by jpeg_write_coefficients.
        }
        if (destination.write_Adobe_marker && is_adobe_app14(marker)) {
            continue; // Written by jpeg_write_coefficients.
        }
        jpeg_write_marker(&destination, marker->marker, marker->data, marker->data_length);
    }
    jpeg_write_marker(&destination, JPEG_APP0 + 1,
                      reinterpret_cast<const JOCTET*>(exif_block.data()),
                      static_cast<unsigned int>(exif_block.size()));
    jpeg_finish_compress(&destination);

cleanup:
    if (destination_created) {
        jpeg_destroy_compress(&destination);
    }
    if (source_created) {
        jpeg_destroy_decompress(&source);
    }
    if (input != nullptr) {
        std::fclose(input);
    }

    if (!failure.empty()) {
        std::free(output);
        return std::unexpected(Error{std::format("could not rewrite {}: {}", path, failure)});
    }
    std::string result(reinterpret_cast<const char*>(output), output_size);
    std::free(output);
    return result;
}

} // namespace

std::optional<ExifOrientation> read_jpeg_orientation(const std::string& file_path) {
    ExifData* exif = exif_data_new_from_file(file_path.c_str());
    if (exif == nullptr) {
        return std::nullopt;
    }

    std::optional<ExifOrientation> result;
    if (const ExifEntry* entry = exif_data_get_entry(exif, EXIF_TAG_ORIENTATION)) {
        const int value = exif_get_short(entry->data, exif_data_get_byte_order(exif));
        if (value >= static_cast<int>(ExifOrientation::Normal) &&
            value <= static_cast<int>(ExifOrientation::Rotate270)) {
            result = static_cast<ExifOrientation>(value);
        }
    }
    exif_data_unref(exif);
    return result;
}

std::expected<void, Error> write_jpeg_orientation(const std::string& file_path,
                                                  ExifOrientation orientation) {
    const auto exif_block = build_exif_block(file_path, orientation);
    if (!exif_block) {
        return std::unexpected(exif_block.error());
    }
    auto rewritten = rewrite_jpeg_with_exif(file_path, *exif_block);
    if (!rewritten) {
        return std::unexpected(rewritten.error());
    }

    try {
        const auto file = Gio::File::create_for_path(file_path);
        std::string etag;
        file->replace_contents(*rewritten, std::string{}, etag);
    } catch (const Glib::Error& error) {
        return std::unexpected(
            Error{std::format("could not write {}: {}", file_path, error.what())});
    }
    return {};
}

} // namespace imagemgr
