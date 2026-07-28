/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 */

#include "inputleap/DragInformation.h"
#include "io/filesystem.h"
#include "test/global/gtest.h"

#include <fstream>

namespace inputleap {

TEST(DragInformationTests, parsesMultipleItems)
{
    DragFileList items;
    DragInformation::parseDragInfo(
        items, 3, "first.png,12,folder,0,Dragged Link.html,44,");

    ASSERT_EQ(items.size(), 3u);
    EXPECT_EQ(items[0].getFilename(), "first.png");
    EXPECT_EQ(items[0].getFilesize(), 12u);
    EXPECT_EQ(items[1].getFilename(), "folder");
    EXPECT_EQ(items[1].getFilesize(), 0u);
    EXPECT_EQ(items[2].getFilename(), "Dragged Link.html");
    EXPECT_EQ(items[2].getFilesize(), 44u);
}

TEST(DragInformationTests, setupSupportsFoldersAndSanitizesMetadataNames)
{
    const auto root = fs::u8path(testing::TempDir()) /
                      fs::u8path("inputleap-drag-information-tests");
    fs::remove_all(root);
    fs::create_directories(root / fs::u8path("photos"));
    const auto image = root / fs::u8path("cat,photo.png");
    std::ofstream stream(image.string(), std::ios::binary);
    stream << "image";
    stream.close();

    DragInformation folder_info;
    folder_info.setFilename((root / fs::u8path("photos")).u8string());
    DragInformation image_info;
    image_info.setFilename(image.u8string());
    DragFileList source{folder_info, image_info};

    std::string wire;
    EXPECT_EQ(DragInformation::setupDragInfo(source, wire), 2);
    EXPECT_EQ(wire, "photos,0,cat_photo.png,5,");

    DragFileList parsed;
    DragInformation::parseDragInfo(parsed, 2, wire);
    ASSERT_EQ(parsed.size(), 2u);
    EXPECT_EQ(parsed[0].getFilename(), "photos");
    EXPECT_EQ(parsed[1].getFilename(), "cat_photo.png");

    fs::remove_all(root);
}

} // namespace inputleap
