/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/TransferFrame.h"

#include <stdexcept>

namespace inputleap {

namespace {

constexpr char kFrameMagic[] = {'I', 'L', 'F', '2'};
constexpr std::size_t kFrameOverhead =
    sizeof(kFrameMagic) + 1 + TransferManifest::kTransferIdLength + 4 + 8 + 4;

void set_error(std::string* error, const std::string& value)
{
    if (error != nullptr) {
        *error = value;
    }
}

bool known_type(TransferFrameType type)
{
    switch (type) {
    case TransferFrameType::Manifest:
    case TransferFrameType::Chunk:
    case TransferFrameType::EntryComplete:
    case TransferFrameType::TransferComplete:
    case TransferFrameType::Cancel:
    case TransferFrameType::ResumeRequest:
    case TransferFrameType::ResumeState:
    case TransferFrameType::Error:
        return true;
    }
    return false;
}

void append_u32(std::string& wire, std::uint32_t value)
{
    wire.push_back(static_cast<char>((value >> 24) & 0xff));
    wire.push_back(static_cast<char>((value >> 16) & 0xff));
    wire.push_back(static_cast<char>((value >> 8) & 0xff));
    wire.push_back(static_cast<char>(value & 0xff));
}

void append_u64(std::string& wire, std::uint64_t value)
{
    for (int shift = 56; shift >= 0; shift -= 8) {
        wire.push_back(static_cast<char>((value >> shift) & 0xff));
    }
}

bool take_u32(const std::string& wire, std::size_t& offset, std::uint32_t& value)
{
    if (offset > wire.size() || wire.size() - offset < 4) {
        return false;
    }
    const auto* bytes =
        reinterpret_cast<const unsigned char*>(wire.data() + offset);
    value = (static_cast<std::uint32_t>(bytes[0]) << 24) |
            (static_cast<std::uint32_t>(bytes[1]) << 16) |
            (static_cast<std::uint32_t>(bytes[2]) << 8) |
            static_cast<std::uint32_t>(bytes[3]);
    offset += 4;
    return true;
}

bool take_u64(const std::string& wire, std::size_t& offset, std::uint64_t& value)
{
    if (offset > wire.size() || wire.size() - offset < 8) {
        return false;
    }
    value = 0;
    for (int index = 0; index < 8; ++index) {
        value = (value << 8) |
                static_cast<unsigned char>(wire[offset + index]);
    }
    offset += 8;
    return true;
}

} // namespace

bool TransferFrame::validate(std::string* error) const
{
    if (!known_type(type)) {
        set_error(error, "unknown transfer frame type");
        return false;
    }
    if (!TransferManifest::is_transfer_id(transfer_id)) {
        set_error(error, "invalid transfer frame ID");
        return false;
    }
    if (payload.size() > kMaxFramePayload ||
        (type == TransferFrameType::Chunk &&
         payload.size() > kMaxChunkPayload)) {
        set_error(error, "transfer frame payload is too large");
        return false;
    }
    if (type == TransferFrameType::Manifest) {
        TransferManifest manifest;
        if (!TransferManifest::deserialize(payload, manifest, error) ||
            manifest.transfer_id() != transfer_id) {
            set_error(error, "transfer frame contains a mismatched manifest");
            return false;
        }
    }
    if ((type == TransferFrameType::EntryComplete ||
         type == TransferFrameType::TransferComplete ||
         type == TransferFrameType::Cancel ||
         type == TransferFrameType::ResumeRequest) &&
        !payload.empty()) {
        set_error(error, "control transfer frame contains unexpected payload");
        return false;
    }
    if (type == TransferFrameType::ResumeState) {
        std::vector<std::uint64_t> offsets;
        if (!deserialize_resume_offsets(payload, offsets, error)) {
            return false;
        }
    }
    return true;
}

std::string TransferFrame::serialize() const
{
    std::string error;
    if (!validate(&error)) {
        throw std::invalid_argument(error);
    }
    std::string wire;
    wire.reserve(kFrameOverhead + payload.size());
    wire.append(kFrameMagic, sizeof(kFrameMagic));
    wire.push_back(static_cast<char>(type));
    wire.append(transfer_id);
    append_u32(wire, entry_index);
    append_u64(wire, offset);
    append_u32(wire, static_cast<std::uint32_t>(payload.size()));
    wire.append(payload);
    return wire;
}

bool TransferFrame::deserialize(const std::string& wire, TransferFrame& frame,
                                std::string* error)
{
    if (wire.size() < kFrameOverhead ||
        wire.compare(0, sizeof(kFrameMagic), kFrameMagic,
                     sizeof(kFrameMagic)) != 0) {
        set_error(error, "invalid transfer frame header");
        return false;
    }

    TransferFrame parsed;
    std::size_t offset = sizeof(kFrameMagic);
    parsed.type = static_cast<TransferFrameType>(
        static_cast<unsigned char>(wire[offset++]));
    parsed.transfer_id.assign(wire.data() + offset,
                              TransferManifest::kTransferIdLength);
    offset += TransferManifest::kTransferIdLength;

    std::uint32_t payload_size = 0;
    if (!take_u32(wire, offset, parsed.entry_index) ||
        !take_u64(wire, offset, parsed.offset) ||
        !take_u32(wire, offset, payload_size) ||
        payload_size > kMaxFramePayload ||
        offset > wire.size() || wire.size() - offset != payload_size) {
        set_error(error, "truncated or oversized transfer frame");
        return false;
    }
    parsed.payload.assign(wire.data() + offset, payload_size);
    if (!parsed.validate(error)) {
        return false;
    }
    frame = std::move(parsed);
    return true;
}

std::string TransferFrame::serialize_resume_offsets(
    const std::vector<std::uint64_t>& offsets)
{
    if (offsets.size() > TransferManifest::kMaxEntries) {
        throw std::invalid_argument("too many transfer resume offsets");
    }
    std::string payload;
    payload.reserve(4 + offsets.size() * 8);
    append_u32(payload, static_cast<std::uint32_t>(offsets.size()));
    for (const auto offset : offsets) {
        append_u64(payload, offset);
    }
    return payload;
}

bool TransferFrame::deserialize_resume_offsets(
    const std::string& payload, std::vector<std::uint64_t>& offsets,
    std::string* error)
{
    std::size_t position = 0;
    std::uint32_t count = 0;
    if (!take_u32(payload, position, count) ||
        count > TransferManifest::kMaxEntries ||
        payload.size() - position != static_cast<std::size_t>(count) * 8) {
        set_error(error, "invalid transfer resume state");
        return false;
    }
    std::vector<std::uint64_t> parsed;
    parsed.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint64_t offset = 0;
        if (!take_u64(payload, position, offset)) {
            set_error(error, "truncated transfer resume state");
            return false;
        }
        parsed.push_back(offset);
    }
    offsets = std::move(parsed);
    return true;
}

} // namespace inputleap
