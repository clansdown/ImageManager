// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <gdkmm/pixbuf.h>
#include <glibmm/dispatcher.h>
#include <glibmm/refptr.h>

#include "io/thumbnail_cache.h"

namespace imagemgr {

/// Generates scaled thumbnails on a small pool of worker threads and delivers
/// them on the main thread.
///
/// request() and cancel_all() must be called from the thread that created the
/// service (the main thread), and so must construction and destruction. The
/// result callback is always invoked on that same thread: immediately for a
/// cache hit, otherwise via an internal Glib::Dispatcher once a worker finishes.
///
/// Results belonging to a superseded generation (see cancel_all()) are dropped.
/// Duplicate in-flight requests for the same (path, size) are coalesced, and
/// completed thumbnails are cached in memory.
///
/// Thread-safety: the worker pool is internal; callers only ever touch the
/// public methods from the main thread.
class ThumbnailService {
public:
    /// Called on the main thread with the requested path, the size it was
    /// requested at, and the generated thumbnail. The thumbnail is null when
    /// decoding failed.
    using ThumbnailCallback =
        std::function<void(const std::string& path, int size, Glib::RefPtr<Gdk::Pixbuf> thumbnail)>;

    /// @param callback Invoked on the main thread for every delivered result.
    ///        Must outlive the service.
    explicit ThumbnailService(ThumbnailCallback callback);

    /// Stops and joins the worker threads. Must run on the main thread.
    ~ThumbnailService();

    ThumbnailService(const ThumbnailService&) = delete;
    ThumbnailService& operator=(const ThumbnailService&) = delete;

    /// Requests a thumbnail for path at the given maximum edge length. A cached
    /// result is delivered synchronously; otherwise the work is queued.
    void request(const std::string& path, int size);

    /// Drops all queued work and ignores any in-flight results generated before
    /// this call. The cache is retained.
    void cancel_all();

private:
    struct Request {
        std::string path;
        int size;
        std::uint64_t generation;
    };

    struct Completed {
        std::string path;
        int size;
        std::uint64_t generation;
        Glib::RefPtr<Gdk::Pixbuf> thumbnail;
    };

    void worker_loop();
    void deliver_completed();
    [[nodiscard]] static std::string cache_key(const std::string& path, int size);

    ThumbnailCallback callback_;
    Glib::Dispatcher dispatcher_;

    // Persistent freedesktop cache, consulted and updated by the workers. All
    // its operations are const, so it needs no locking here.
    ThumbnailCache disk_cache_;

    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<Request> queue_;
    std::deque<Completed> completed_;
    std::unordered_set<std::string> pending_; // cache keys queued or in flight
    std::unordered_map<std::string, Glib::RefPtr<Gdk::Pixbuf>> cache_;
    std::uint64_t generation_ = 0; // only written on the main thread
    bool stopping_ = false;
    std::vector<std::thread> workers_;
};

} // namespace imagemgr
