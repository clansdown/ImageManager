// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cctype>
#include <optional>
#include <string>
#include <string_view>

#include "core/geometry.h"

namespace imagemgr {

/// Image formats the application can write.
enum class ImageFormat {
    Png,
    Jpeg,
    Webp,
    Avif,
};

/// The file extension (without the dot) used for a format.
[[nodiscard]] constexpr std::string_view extension_for_format(ImageFormat format) {
    switch (format) {
    case ImageFormat::Png:
        return "png";
    case ImageFormat::Jpeg:
        return "jpg";
    case ImageFormat::Webp:
        return "webp";
    case ImageFormat::Avif:
        return "avif";
    }
    return "png";
}

/// Maps a file extension to a writable format.
///
/// The extension may include a leading dot and is matched case-insensitively.
/// Both "jpg" and "jpeg" map to ImageFormat::Jpeg. Returns nullopt for anything
/// else.
[[nodiscard]] inline std::optional<ImageFormat> format_from_extension(
    std::string_view extension) {
    if (!extension.empty() && extension.front() == '.') {
        extension.remove_prefix(1);
    }
    std::string lowered;
    lowered.reserve(extension.size());
    for (const char character : extension) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }

    if (lowered == "png") {
        return ImageFormat::Png;
    }
    if (lowered == "jpg" || lowered == "jpeg") {
        return ImageFormat::Jpeg;
    }
    if (lowered == "webp") {
        return ImageFormat::Webp;
    }
    if (lowered == "avif") {
        return ImageFormat::Avif;
    }
    return std::nullopt;
}

/// Everything needed to write one cropped region to a file.
///
/// The crop region is expressed in the display (already-rotated) image's pixel
/// coordinates; the caller supplies the rotated pixbuf, so no rotation state is
/// carried here.
struct ExportRequest {
    /// Directory the new file is written into. Must not be empty.
    std::string target_directory;

    /// Region of the display image to write, clamped to the image bounds.
    ImageRectangle crop_region;

    /// Output format.
    ImageFormat format = ImageFormat::Png;

    /// Encoder quality (0-100) for lossy formats; ignored by PNG.
    int quality = 90;
};

} // namespace imagemgr
