/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "inputleap/TransferManifest.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace inputleap {

enum class TransferState {
    Pending,
    Running,
    Paused,
    Completed,
    Cancelled,
    Failed
};

struct TransferProgressSnapshot {
    std::string transfer_id;
    std::string current_path;
    TransferState state = TransferState::Pending;
    std::size_t entry_count = 0;
    std::size_t completed_entries = 0;
    std::uint64_t total_bytes = 0;
    std::uint64_t transferred_bytes = 0;
    double bytes_per_second = 0.0;
    std::string error;

    double fraction() const;
};

class TransferProgress {
public:
    using Observer = std::function<void(const TransferProgressSnapshot&)>;

    void set_observer(Observer observer);
    void begin(const TransferManifest& manifest);
    void set_current_entry(std::size_t index);
    void add_bytes(std::uint64_t count);
    void finish_entry();
    void pause();
    void resume();
    void cancel();
    void complete();
    void fail(std::string error);

    bool should_cancel() const;
    TransferProgressSnapshot snapshot() const;

private:
    using Clock = std::chrono::steady_clock;

    void notify(const TransferProgressSnapshot& snapshot,
                const Observer& observer) const;
    bool prepare_notification_locked(bool force,
                                     TransferProgressSnapshot& snapshot,
                                     Observer& observer);

    mutable std::mutex mutex_;
    TransferManifest manifest_;
    TransferProgressSnapshot snapshot_;
    Clock::time_point started_;
    Clock::time_point last_notification_;
    Observer observer_;
};

} // namespace inputleap
