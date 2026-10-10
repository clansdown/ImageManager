// SPDX-License-Identifier: GPL-3.0-only

#include "io/thumbnail_service.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <giomm/file.h>
#include <glib.h>

#include "io/canonical_path.h"
#include "io/image_loader.h"
#include "io/thumbnail_cache.h"

namespace imagemgr {

namespace {

// Upper bound on decode threads. Image decoding is CPU-heavy, so more workers
// than cores is counterproductive; one core is left for the UI thread.
constexpr unsigned int kMaxWorkerCount = 4;

unsigned int worker_count() {
    const unsigned int hardware = std::max(1U, std::thread::hardware_concurrency());
    const unsigned int leaving_one_for_ui = hardware > 1 ? hardware - 1 : 1;
    return std::clamp(leaving_one_for_ui, 1U, kMaxWorkerCount);
}

// The source file's freshness and type, as the thumbnail standard needs them.
struct SourceMetadata {
    std::int64_t mtime = 0;
    std::uint64_t file_size = 0;
    std::string mimetype;
};

// Scales a thumbnail down so its largest edge is at most size; returns the
// input unchanged when it already fits.
Glib::RefPtr<Gdk::Pixbuf> scale_to_fit(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf, int size) {
    if (!pixbuf) {
        return pixbuf;
    }
    const int largest = std::max(pixbuf->get_width(), pixbuf->get_height());
    if (largest <= size) {
        return pixbuf;
    }
    const double factor = static_cast<double>(size) / static_cast<double>(largest);
    const int target_width = std::max(1, static_cast<int>(pixbuf->get_width() * factor));
    const int target_height = std::max(1, static_cast<int>(pixbuf->get_height() * factor));
    return pixbuf->scale_simple(target_width, target_height, Gdk::InterpType::BILINEAR);
}

// Reads the source file's mtime, size and content type. Returns nullopt when the
// file is unreadable or has no mtime; the standard forbids using or writing a
// cache entry in that case.
std::optional<SourceMetadata> query_source_metadata(const std::string& path) {
    try {
        const auto info = Gio::File::create_for_path(path)->query_info(
            "access::can-read,time::modified,standard::size,standard::content-type",
            Gio::FileQueryInfoFlags::NONE);
        if (!info) {
            return std::nullopt;
        }
        if (info->has_attribute("access::can-read") &&
            !info->get_attribute_boolean("access::can-read")) {
            return std::nullopt;
        }
        SourceMetadata metadata;
        metadata.mtime = static_cast<std::int64_t>(info->get_attribute_uint64("time::modified"));
        metadata.file_size = info->get_attribute_uint64("standard::size");
        if (info->has_attribute("standard::content-type")) {
            metadata.mimetype = info->get_content_type();
        }
        return metadata;
    } catch (const Glib::Error&) {
        return std::nullopt;
    }
}

// Produces a thumbnail for path at the requested size, consulting and updating
// the freedesktop cache. Runs on a worker thread.
Glib::RefPtr<Gdk::Pixbuf> generate_thumbnail(const ThumbnailCache& cache,
                                             const std::string& path, int size) {
    // Never thumbnail the cache itself; decode directly.
    if (cache.contains(path)) {
        const auto decoded = load_image_scaled(path, size);
        return decoded ? *decoded : Glib::RefPtr<Gdk::Pixbuf>{};
    }

    const auto metadata = query_source_metadata(path);
    if (!metadata) {
        // Unreadable or without an mtime: skip the cache but still try to show
        // the image.
        const auto decoded = load_image_scaled(path, size);
        return decoded ? *decoded : Glib::RefPtr<Gdk::Pixbuf>{};
    }

    const std::string uri = canonical_file_uri(path);
    const ThumbnailBucket bucket = bucket_for_thumbnail_size(size);

    if (auto cached = cache.load(bucket, uri, metadata->mtime, metadata->file_size)) {
        return scale_to_fit(*cached, size);
    }

    const auto decoded = load_image_scaled(path, bucket.dimension);
    if (!decoded) {
        return {}; // Decode failure; report no thumbnail.
    }
    if (!cache.store(bucket, uri, metadata->mtime, metadata->file_size, metadata->mimetype,
                     *decoded)) {
        g_debug("could not cache thumbnail for %s", path.c_str());
    }
    return scale_to_fit(*decoded, size);
}

} // namespace

ThumbnailService::ThumbnailService(ThumbnailCallback callback)
    : callback_(std::move(callback)) {
    dispatcher_.connect([this] { deliver_completed(); });

    const unsigned int count = worker_count();
    workers_.reserve(count);
    for (unsigned int index = 0; index < count; ++index) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

ThumbnailService::~ThumbnailService() {
    {
        const std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    condition_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::string ThumbnailService::cache_key(const std::string& path, int size) {
    // A separator that cannot appear in a size keeps path/size keys distinct.
    return path + '\n' + std::to_string(size);
}

void ThumbnailService::request(const std::string& path, int size) {
    const std::string key = cache_key(path, size);
    Glib::RefPtr<Gdk::Pixbuf> cached;

    {
        const std::lock_guard lock(mutex_);
        if (const auto found = cache_.find(key); found != cache_.end()) {
            cached = found->second;
        } else if (pending_.insert(key).second) {
            queue_.push_back(Request{path, size, generation_});
        } else {
            return; // Already queued or being generated.
        }
    }

    if (cached) {
        callback_(path, size, std::move(cached)); // Cache hit: deliver now.
        return;
    }
    condition_.notify_one();
}

void ThumbnailService::cancel_all() {
    const std::lock_guard lock(mutex_);
    ++generation_;
    queue_.clear();
    pending_.clear();
    // completed_ is left for deliver_completed() to drain and discard by
    // generation.
}

void ThumbnailService::invalidate(const std::string& path) {
    const std::lock_guard lock(mutex_);
    // cache_key() is path + '\n' + size, so this prefix matches all sizes.
    const std::string prefix = path + '\n';
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->first.starts_with(prefix)) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (it->starts_with(prefix)) {
            it = pending_.erase(it);
        } else {
            ++it;
        }
    }
}

void ThumbnailService::worker_loop() {
    while (true) {
        Request request;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_) {
                return;
            }
            request = std::move(queue_.front());
            queue_.pop_front();
        }

        Completed completed{
            .path = request.path,
            .size = request.size,
            .generation = request.generation,
            .thumbnail = generate_thumbnail(disk_cache_, request.path, request.size),
        };
        {
            const std::lock_guard lock(mutex_);
            completed_.push_back(std::move(completed));
        }
        dispatcher_.emit();
    }
}

void ThumbnailService::deliver_completed() {
    std::deque<Completed> ready;
    {
        const std::lock_guard lock(mutex_);
        ready.swap(completed_);
    }

    for (auto& completed : ready) {
        const std::string key = cache_key(completed.path, completed.size);
        {
            const std::lock_guard lock(mutex_);
            pending_.erase(key);
        }

        if (completed.generation != generation_) {
            continue; // Superseded by cancel_all().
        }

        if (completed.thumbnail) {
            const std::lock_guard lock(mutex_);
            cache_[key] = completed.thumbnail;
        }
        callback_(completed.path, completed.size, std::move(completed.thumbnail));
    }
}

} // namespace imagemgr
