/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/ProtocolUtil.h"
#include "io/IStream.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace inputleap {
namespace {

class RecordingStream final : public IStream {
public:
    void close() override {}
    std::uint32_t read(void*, std::uint32_t) override { return 0; }

    void write(const void* data, std::uint32_t size) override
    {
        const auto* first = static_cast<const std::uint8_t*>(data);
        bytes.assign(first, first + size);
    }

    void flush() override {}
    void shutdownInput() override {}
    void shutdownOutput() override {}
    const EventTarget* get_event_target() const override { return nullptr; }
    bool isReady() const override { return false; }
    std::uint32_t getSize() const override { return 0; }

    std::vector<std::uint8_t> bytes;
};

TEST(ProtocolUtilTest, SerializesSmallInputMessage)
{
    RecordingStream stream;

    ProtocolUtil::writef(&stream, "DMMV%2i%2i", 0x1234u, 0xabcdu);

    const std::vector<std::uint8_t> expected{
        'D', 'M', 'M', 'V', 0x12, 0x34, 0xab, 0xcd
    };
    EXPECT_EQ(stream.bytes, expected);
}

TEST(ProtocolUtilTest, SerializesLargeStringPayload)
{
    RecordingStream stream;
    const std::string payload(1024, 'x');

    ProtocolUtil::writef(&stream, "DATA%s", &payload);

    ASSERT_EQ(stream.bytes.size(), payload.size() + 8);
    EXPECT_EQ(std::string(stream.bytes.begin(), stream.bytes.begin() + 4), "DATA");
    EXPECT_EQ(stream.bytes[4], 0);
    EXPECT_EQ(stream.bytes[5], 0);
    EXPECT_EQ(stream.bytes[6], 4);
    EXPECT_EQ(stream.bytes[7], 0);
    EXPECT_TRUE(std::all_of(stream.bytes.begin() + 8, stream.bytes.end(),
                            [](std::uint8_t byte) { return byte == 'x'; }));
}

} // namespace
} // namespace inputleap
