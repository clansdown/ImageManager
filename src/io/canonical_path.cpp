// SPDX-License-Identifier: GPL-3.0-only

#include "io/canonical_path.h"

#include <giomm/file.h>
#include <glib.h>

namespace imagemgr {

std::string canonical_file_uri(const std::string& file_path) {
    return Gio::File::create_for_path(file_path)->get_uri();
}

std::string md5_hex(std::string_view text) {
    gchar* checksum = g_compute_checksum_for_string(G_CHECKSUM_MD5, text.data(),
                                                    static_cast<gssize>(text.size()));
    std::string result = checksum != nullptr ? checksum : std::string{};
    g_free(checksum);
    return result;
}

} // namespace imagemgr
