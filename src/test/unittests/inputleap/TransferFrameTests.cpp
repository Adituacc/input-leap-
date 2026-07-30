/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/TransferFrame.h"
#include "inputleap/TransferResumeCoordinator.h"

#include "test/global/gtest.h"

#include <thread>

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

TEST(TransferFrameTests, resumeOffsetsRoundTripAndRejectMalformedPayload)
{
    const std::vector<std::uint64_t> expected{0, 17, 0xffffffffull,
                                               0x100000000ull};
    const auto payload = TransferFrame::serialize_resume_offsets(expected);
    TransferFrame frame{
        TransferFrameType::ResumeState,
        "30112233445566778899aabbccddeeff",
        0,
        0,
        payload
    };
    EXPECT_TRUE(frame.validate());

    std::vector<std::uint64_t> parsed;
    std::string error;
    EXPECT_TRUE(TransferFrame::deserialize_resume_offsets(
        payload, parsed, &error)) << error;
    EXPECT_EQ(parsed, expected);

    EXPECT_FALSE(TransferFrame::deserialize_resume_offsets(
        std::string{"\0\0\0\2\0", 5}, parsed, &error));
}

TEST(TransferFrameTests, resumeCoordinatorDeliversOffsetsAcrossThreads)
{
    TransferResumeCoordinator coordinator;
    const std::string transfer_id = "40112233445566778899aabbccddeeff";
    coordinator.prepare(transfer_id, 2);

    std::thread response([&coordinator, &transfer_id]() {
        coordinator.accept(transfer_id, {12, 34});
    });
    EXPECT_EQ(coordinator.wait(), (std::vector<std::uint64_t>{12, 34}));
    response.join();
}

} // namespace inputleap
