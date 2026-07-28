/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/DragPayload.h"
#include "io/filesystem.h"

#include <gtest/gtest.h>
#include <string>

namespace inputleap {

TEST(DragPayloadTest, AcceptsOnlyHttpUrlsWithoutLineBreaks)
{
    EXPECT_TRUE(is_supported_drag_url("https://example.com/image.png"));
    EXPECT_TRUE(is_supported_drag_url("HTTP://example.com"));
    EXPECT_FALSE(is_supported_drag_url("file:///etc/passwd"));
    EXPECT_FALSE(is_supported_drag_url("javascript:alert(1)"));
    EXPECT_FALSE(is_supported_drag_url("https://example.com\r\nIconFile=bad.dll"));
}

TEST(DragPayloadTest, BuildsWindowsInternetShortcut)
{
    EXPECT_EQ(make_windows_internet_shortcut("https://example.com/path?q=1"),
              "[InternetShortcut]\r\nURL=https://example.com/path?q=1\r\n");
    EXPECT_TRUE(make_windows_internet_shortcut("javascript:alert(1)").empty());
}

TEST(DragPayloadTest, BuildsPortableEscapedLinkPage)
{
    const auto page = make_portable_link_page(
        "https://example.com/image?a=1&b=2", "Cats & Dogs");
    EXPECT_NE(page.find("Cats &amp; Dogs"), std::string::npos);
    EXPECT_NE(page.find("a=1&amp;b=2"), std::string::npos);
    EXPECT_TRUE(make_portable_link_page("javascript:alert(1)").empty());
}

TEST(DragPayloadTest, SanitizesCrossPlatformFilename)
{
    EXPECT_EQ(sanitize_drag_filename("/tmp/folder/cat:photo,1?.png"),
              "cat_photo_1_.png");
    EXPECT_EQ(sanitize_drag_filename("CON.txt"), "_CON.txt");
    EXPECT_EQ(sanitize_drag_filename("../"), "InputLeap Drop");
    EXPECT_EQ(sanitize_drag_filename("name. "), "name");
}

TEST(DragPayloadTest, MaterializesPayloadWithPortableName)
{
    const auto path = materialize_drag_payload("image bytes", "cat:photo?.png");
    ASSERT_FALSE(path.empty());
    EXPECT_EQ(fs::u8path(path).filename().u8string(), "cat_photo_.png");
    EXPECT_TRUE(fs::is_regular_file(fs::u8path(path)));
    fs::remove_all(fs::u8path(path).parent_path());
}

} // namespace inputleap
