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

double TransferProgressSnapshot::fraction() const
{
    if (total_bytes == 0) {
        return state == TransferState::Completed ? 1.0 : 0.0;
    }
    return std::min(1.0, static_cast<double>(transferred_bytes) /
                            static_cast<double>(total_bytes));
}

void TransferProgress::begin(const TransferManifest& manifest)
{
    std::lock_guard<std::mutex> lock(mutex_);
    manifest_ = manifest;
    snapshot_ = {};
    snapshot_.transfer_id = manifest.transfer_id();
    snapshot_.entry_count = manifest.entries().size();
    snapshot_.total_bytes = manifest.total_size();
    snapshot_.state = TransferState::Running;
    started_ = Clock::now();
}

void TransferProgress::set_current_entry(std::size_t index)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (index < manifest_.entries().size()) {
        snapshot_.current_path = manifest_.entries()[index].relative_path;
    }
}

void TransferProgress::add_bytes(std::uint64_t count)
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
}

void TransferProgress::finish_entry()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.completed_entries < snapshot_.entry_count) {
        ++snapshot_.completed_entries;
    }
}

void TransferProgress::pause()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.state == TransferState::Running) {
        snapshot_.state = TransferState::Paused;
    }
}

void TransferProgress::resume()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.state == TransferState::Paused) {
        snapshot_.state = TransferState::Running;
    }
}

void TransferProgress::cancel()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.state != TransferState::Completed) {
        snapshot_.state = TransferState::Cancelled;
    }
}

void TransferProgress::complete()
{
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.state = TransferState::Completed;
    snapshot_.transferred_bytes = snapshot_.total_bytes;
    snapshot_.completed_entries = snapshot_.entry_count;
    snapshot_.current_path.clear();
}

void TransferProgress::fail(std::string error)
{
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.state = TransferState::Failed;
    snapshot_.error = std::move(error);
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

} // namespace inputleap
