/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/PacketStreamFilter.h"
#include "inputleap/protocol_types.h"
#include "base/EventTarget.h"
#include "test/mock/inputleap/MockEventQueue.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace inputleap {
namespace {

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::SaveArg;

class MemoryStream final : public IStream, public EventTarget {
public:
    explicit MemoryStream(std::vector<std::uint8_t> bytes) :
        bytes_{std::move(bytes)}
    {
    }

    void close() override {}

    std::uint32_t read(void* output, std::uint32_t size) override
    {
        ++read_calls_;
        const auto remaining = bytes_.size() - offset_;
        const auto count = std::min<std::size_t>(remaining, size);
        if (count > 0 && output != nullptr) {
            std::memcpy(output, bytes_.data() + offset_, count);
        }
        offset_ += count;
        return static_cast<std::uint32_t>(count);
    }

    void write(const void*, std::uint32_t) override {}
    void flush() override {}
    void shutdownInput() override {}
    void shutdownOutput() override {}
    const EventTarget* get_event_target() const override { return this; }
    bool isReady() const override { return offset_ < bytes_.size(); }
    std::uint32_t getSize() const override
    {
        return static_cast<std::uint32_t>(bytes_.size() - offset_);
    }

    std::size_t read_calls() const { return read_calls_; }

private:
    std::vector<std::uint8_t> bytes_;
    std::size_t offset_ = 0;
    std::size_t read_calls_ = 0;
};

std::vector<std::uint8_t> packet_header(std::uint32_t size)
{
    return {
        static_cast<std::uint8_t>((size >> 24) & 0xff),
        static_cast<std::uint8_t>((size >> 16) & 0xff),
        static_cast<std::uint8_t>((size >> 8) & 0xff),
        static_cast<std::uint8_t>(size & 0xff)
    };
}

TEST(PacketStreamFilterTest, RejectsOversizedPacketAndStopsReading)
{
    NiceMock<MockEventQueue> events;
    IEventQueue::EventHandler upstream_handler;
    auto source = std::make_unique<MemoryStream>(
        packet_header(PROTOCOL_MAX_MESSAGE_LENGTH + 1));
    auto* source_ptr = source.get();

    EXPECT_CALL(events, add_handler(EventType::UNKNOWN, source_ptr, _))
        .WillOnce(SaveArg<2>(&upstream_handler));

    EventType reported_type = EventType::UNKNOWN;
    EXPECT_CALL(events, add_event(_))
        .WillOnce(Invoke([&reported_type](Event&& event) {
            reported_type = event.getType();
        }));

    PacketStreamFilter filter(&events, std::move(source));
    ASSERT_TRUE(static_cast<bool>(upstream_handler));

    upstream_handler(Event(EventType::STREAM_INPUT_READY, source_ptr));
    EXPECT_EQ(reported_type, EventType::STREAM_INPUT_FORMAT_ERROR);
    const auto reads_after_error = source_ptr->read_calls();

    upstream_handler(Event(EventType::STREAM_INPUT_READY, source_ptr));
    EXPECT_EQ(source_ptr->read_calls(), reads_after_error);
    EXPECT_FALSE(filter.isReady());
}

TEST(PacketStreamFilterTest, RejectsZeroLengthPacket)
{
    NiceMock<MockEventQueue> events;
    IEventQueue::EventHandler upstream_handler;
    auto source = std::make_unique<MemoryStream>(packet_header(0));
    auto* source_ptr = source.get();

    EXPECT_CALL(events, add_handler(EventType::UNKNOWN, source_ptr, _))
        .WillOnce(SaveArg<2>(&upstream_handler));
    EXPECT_CALL(events, add_event(_))
        .WillOnce(Invoke([](Event&& event) {
            EXPECT_EQ(event.getType(), EventType::STREAM_INPUT_FORMAT_ERROR);
        }));

    PacketStreamFilter filter(&events, std::move(source));
    upstream_handler(Event(EventType::STREAM_INPUT_READY, source_ptr));
    EXPECT_FALSE(filter.isReady());
}

} // namespace
} // namespace inputleap
