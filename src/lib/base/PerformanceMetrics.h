/*
    InputLeap -- mouse and keyboard sharing utility
    Copyright (C) InputLeap contributors

    This package is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    found in the file LICENSE that should have accompanied this file.

    This package is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
*/

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace inputleap {

enum class PerformanceStage : std::size_t {
    INPUT_CAPTURE,
    CAPTURE_TO_SERVER_DISPATCH,
    SERVER_DISPATCH,
    CLIENT_PROTOCOL,
    INPUT_INJECTION,
    COUNT
};

struct PerformanceSnapshot {
    std::uint64_t count = 0;
    std::uint64_t total_us = 0;
    std::uint64_t min_us = 0;
    std::uint64_t max_us = 0;
    std::uint64_t p50_us = 0;
    std::uint64_t p95_us = 0;
    std::uint64_t p99_us = 0;

    double average_us() const
    {
        return count == 0 ? 0.0 : static_cast<double>(total_us) / static_cast<double>(count);
    }
};

class PerformanceMetrics {
public:
    static PerformanceMetrics& instance();

    void set_enabled(bool enabled);
    bool enabled() const;

    void record(PerformanceStage stage, std::uint64_t elapsed_us);
    void record_since(PerformanceStage stage, std::uint64_t start_us);

    PerformanceSnapshot snapshot(PerformanceStage stage) const;
    void reset();
    void log_summary() const;

private:
    static constexpr std::size_t kHistogramBuckets = 64;

    struct Metric {
        std::atomic<std::uint64_t> count{0};
        std::atomic<std::uint64_t> total_us{0};
        std::atomic<std::uint64_t> min_us{(std::numeric_limits<std::uint64_t>::max)()};
        std::atomic<std::uint64_t> max_us{0};
        std::array<std::atomic<std::uint64_t>, kHistogramBuckets> histogram{};
    };

    static std::size_t histogram_bucket(std::uint64_t elapsed_us);
    static std::uint64_t percentile(const Metric& metric, std::uint64_t count,
                                    std::uint64_t numerator);

    std::atomic<bool> enabled_{false};
    std::array<Metric, static_cast<std::size_t>(PerformanceStage::COUNT)> metrics_{};
};

std::uint64_t performance_timestamp_us();
const char* performance_stage_name(PerformanceStage stage);

class ScopedPerformanceTimer {
public:
    explicit ScopedPerformanceTimer(PerformanceStage stage);
    ~ScopedPerformanceTimer();

    ScopedPerformanceTimer(const ScopedPerformanceTimer&) = delete;
    ScopedPerformanceTimer& operator=(const ScopedPerformanceTimer&) = delete;

private:
    PerformanceStage stage_;
    std::uint64_t start_us_;
};

} // namespace inputleap
