/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "platform/MSWindowsWatchdog.h"

#include "test/global/gtest.h"

namespace inputleap {

TEST(MSWindowsWatchdogTests, UnknownDesktopDoesNotAutoElevate)
{
    EXPECT_FALSE(should_auto_elevate_for_desktop(""));
}

TEST(MSWindowsWatchdogTests, DefaultDesktopDoesNotAutoElevate)
{
    EXPECT_FALSE(should_auto_elevate_for_desktop("Default"));
}

TEST(MSWindowsWatchdogTests, SecureDesktopAutoElevates)
{
    EXPECT_TRUE(should_auto_elevate_for_desktop("Winlogon"));
}

TEST(MSWindowsWatchdogTests, NormalLaunchDoesNotEnableUiAccess)
{
    EXPECT_FALSE(should_enable_ui_access(false, false, true));
}

TEST(MSWindowsWatchdogTests, NonDragLaunchPreservesLegacyUiAccess)
{
    EXPECT_TRUE(should_enable_ui_access(false, false, false));
}

TEST(MSWindowsWatchdogTests, ElevatedLaunchEnablesUiAccess)
{
    EXPECT_TRUE(should_enable_ui_access(true, false, true));
    EXPECT_TRUE(should_enable_ui_access(false, true, true));
}

} // namespace inputleap
