// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/export.h"
#include "core/geometry.h"

namespace imagemgr {

/// One completed export of a region of a source image.
///
/// The region is in source-image pixel coordinates. Rotation is written out to
/// the source file, so the document's pixel space at export time is the file's
/// and no rotation needs recording here.
struct ExportRecord {
    std::string id;                  // stable identity for future re-export
    ImageRectangle region;           // source-image pixels
    std::string output_path;         // absolute path written
    ImageFormat format = ImageFormat::Png;
    std::int64_t exported_unix_seconds = 0;
};

/// All exports recorded for one source image.
struct ExportHistory {
    std::string source_path;
    std::int64_t source_modified_unix_seconds = 0;
    std::vector<ExportRecord> records;
};

} // namespace imagemgr
