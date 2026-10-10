// SPDX-License-Identifier: GPL-3.0-only

#include "io/image_load_service.h"

#include <utility>

#include "io/image_loader.h"

namespace imagemgr {

ImageLoadService::ImageLoadService(LoadCallback callback)
    : callback_(std::move(callback)) {
    dispatcher_.connect([this] { deliver_completed(); });
    worker_ = std::thread([this] { worker_loop(); });
}

ImageLoadService::~ImageLoadService() {
    {
        const std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    condition_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void ImageLoadService::load(const std::string& file_path) {
    {
        const std::lock_guard lock(mutex_);
        ++generation_;
        pending_ = Request{file_path, generation_};
    }
    condition_.notify_one();
}

void ImageLoadService::cancel() {
    const std::lock_guard lock(mutex_);
    ++generation_;
    pending_.reset();
}

void ImageLoadService::worker_loop() {
    while (true) {
        Request request;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [this] { return stopping_ || pending_.has_value(); });
            if (stopping_) {
                return;
            }
            request = std::move(*pending_);
            pending_.reset();
        }

        auto result = load_image(request.path);

        bool still_current = false;
        {
            const std::lock_guard lock(mutex_);
            if (request.generation == generation_) {
                completed_ = Completed{request.path, request.generation, std::move(result)};
                still_current = true;
            }
        }
        if (still_current) {
            dispatcher_.emit();
        }
    }
}

void ImageLoadService::deliver_completed() {
    std::optional<Completed> completed;
    {
        const std::lock_guard lock(mutex_);
        completed.swap(completed_);
    }
    if (!completed) {
        return;
    }
    callback_(completed->path, std::move(completed->result));
}

} // namespace imagemgr
