/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 */

#include "inputleap/PressedInputState.h"
#include "inputleap/ReconnectBackoff.h"

#include "test/global/gtest.h"

namespace inputleap {

TEST(InputRecoveryTests, tracksPressedMouseButtonsWithoutDuplicates)
{
    PressedMouseButtons pressed;
    pressed.press(kButtonLeft);
    pressed.press(kButtonLeft);
    pressed.press(kButtonRight);
    EXPECT_EQ(pressed.buttons().size(), 2u);
    pressed.release(kButtonLeft);
    EXPECT_EQ(pressed.buttons().count(kButtonLeft), 0u);
    pressed.clear();
    EXPECT_TRUE(pressed.empty());
}

TEST(InputRecoveryTests, reconnectBackoffIsFastThenBounded)
{
    ReconnectBackoff backoff;
    EXPECT_DOUBLE_EQ(backoff.next_delay(), 0.5);
    EXPECT_DOUBLE_EQ(backoff.next_delay(), 1.0);
    EXPECT_DOUBLE_EQ(backoff.next_delay(), 2.0);
    EXPECT_DOUBLE_EQ(backoff.next_delay(), 5.0);
    EXPECT_DOUBLE_EQ(backoff.next_delay(), 10.0);
    EXPECT_DOUBLE_EQ(backoff.next_delay(), 10.0);
    backoff.reset();
    EXPECT_DOUBLE_EQ(backoff.next_delay(), 0.5);
}

} // namespace inputleap
