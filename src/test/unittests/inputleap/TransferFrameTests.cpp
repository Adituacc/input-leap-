/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/TransferFrame.h"

#include "test/global/gtest.h"

namespace inputleap {

TEST(TransferFrameTests, chunkRoundTripPreservesBinaryPayload)
{
    TransferFrame source{
        TransferFrameType::Chunk,
        "00112233445566778899aabbccddeeff",
        42,
        1024,
        std::string{"a\0b", 3}
    };

    TransferFrame parsed;
    std::string error;
    ASSERT_TRUE(
        TransferFrame::deserialize(source.serialize(), parsed, &error)) << error;
    EXPECT_EQ(parsed.type, source.type);
    EXPECT_EQ(parsed.transfer_id, source.transfer_id);
    EXPECT_EQ(parsed.entry_index, 42u);
    EXPECT_EQ(parsed.offset, 1024u);
    EXPECT_EQ(parsed.payload, source.payload);
}

TEST(TransferFrameTests, rejectsOversizedChunkAndMismatchedManifest)
{
    TransferFrame oversized{
        TransferFrameType::Chunk,
        "00112233445566778899aabbccddeeff",
        0,
        0,
        std::string(TransferFrame::kMaxChunkPayload + 1, 'x')
    };
    EXPECT_FALSE(oversized.validate());

    TransferManifest manifest;
    manifest.set_transfer_id("11112233445566778899aabbccddeeff");
    manifest.entries().push_back(
        {TransferEntryKind::File, "file.txt", 1,
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"});
    TransferFrame mismatched{
        TransferFrameType::Manifest,
        "22112233445566778899aabbccddeeff",
        0,
        0,
        manifest.serialize()
    };
    EXPECT_FALSE(mismatched.validate());
}

} // namespace inputleap
