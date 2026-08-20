/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 */

#include "inputleap/ScrollDirection.h"

#include <gtest/gtest.h>

namespace inputleap {

TEST(ScrollDirectionTests, leavesStandardScrollingUnchanged)
{
    EXPECT_EQ(apply_scroll_direction(120, -240, false), std::make_pair(120, -240));
}

TEST(ScrollDirectionTests, reversesBothScrollAxes)
{
    EXPECT_EQ(apply_scroll_direction(120, -240, true), std::make_pair(-120, 240));
}

} // namespace inputleap
