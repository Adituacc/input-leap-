/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace inputleap {

class TransferResumeCoordinator {
public:
    void prepare(const std::string& transfer_id, std::size_t entry_count);
    bool accept(const std::string& transfer_id,
                const std::vector<std::uint64_t>& offsets);
    void cancel(const std::string& transfer_id);
    std::vector<std::uint64_t> wait(
        std::chrono::milliseconds timeout = std::chrono::seconds(5));

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::string transfer_id_;
    std::size_t entry_count_ = 0;
    std::vector<std::uint64_t> offsets_;
    bool ready_ = false;
    bool cancelled_ = false;
};

} // namespace inputleap
