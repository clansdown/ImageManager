// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string_view>

namespace imagemgr {

/// Human-readable application name, used for user-facing strings such as the
/// PNG "Software" chunk written into cached thumbnails.
inline constexpr std::string_view kApplicationName = "ImageManager";

/// Application version. Written into cached thumbnails' "Software" chunk; keep
/// it in step with the version in CMakeLists.txt's project().
inline constexpr std::string_view kApplicationVersion = "0.1";

} // namespace imagemgr
