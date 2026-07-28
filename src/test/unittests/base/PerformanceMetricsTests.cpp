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

#include <gtest/gtest.h>

namespace inputleap {

class PerformanceMetricsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        metrics_.set_enabled(false);
        metrics_.reset();
    }

    void TearDown() override
    {
        metrics_.set_enabled(false);
        metrics_.reset();
    }

    PerformanceMetrics& metrics_ = PerformanceMetrics::instance();
};

TEST_F(PerformanceMetricsTest, DisabledCollectorDoesNotRecord)
{
    metrics_.record(PerformanceStage::SERVER_DISPATCH, 42);

    EXPECT_EQ(metrics_.snapshot(PerformanceStage::SERVER_DISPATCH).count, 0u);
    EXPECT_EQ(performance_timestamp_us(), 0u);
}

TEST_F(PerformanceMetricsTest, SnapshotAggregatesDurationsAndPercentiles)
{
    metrics_.set_enabled(true);
    for (std::uint64_t value = 1; value <= 100; ++value) {
        metrics_.record(PerformanceStage::INPUT_INJECTION, value);
    }

    const auto snapshot = metrics_.snapshot(PerformanceStage::INPUT_INJECTION);
    EXPECT_EQ(snapshot.count, 100u);
    EXPECT_EQ(snapshot.total_us, 5050u);
    EXPECT_EQ(snapshot.min_us, 1u);
    EXPECT_EQ(snapshot.max_us, 100u);
    EXPECT_DOUBLE_EQ(snapshot.average_us(), 50.5);
    EXPECT_EQ(snapshot.p50_us, 64u);
    EXPECT_EQ(snapshot.p95_us, 128u);
    EXPECT_EQ(snapshot.p99_us, 128u);
}

TEST_F(PerformanceMetricsTest, ResetClearsExistingSamples)
{
    metrics_.set_enabled(true);
    metrics_.record(PerformanceStage::INPUT_CAPTURE, 10);
    metrics_.reset();

    const auto snapshot = metrics_.snapshot(PerformanceStage::INPUT_CAPTURE);
    EXPECT_EQ(snapshot.count, 0u);
    EXPECT_EQ(snapshot.total_us, 0u);
    EXPECT_EQ(snapshot.min_us, 0u);
    EXPECT_EQ(snapshot.max_us, 0u);
}

} // namespace inputleap
