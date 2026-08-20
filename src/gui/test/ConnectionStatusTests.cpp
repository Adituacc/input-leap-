/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 */

#include "ConnectionStatus.h"

#include <gtest/gtest.h>

TEST(ConnectionStatusTests, parsesStructuredStatusInsideDecoratedLogLine)
{
    ConnectionStatusUpdate update;
    ASSERT_TRUE(parseConnectionStatusLine(
        QStringLiteral("[2026-08-20] INFO INPUTLEAP_STATUS|reconnecting|Retrying in 2.0 seconds"),
        update));
    EXPECT_EQ(update.state, AppConnectionState::RECONNECTING);
    EXPECT_EQ(update.detail, QStringLiteral("Retrying in 2.0 seconds"));
}

TEST(ConnectionStatusTests, rejectsUnknownAndUnstructuredLines)
{
    ConnectionStatusUpdate update;
    EXPECT_FALSE(parseConnectionStatusLine(QStringLiteral("connected to server"), update));
    EXPECT_FALSE(parseConnectionStatusLine(
        QStringLiteral("INPUTLEAP_STATUS|mystery|detail"), update));
}
