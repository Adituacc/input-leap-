/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 */

#include "inputleap/CursorHandoff.h"

#include "test/global/gtest.h"

namespace inputleap {

TEST(CursorHandoffTests, mapsEdgesWithoutLeavingDestinationBounds)
{
    EXPECT_DOUBLE_EQ(cursor_edge_fraction(-100, -100, 200), 0.0025);
    EXPECT_EQ(cursor_coordinate_from_fraction(0.0025, 50, 100), 50);
    EXPECT_EQ(cursor_coordinate_from_fraction(1.0, 50, 100), 149);
}

TEST(CursorHandoffTests, insetsEveryDestinationEdge)
{
    const CursorRect rect{-1920, 0, 1920, 1080};
    std::int32_t x = -1920;
    std::int32_t y = 0;
    inset_cursor_on_entry(rect, kRight, 8, x, y);
    EXPECT_EQ(x, -1912);
    EXPECT_EQ(y, 0);

    x = -1;
    y = 1079;
    inset_cursor_on_entry(rect, kLeft, 8, x, y);
    EXPECT_EQ(x, -9);
    EXPECT_EQ(y, 1079);
}

TEST(CursorHandoffTests, suppressesOnlyImmediateReverseMotion)
{
    CursorHandoffGuard guard;
    guard.begin(kRight);
    EXPECT_TRUE(guard.should_suppress(-4, 0, 0.01));
    EXPECT_TRUE(guard.active());
    EXPECT_FALSE(guard.should_suppress(0, 3, 0.02));
    EXPECT_FALSE(guard.should_suppress(2, 0, 0.03));
    EXPECT_FALSE(guard.active());

    guard.begin(kBottom);
    EXPECT_FALSE(guard.should_suppress(0, -4, 0.2));
    EXPECT_FALSE(guard.active());
}

} // namespace inputleap
