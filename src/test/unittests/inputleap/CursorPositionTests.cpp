/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/CursorPosition.h"

#include "test/global/gtest.h"

namespace inputleap {

TEST(CursorPositionTests, mapsCompleteRangeIncludingNegativeDesktopOrigin)
{
    EXPECT_EQ(normalize_absolute_cursor_coordinate(-1920, -1920, 3840), 0u);
    EXPECT_EQ(normalize_absolute_cursor_coordinate(1919, -1920, 3840), 65535u);
    EXPECT_EQ(normalize_absolute_cursor_coordinate(0, -1920, 3840), 32776u);
}

TEST(CursorPositionTests, clampsCoordinatesOutsideVirtualDesktop)
{
    EXPECT_EQ(normalize_absolute_cursor_coordinate(-2000, -1920, 3840), 0u);
    EXPECT_EQ(normalize_absolute_cursor_coordinate(2500, -1920, 3840), 65535u);
}

TEST(CursorPositionTests, handlesDegenerateDesktopDimensions)
{
    EXPECT_EQ(normalize_absolute_cursor_coordinate(100, 100, 0), 0u);
    EXPECT_EQ(normalize_absolute_cursor_coordinate(100, 100, 1), 0u);
}

} // namespace inputleap
