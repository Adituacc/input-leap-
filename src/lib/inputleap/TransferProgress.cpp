/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/TransferProgress.h"

#include <algorithm>
#include <utility>

namespace inputleap {

namespace {

constexpr auto kProgressNotificationInterval = std::chrono::milliseconds(200);

}

double TransferProgressSnapshot::fraction() const
{
    if (total_bytes == 0) {
        return state == TransferState::Completed ? 1.0 : 0.0;
    }
    return std::min(1.0, static_cast<double>(transferred_bytes) /
                            static_cast<double>(total_bytes));
}

void TransferProgress::set_observer(Observer observer)
{
    TransferProgressSnapshot snapshot;
    Observer active_observer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        observer_ = std::move(observer);
        prepare_notification_locked(true, snapshot, active_observer);
    }
    notify(snapshot, active_observer);
}

void TransferProgress::begin(const TransferManifest& manifest)
{
    TransferProgressSnapshot snapshot;
    Observer observer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        manifest_ = manifest;
        snapshot_ = {};
        snapshot_.transfer_id = manifest.transfer_id();
        snapshot_.entry_count = manifest.entries().size();
        snapshot_.total_bytes = manifest.total_size();
        snapshot_.state = TransferState::Running;
        started_ = Clock::now();
        last_notification_ = {};
        prepare_notification_locked(true, snapshot, observer);
    }
    notify(snapshot, observer);
}

void TransferProgress::set_current_entry(std::size_t index)
{
    TransferProgressSnapshot snapshot;
    Observer observer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index < manifest_.entries().size()) {
            snapshot_.current_path = manifest_.entries()[index].relative_path;
        }
        prepare_notification_locked(false, snapshot, observer);
    }
    notify(snapshot, observer);
}

void TransferProgress::add_bytes(std::uint64_t count)
{
    TransferProgressSnapshot snapshot;
    Observer observer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto remaining =
            snapshot_.transferred_bytes < snapshot_.total_bytes
                ? snapshot_.total_bytes - snapshot_.transferred_bytes
                : 0;
        snapshot_.transferred_bytes += std::min(count, remaining);
        const auto seconds =
            std::chrono::duration<double>(Clock::now() - started_).count();
        if (seconds > 0.0) {
            snapshot_.bytes_per_second =
                static_cast<double>(snapshot_.transferred_bytes) / seconds;
        }
        prepare_notification_locked(false, snapshot, observer);
    }
    notify(snapshot, observer);
}

void TransferProgress::finish_entry()
{
    TransferProgressSnapshot snapshot;
    Observer observer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (snapshot_.completed_entries < snapshot_.entry_count) {
            ++snapshot_.completed_entries;
        }
        prepare_notification_locked(true, snapshot, observer);
    }
    notify(snapshot, observer);
}

void TransferProgress::pause()
{
    TransferProgressSnapshot snapshot;
    Observer observer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (snapshot_.state == TransferState::Running) {
            snapshot_.state = TransferState::Paused;
        }
        prepare_notification_locked(true, snapshot, observer);
    }
    notify(snapshot, observer);
}

void TransferProgress::resume()
{
    TransferProgressSnapshot snapshot;
    Observer observer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (snapshot_.state == TransferState::Paused) {
            snapshot_.state = TransferState::Running;
        }
        prepare_notification_locked(true, snapshot, observer);
    }
    notify(snapshot, observer);
}

void TransferProgress::cancel()
{
    TransferProgressSnapshot snapshot;
    Observer observer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (snapshot_.state != TransferState::Completed) {
            snapshot_.state = TransferState::Cancelled;
        }
        prepare_notification_locked(true, snapshot, observer);
    }
    notify(snapshot, observer);
}

void TransferProgress::complete()
{
    TransferProgressSnapshot snapshot;
    Observer observer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.state = TransferState::Completed;
        snapshot_.transferred_bytes = snapshot_.total_bytes;
        snapshot_.completed_entries = snapshot_.entry_count;
        snapshot_.current_path.clear();
        prepare_notification_locked(true, snapshot, observer);
    }
    notify(snapshot, observer);
}

void TransferProgress::fail(std::string error)
{
    TransferProgressSnapshot snapshot;
    Observer observer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.state = TransferState::Failed;
        snapshot_.error = std::move(error);
        prepare_notification_locked(true, snapshot, observer);
    }
    notify(snapshot, observer);
}

bool TransferProgress::should_cancel() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_.state == TransferState::Cancelled ||
           snapshot_.state == TransferState::Failed;
}

TransferProgressSnapshot TransferProgress::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

void TransferProgress::notify(const TransferProgressSnapshot& snapshot,
                              const Observer& observer) const
{
    if (observer && !snapshot.transfer_id.empty()) {
        observer(snapshot);
    }
}

bool TransferProgress::prepare_notification_locked(
    bool force, TransferProgressSnapshot& snapshot, Observer& observer)
{
    if (!observer_ || snapshot_.transfer_id.empty()) {
        return false;
    }
    const auto now = Clock::now();
    if (!force && last_notification_ != Clock::time_point{} &&
        now - last_notification_ < kProgressNotificationInterval) {
        return false;
    }
    last_notification_ = now;
    snapshot = snapshot_;
    observer = observer_;
    return true;
}

} // namespace inputleap
