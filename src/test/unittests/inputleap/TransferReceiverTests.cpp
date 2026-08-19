/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/TransferCatalog.h"
#include "inputleap/TransferHash.h"
#include "inputleap/TransferProgress.h"
#include "inputleap/TransferReceiver.h"
#include "inputleap/TransferSender.h"

#include "test/global/gtest.h"

#include <fstream>

namespace inputleap {

namespace {

fs::path test_root()
{
    return fs::temp_directory_path() /
           "input-leap-transfer-v2-receiver-tests";
}

void write_file(const fs::path& path, const std::string& data)
{
    fs::create_directories(path.parent_path());
    std::ofstream stream;
    open_utf8_path(stream, path, std::ios::out | std::ios::binary);
    stream.write(data.data(), static_cast<std::streamsize>(data.size()));
}

std::string read_file(const fs::path& path)
{
    std::ifstream stream;
    open_utf8_path(stream, path, std::ios::in | std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

class TransferReceiverTests : public testing::Test {
public:
    void SetUp() override
    {
        fs::remove_all(test_root());
        fs::create_directories(test_root());
    }

    void TearDown() override
    {
        fs::remove_all(test_root());
    }
};

} // namespace

TEST_F(TransferReceiverTests, resumesVerifiesAndCommitsMultipleEntries)
{
    const std::string first = "hello ";
    const std::string second = "world";
    const auto full = first + second;

    TransferManifest manifest;
    manifest.set_transfer_id("10112233445566778899aabbccddeeff");
    manifest.entries().push_back(
        {TransferEntryKind::Directory, "Folder", 0, {}});
    manifest.entries().push_back(
        {TransferEntryKind::File, "Folder/message.txt", full.size(),
         sha256_bytes(full.data(), full.size())});
    manifest.entries().push_back(
        {TransferEntryKind::InternetShortcut, "Site.url", 3,
         sha256_bytes("url", 3)});

    TransferProgress progress;
    {
        TransferReceiver receiver(&progress);
        receiver.begin(manifest, test_root());
        receiver.write_chunk(1, 0, first.data(), first.size());
        EXPECT_EQ(receiver.resume_offsets()[1], first.size());
    }

    TransferReceiver resumed(&progress);
    resumed.begin(manifest, test_root());
    ASSERT_EQ(resumed.resume_offsets()[1], first.size());
    EXPECT_THROW(
        resumed.write_chunk(1, 0, second.data(), second.size()),
        std::invalid_argument);
    resumed.write_chunk(1, first.size(), second.data(), second.size());
    resumed.finish_entry(1);
    resumed.write_chunk(2, 0, "url", 3);
    resumed.finish_entry(2);

    const auto results = resumed.complete();
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(read_file(test_root() / "Folder" / "message.txt"), full);
    EXPECT_EQ(read_file(test_root() / "Site.url"), "url");

    const auto snapshot = progress.snapshot();
    EXPECT_EQ(snapshot.state, TransferState::Completed);
    EXPECT_DOUBLE_EQ(snapshot.fraction(), 1.0);
}

TEST_F(TransferReceiverTests, refusesCorruptEntryAndCancellationRemovesPartialData)
{
    TransferManifest manifest;
    manifest.set_transfer_id("20112233445566778899aabbccddeeff");
    manifest.entries().push_back(
        {TransferEntryKind::File, "message.txt", 3,
         sha256_bytes("abc", 3)});

    TransferReceiver receiver;
    receiver.begin(manifest, test_root());
    receiver.write_chunk(0, 0, "xyz", 3);
    EXPECT_THROW(receiver.finish_entry(0), std::runtime_error);
    receiver.cancel();

    EXPECT_FALSE(fs::exists(test_root() / ".inputleap-partials"));
    EXPECT_FALSE(fs::exists(test_root() / "message.txt"));
}

TEST_F(TransferReceiverTests, repeatedManifestKeepsLiveResumeOffsets)
{
    TransferManifest manifest;
    manifest.set_transfer_id("21112233445566778899aabbccddeeff");
    manifest.entries().push_back(
        {TransferEntryKind::File, "message.txt", 6,
         sha256_bytes("abcdef", 6)});
    const TransferFrame manifest_frame{
        TransferFrameType::Manifest, manifest.transfer_id(), 0, 0,
        manifest.serialize()};

    TransferReceiver receiver;
    receiver.handle_frame(manifest_frame, test_root());
    receiver.write_chunk(0, 0, "abc", 3);
    receiver.handle_frame(manifest_frame, test_root());

    ASSERT_EQ(receiver.resume_offsets().size(), 1u);
    EXPECT_EQ(receiver.resume_offsets()[0], 3u);
}

TEST_F(TransferReceiverTests, progressObserverReceivesTerminalSnapshot)
{
    TransferManifest manifest;
    manifest.set_transfer_id("22112233445566778899aabbccddeeff");
    manifest.entries().push_back(
        {TransferEntryKind::File, "message.txt", 3,
         sha256_bytes("abc", 3)});

    TransferProgress progress;
    std::vector<TransferProgressSnapshot> snapshots;
    progress.set_observer(
        [&snapshots](const TransferProgressSnapshot& snapshot) {
            snapshots.push_back(snapshot);
        });
    TransferReceiver receiver(&progress);
    receiver.begin(manifest, test_root());
    receiver.write_chunk(0, 0, "abc", 3);
    receiver.finish_entry(0);
    receiver.complete();

    ASSERT_FALSE(snapshots.empty());
    EXPECT_EQ(snapshots.back().state, TransferState::Completed);
    EXPECT_EQ(snapshots.back().transferred_bytes, 3u);
}

TEST_F(TransferReceiverTests, catalogBuildsFolderManifestWithHashes)
{
    const auto source = test_root() / "source";
    write_file(source / "a.txt", "alpha");
    write_file(source / "nested" / "b.txt", "beta");

    const auto manifest = TransferCatalog::from_paths({source});
    EXPECT_TRUE(manifest.validate());
    EXPECT_EQ(manifest.entries().size(), 4u);
    EXPECT_EQ(manifest.total_size(), 9u);
    EXPECT_TRUE(TransferManifest::is_transfer_id(manifest.transfer_id()));
}

TEST_F(TransferReceiverTests, catalogKeepsDuplicateTopLevelNamesUnique)
{
    const auto first = test_root() / "first" / "report.txt";
    const auto second = test_root() / "second" / "report.txt";
    write_file(first, "first report");
    write_file(second, "second report");

    const auto plan = TransferCatalog::plan_from_paths({first, second});
    ASSERT_EQ(plan.manifest.entries().size(), 2u);
    EXPECT_EQ(plan.manifest.entries()[0].relative_path, "report.txt");
    EXPECT_EQ(plan.manifest.entries()[1].relative_path, "report (1).txt");
}

#ifndef _WIN32
TEST_F(TransferReceiverTests, catalogKeepsSanitizedSiblingNamesUnique)
{
    const auto source = test_root() / "portable-names";
    write_file(source / "report:final.txt", "colon");
    write_file(source / "report?final.txt", "question");

    const auto plan = TransferCatalog::plan_from_paths({source});
    ASSERT_EQ(plan.manifest.entries().size(), 3u);
    EXPECT_EQ(plan.manifest.entries()[1].relative_path,
              "portable-names/report_final.txt");
    EXPECT_EQ(plan.manifest.entries()[2].relative_path,
              "portable-names/report_final (1).txt");

    TransferSender sender;
    TransferReceiver receiver;
    const auto destination = test_root() / "received-portable-names";
    sender.send(plan, [&](const TransferFrame& frame) {
        receiver.handle_frame(frame, destination);
    });

    EXPECT_EQ(read_file(destination / "portable-names" /
                        "report_final.txt"),
              "colon");
    EXPECT_EQ(read_file(destination / "portable-names" /
                        "report_final (1).txt"),
              "question");
}
#endif

TEST_F(TransferReceiverTests, senderFramesRoundTripThroughStreamingReceiver)
{
    const auto source = test_root() / "send-source";
    const auto destination = test_root() / "received";
    write_file(source / "one.bin", std::string(700000, 'a'));
    write_file(source / "nested" / "two.txt", "second");

    const auto plan = TransferCatalog::plan_from_paths({source});
    TransferProgress send_progress;
    TransferProgress receive_progress;
    TransferSender sender(&send_progress);
    TransferReceiver receiver(&receive_progress);
    std::vector<fs::path> results;

    sender.send(plan, [&](const TransferFrame& outgoing) {
        TransferFrame incoming;
        std::string error;
        ASSERT_TRUE(TransferFrame::deserialize(
            outgoing.serialize(), incoming, &error)) << error;
        auto completed = receiver.handle_frame(incoming, destination);
        if (!completed.empty()) {
            results = std::move(completed);
        }
    });

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(read_file(destination / "send-source" / "one.bin"),
              std::string(700000, 'a'));
    EXPECT_EQ(read_file(destination / "send-source" / "nested" / "two.txt"),
              "second");
    EXPECT_EQ(send_progress.snapshot().state, TransferState::Completed);
    EXPECT_EQ(receive_progress.snapshot().state, TransferState::Completed);
}

TEST_F(TransferReceiverTests, preservesCompleteFolderTreesAsTopLevelItems)
{
    const auto first = test_root() / fs::u8path("extension-folder");
    const auto second = test_root() / fs::u8path("assets");
    const auto destination = test_root() / fs::u8path("received");
    const std::string binary("\0\x01\x7f\xff", 4);

    write_file(first / fs::u8path("manifest.json"),
               R"({"manifest_version":3,"name":"Input Leap test"})");
    write_file(first / fs::u8path(".gitignore"), "dist/\n");
    write_file(first / fs::u8path("src") / fs::u8path("background.js"),
               "export const ready = true;\n");
    write_file(first / fs::u8path("src") / fs::u8path("nested") /
                   fs::u8path("data.bin"),
               binary);
    fs::create_directories(first / fs::u8path("empty") /
                           fs::u8path("keep-this-folder"));
    write_file(second / fs::u8path(u8"icons") / fs::u8path(u8"café.txt"),
               "unicode filename");

    const auto plan = TransferCatalog::plan_from_paths({first, second});
    TransferSender sender;
    TransferReceiver receiver;
    std::vector<fs::path> results;
    sender.send(plan, [&](const TransferFrame& frame) {
        auto completed = receiver.handle_frame(frame, destination);
        if (!completed.empty()) {
            results = std::move(completed);
        }
    });

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].filename(), fs::u8path("assets"));
    EXPECT_EQ(results[1].filename(), fs::u8path("extension-folder"));
    EXPECT_EQ(read_file(destination / fs::u8path("extension-folder") /
                        fs::u8path("manifest.json")),
              R"({"manifest_version":3,"name":"Input Leap test"})");
    EXPECT_EQ(read_file(destination / fs::u8path("extension-folder") /
                        fs::u8path(".gitignore")),
              "dist/\n");
    EXPECT_EQ(read_file(destination / fs::u8path("extension-folder") /
                        fs::u8path("src") / fs::u8path("background.js")),
              "export const ready = true;\n");
    EXPECT_EQ(read_file(destination / fs::u8path("extension-folder") /
                        fs::u8path("src") / fs::u8path("nested") /
                        fs::u8path("data.bin")),
              binary);
    EXPECT_TRUE(fs::is_directory(
        destination / fs::u8path("extension-folder") / fs::u8path("empty") /
        fs::u8path("keep-this-folder")));
    EXPECT_EQ(read_file(destination / fs::u8path("assets") /
                        fs::u8path(u8"icons") / fs::u8path(u8"café.txt")),
              "unicode filename");
}

TEST_F(TransferReceiverTests, senderNegotiatesAndResumesFromReceiverOffsets)
{
    const auto source = test_root() / "resume-source.bin";
    const auto destination = test_root() / "received";
    const std::string contents(900000, 'r');
    write_file(source, contents);

    const auto plan = TransferCatalog::plan_from_paths({source});
    TransferReceiver receiver;
    receiver.begin(plan.manifest, destination);
    receiver.write_chunk(0, 0, contents.data(), 300000);

    TransferSender sender;
    std::vector<fs::path> results;
    sender.send(
        plan,
        [&](const TransferFrame& frame) {
            auto completed = receiver.handle_frame(frame, destination);
            if (!completed.empty()) {
                results = std::move(completed);
            }
        },
        {},
        [&](const TransferManifest&) {
            return receiver.resume_offsets();
        });

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(read_file(destination / "resume-source.bin"), contents);
}

TEST_F(TransferReceiverTests, lostFinalAcknowledgmentDoesNotDuplicateCommit)
{
    const auto source = test_root() / "once.txt";
    const auto destination = test_root() / "received";
    write_file(source, "only once");
    const auto plan = TransferCatalog::plan_from_paths({source});
    TransferReceiver receiver;
    TransferSender sender;

    auto deliver = [&](const TransferFrame& frame) {
        receiver.handle_frame(frame, destination);
    };
    sender.send(plan, deliver);
    ASSERT_TRUE(fs::exists(destination / "once.txt"));

    sender.send(
        plan, deliver, {},
        [&](const TransferManifest&) {
            return receiver.resume_offsets();
        });

    EXPECT_TRUE(fs::exists(destination / "once.txt"));
    EXPECT_FALSE(fs::exists(destination / "once (1).txt"));
}

TEST_F(TransferReceiverTests, createsAndVerifiesEmptyFiles)
{
    TransferManifest manifest;
    manifest.set_transfer_id("30112233445566778899aabbccddeeff");
    manifest.entries().push_back(
        {TransferEntryKind::File, "empty.txt", 0,
         sha256_bytes(nullptr, 0)});

    TransferReceiver receiver;
    receiver.begin(manifest, test_root());
    receiver.finish_entry(0);
    const auto results = receiver.complete();

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(fs::is_regular_file(test_root() / "empty.txt"));
    EXPECT_EQ(fs::file_size(test_root() / "empty.txt"), 0u);
}

} // namespace inputleap
