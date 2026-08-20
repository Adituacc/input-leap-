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

#include "base/PerformanceMetrics.h"

#include "base/Log.h"

#include <algorithm>

namespace inputleap {

PerformanceMetrics& PerformanceMetrics::instance()
{
    static PerformanceMetrics metrics;
    return metrics;
}

void PerformanceMetrics::set_enabled(bool enabled)
{
    enabled_.store(enabled, std::memory_order_release);
}

bool PerformanceMetrics::enabled() const
{
    return enabled_.load(std::memory_order_acquire);
}

std::size_t PerformanceMetrics::histogram_bucket(std::uint64_t elapsed_us)
{
    std::size_t bucket = 0;
    while (elapsed_us > 1 && bucket + 1 < kHistogramBuckets) {
        elapsed_us = (elapsed_us + 1) >> 1;
        ++bucket;
    }
    return bucket;
}

void PerformanceMetrics::record(PerformanceStage stage, std::uint64_t elapsed_us)
{
    if (!enabled()) {
        return;
    }

    auto& metric = metrics_[static_cast<std::size_t>(stage)];
    metric.count.fetch_add(1, std::memory_order_relaxed);
    metric.total_us.fetch_add(elapsed_us, std::memory_order_relaxed);
    metric.histogram[histogram_bucket(elapsed_us)].fetch_add(1, std::memory_order_relaxed);

    auto minimum = metric.min_us.load(std::memory_order_relaxed);
    while (elapsed_us < minimum &&
           !metric.min_us.compare_exchange_weak(minimum, elapsed_us, std::memory_order_relaxed)) {
    }

    auto maximum = metric.max_us.load(std::memory_order_relaxed);
    while (elapsed_us > maximum &&
           !metric.max_us.compare_exchange_weak(maximum, elapsed_us, std::memory_order_relaxed)) {
    }
}

void PerformanceMetrics::record_since(PerformanceStage stage, std::uint64_t start_us)
{
    if (start_us == 0 || !enabled()) {
        return;
    }

    const auto end_us = performance_timestamp_us();
    record(stage, end_us >= start_us ? end_us - start_us : 0);
}

std::uint64_t PerformanceMetrics::percentile(const Metric& metric, std::uint64_t count,
                                             std::uint64_t numerator)
{
    if (count == 0) {
        return 0;
    }

    const auto target = (count * numerator + 99) / 100;
    std::uint64_t cumulative = 0;
    for (std::size_t bucket = 0; bucket < kHistogramBuckets; ++bucket) {
        cumulative += metric.histogram[bucket].load(std::memory_order_relaxed);
        if (cumulative >= target) {
            if (bucket == kHistogramBuckets - 1) {
                return (std::numeric_limits<std::uint64_t>::max)();
            }
            return std::uint64_t{1} << bucket;
        }
    }
    return metric.max_us.load(std::memory_order_relaxed);
}

PerformanceSnapshot PerformanceMetrics::snapshot(PerformanceStage stage) const
{
    const auto& metric = metrics_[static_cast<std::size_t>(stage)];
    PerformanceSnapshot result;
    result.count = metric.count.load(std::memory_order_relaxed);
    result.total_us = metric.total_us.load(std::memory_order_relaxed);
    result.min_us = result.count == 0 ? 0 : metric.min_us.load(std::memory_order_relaxed);
    result.max_us = metric.max_us.load(std::memory_order_relaxed);
    result.p50_us = percentile(metric, result.count, 50);
    result.p95_us = percentile(metric, result.count, 95);
    result.p99_us = percentile(metric, result.count, 99);
    return result;
}

void PerformanceMetrics::reset()
{
    for (auto& metric : metrics_) {
        metric.count.store(0, std::memory_order_relaxed);
        metric.total_us.store(0, std::memory_order_relaxed);
        metric.min_us.store((std::numeric_limits<std::uint64_t>::max)(),
                            std::memory_order_relaxed);
        metric.max_us.store(0, std::memory_order_relaxed);
        for (auto& bucket : metric.histogram) {
            bucket.store(0, std::memory_order_relaxed);
        }
    }
}

void PerformanceMetrics::log_summary() const
{
    if (!enabled()) {
        return;
    }

    for (std::size_t index = 0;
         index < static_cast<std::size_t>(PerformanceStage::STAGE_COUNT);
         ++index) {
        const auto stage = static_cast<PerformanceStage>(index);
        const auto value = snapshot(stage);
        if (value.count == 0) {
            continue;
        }

        LOG_NOTE("performance %s: count=%llu avg=%.1fus min=%lluus p50=%lluus "
                 "p95=%lluus p99=%lluus max=%lluus",
                 performance_stage_name(stage),
                 static_cast<unsigned long long>(value.count),
                 value.average_us(),
                 static_cast<unsigned long long>(value.min_us),
                 static_cast<unsigned long long>(value.p50_us),
                 static_cast<unsigned long long>(value.p95_us),
                 static_cast<unsigned long long>(value.p99_us),
                 static_cast<unsigned long long>(value.max_us));
    }
}

std::uint64_t performance_timestamp_us()
{
    if (!PerformanceMetrics::instance().enabled()) {
        return 0;
    }

    const auto since_epoch = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(since_epoch).count());
}

const char* performance_stage_name(PerformanceStage stage)
{
    switch (stage) {
    case PerformanceStage::INPUT_CAPTURE:
        return "input-capture";
    case PerformanceStage::CAPTURE_TO_SERVER_DISPATCH:
        return "capture-to-server-dispatch";
    case PerformanceStage::SERVER_DISPATCH:
        return "server-dispatch";
    case PerformanceStage::CLIENT_PROTOCOL:
        return "client-protocol";
    case PerformanceStage::INPUT_INJECTION:
        return "input-injection";
    case PerformanceStage::STAGE_COUNT:
        break;
    }
    return "unknown";
}

ScopedPerformanceTimer::ScopedPerformanceTimer(PerformanceStage stage) :
    stage_{stage},
    start_us_{performance_timestamp_us()}
{
}

ScopedPerformanceTimer::~ScopedPerformanceTimer()
{
    PerformanceMetrics::instance().record_since(stage_, start_us_);
}

} // namespace inputleap
