// SPDX-License-Identifier: GPL-3.0-only

#include "app/directory_item.h"

#include <utility>

#include <glibmm/ustring.h>

namespace imagemgr {

DirectoryItem::DirectoryItem(std::string path, std::string display_name,
                             std::int64_t modified_unix_seconds)
    : path_(std::move(path)),
      display_name_(std::move(display_name)),
      modified_unix_seconds_(modified_unix_seconds),
      name_sort_key_(Glib::ustring(display_name_).casefold_collate_key()) {}

Glib::RefPtr<DirectoryItem> DirectoryItem::create(std::string path, std::string display_name,
                                                  std::int64_t modified_unix_seconds) {
    return Glib::make_refptr_for_instance<DirectoryItem>(
        new DirectoryItem(std::move(path), std::move(display_name), modified_unix_seconds));
}

void DirectoryItem::set_thumbnail(Glib::RefPtr<Gdk::Pixbuf> thumbnail) {
    thumbnail_ = std::move(thumbnail);
    thumbnail_changed_signal_.emit();
}

sigc::signal<void()>& DirectoryItem::signal_thumbnail_changed() {
    return thumbnail_changed_signal_;
}

} // namespace imagemgr
