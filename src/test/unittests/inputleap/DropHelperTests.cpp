/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/DropHelper.h"
#include "io/filesystem.h"

#include <gtest/gtest.h>
#include <fstream>
#include <iterator>
#include <string>

namespace inputleap {

TEST(DropHelperTest, SanitizesNameAndDoesNotOverwriteExistingFile)
{
    const auto directory =
        fs::u8path(testing::TempDir()) / "inputleap-drop-helper-test";
    fs::remove_all(directory);
    fs::create_directories(directory);

    {
        std::ofstream existing((directory / "photo.png").u8string(), std::ios::binary);
        existing << "existing";
    }

    DragInformation info;
    std::string unsafe_name = "../../photo.png";
    info.setFilename(unsafe_name);
    DragFileList files{info};
    std::string payload{"new image bytes"};

    DropHelper::writeToDir(directory.u8string(), files, payload);

    EXPECT_TRUE(fs::exists(directory / "photo.png"));
    ASSERT_TRUE(fs::exists(directory / "photo (1).png"));

    std::ifstream written((directory / "photo (1).png").u8string(), std::ios::binary);
    const std::string contents{std::istreambuf_iterator<char>{written},
                               std::istreambuf_iterator<char>{}};
    written.close();
    EXPECT_EQ(contents, payload);
    EXPECT_TRUE(files.empty());

    fs::remove_all(directory);
}

} // namespace inputleap
