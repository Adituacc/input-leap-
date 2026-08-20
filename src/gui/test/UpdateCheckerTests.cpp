/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 */

#include "UpdateChecker.h"

#include <gtest/gtest.h>

TEST(UpdateCheckerTests, comparesSemanticReleaseTags)
{
    EXPECT_TRUE(isNewerVersion(QStringLiteral("v3.1.0"),
                               QStringLiteral("3.0.3")));
    EXPECT_FALSE(isNewerVersion(QStringLiteral("v3.0.3"),
                                QStringLiteral("3.0.3")));
    EXPECT_FALSE(isNewerVersion(QStringLiteral("v2.9.9"),
                                QStringLiteral("3.0.3")));
}

TEST(UpdateCheckerTests, rejectsMalformedVersions)
{
    EXPECT_FALSE(isNewerVersion(QStringLiteral("nightly"),
                                QStringLiteral("3.0.3")));
}
