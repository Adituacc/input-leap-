/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 */

#include "server/Config.h"
#include "inputleap/option_types.h"

#include <gtest/gtest.h>
#include <sstream>

namespace inputleap {

TEST(ConfigSerializationTests, preservesNumericClipboardLimit)
{
    Config config;
    ASSERT_TRUE(config.addScreen("mac"));
    ASSERT_TRUE(config.addOption("", kOptionClipboardSharingSize, 104857600));

    std::ostringstream stream;
    stream << config;
    EXPECT_NE(stream.str().find("clipboardSharingSize = 104857600"),
              std::string::npos);
}

TEST(ConfigSerializationTests, roundTripsPerScreenScrollDirection)
{
    Config original;
    ASSERT_TRUE(original.addScreen("windows"));
    ASSERT_TRUE(original.addOption("windows", kOptionInvertScroll, 1));

    std::stringstream stream;
    stream << original;
    EXPECT_NE(stream.str().find("invertScroll = true"), std::string::npos);

    Config restored;
    stream >> restored;
    ASSERT_TRUE(stream.good() || stream.eof());
    const auto* options = restored.getOptions("windows");
    ASSERT_NE(options, nullptr);
    EXPECT_EQ(options->at(kOptionInvertScroll), 1);
}

} // namespace inputleap
