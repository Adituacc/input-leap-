/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/TransferResumeCoordinator.h"

#include <stdexcept>

namespace inputleap {

void TransferResumeCoordinator::prepare(const std::string& transfer_id,
                                        std::size_t entry_count)
{
    std::lock_guard<std::mutex> lock(mutex_);
    transfer_id_ = transfer_id;
    entry_count_ = entry_count;
    offsets_.clear();
    ready_ = false;
    cancelled_ = false;
}

bool TransferResumeCoordinator::accept(
    const std::string& transfer_id,
    const std::vector<std::uint64_t>& offsets)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (transfer_id != transfer_id_ || offsets.size() != entry_count_) {
        return false;
    }
    offsets_ = offsets;
    ready_ = true;
    condition_.notify_all();
    return true;
}

void TransferResumeCoordinator::cancel(const std::string& transfer_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (transfer_id == transfer_id_) {
        cancelled_ = true;
        condition_.notify_all();
    }
}

std::vector<std::uint64_t> TransferResumeCoordinator::wait(
    std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (!condition_.wait_for(lock, timeout,
                             [this]() { return ready_ || cancelled_; })) {
        throw std::runtime_error("timed out waiting for transfer resume state");
    }
    if (cancelled_) {
        throw std::runtime_error("transfer was cancelled");
    }
    auto offsets = offsets_;
    offsets_.clear();
    ready_ = false;
    return offsets;
}

} // namespace inputleap
