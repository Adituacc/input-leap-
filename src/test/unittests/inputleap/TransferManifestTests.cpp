/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/TransferManifest.h"

#include "test/global/gtest.h"

namespace inputleap {

namespace {

TransferManifest make_manifest()
{
    TransferManifest manifest;
    manifest.set_transfer_id("00112233445566778899aabbccddeeff");
    manifest.entries().push_back(
        {TransferEntryKind::Directory, "Photos", 0, {}});
    manifest.entries().push_back(
        {TransferEntryKind::Image, "Photos/image.png", 3,
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"});
    manifest.entries().push_back(
        {TransferEntryKind::InternetShortcut, "Input Leap.url", 4,
         "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"});
    return manifest;
}

} // namespace

TEST(TransferManifestTests, roundTripPreservesEntries)
{
    const auto source = make_manifest();
    const auto wire = source.serialize();

    TransferManifest parsed;
    std::string error;
    ASSERT_TRUE(TransferManifest::deserialize(wire, parsed, &error)) << error;
    EXPECT_EQ(parsed.transfer_id(), source.transfer_id());
    ASSERT_EQ(parsed.entries().size(), 3u);
    EXPECT_EQ(parsed.entries()[1].kind, TransferEntryKind::Image);
    EXPECT_EQ(parsed.entries()[1].relative_path, "Photos/image.png");
    EXPECT_EQ(parsed.entries()[1].size, 3u);
    EXPECT_EQ(parsed.entries()[1].sha256, source.entries()[1].sha256);
    EXPECT_EQ(parsed.total_size(), 7u);
}

TEST(TransferManifestTests, rejectsUnsafePathsAndMalformedHashes)
{
    const char* unsafe_paths[] = {
        "../secret", "/absolute", "folder//file", "folder\\file",
        "C:/windows", "folder/./file", "folder/../file"
    };
    for (const auto* path : unsafe_paths) {
        EXPECT_FALSE(TransferManifest::is_safe_relative_path(path)) << path;
    }

    auto manifest = make_manifest();
    manifest.entries()[1].relative_path = "../image.png";
    EXPECT_FALSE(manifest.validate());
    manifest.entries()[1].relative_path = "image.png";
    manifest.entries()[1].sha256 = "not-a-digest";
    EXPECT_FALSE(manifest.validate());
}

TEST(TransferManifestTests, rejectsTruncatedAndTrailingWireData)
{
    const auto wire = make_manifest().serialize();
    TransferManifest parsed;
    EXPECT_FALSE(TransferManifest::deserialize(
        wire.substr(0, wire.size() - 1), parsed));
    EXPECT_FALSE(TransferManifest::deserialize(wire + "trailing", parsed));
}

TEST(TransferManifestTests, rejectsNonPortableDuplicateAndConflictingPaths)
{
    auto manifest = make_manifest();
    manifest.entries()[1].relative_path = "Photos/CON";
    EXPECT_FALSE(manifest.validate());

    manifest = make_manifest();
    manifest.entries().push_back(manifest.entries()[1]);
    EXPECT_FALSE(manifest.validate());

    manifest = make_manifest();
    manifest.entries()[0].kind = TransferEntryKind::File;
    manifest.entries()[0].size = 1;
    manifest.entries()[0].sha256 =
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
    EXPECT_FALSE(manifest.validate());
}

} // namespace inputleap
