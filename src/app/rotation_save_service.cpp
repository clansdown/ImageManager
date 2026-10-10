// SPDX-License-Identifier: GPL-3.0-only

#include "app/rotation_save_service.h"

#include <utility>

#include "io/image_rotation.h"

namespace imagemgr {

RotationSaveService::RotationSaveService(SaveCallback callback)
    : callback_(std::move(callback)) {
    dispatcher_.connect([this] { deliver_completed(); });
    worker_ = std::thread([this] { worker_loop(); });
}

RotationSaveService::~RotationSaveService() {
    {
        const std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    condition_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void RotationSaveService::save(const std::string& path, QuarterTurns turns,
                               Glib::RefPtr<Gdk::Pixbuf> rotated_pixbuf) {
    {
        const std::lock_guard lock(mutex_);
        ++generation_;
        pending_ = Request{path, turns, std::move(rotated_pixbuf), generation_};
    }
    condition_.notify_one();
}

void RotationSaveService::worker_loop() {
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

        Completed completed{
            .path = request.path,
            .turns = request.turns,
            .generation = request.generation,
            .result = write_rotated_image(request.path, request.turns, request.rotated_pixbuf),
        };
        {
            const std::lock_guard lock(mutex_);
            completed_ = std::move(completed);
        }
        dispatcher_.emit();
    }
}

void RotationSaveService::deliver_completed() {
    std::optional<Completed> ready;
    {
        const std::lock_guard lock(mutex_);
        ready = std::move(completed_);
        completed_.reset();
    }
    if (!ready || ready->generation != generation_) {
        return; // Superseded by a newer save().
    }
    callback_(ready->path, ready->turns, std::move(ready->result));
}

} // namespace imagemgr
