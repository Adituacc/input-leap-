/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "inputleap/TransferManifest.h"

#include <cstdint>
#include <string>

namespace inputleap {

enum class TransferFrameType : std::uint8_t {
    Manifest = 0,
    Chunk = 1,
    EntryComplete = 2,
    TransferComplete = 3,
    Cancel = 4,
    ResumeRequest = 5,
    ResumeState = 6,
    Error = 7
};

struct TransferFrame {
    static constexpr std::size_t kMaxChunkPayload = 256u * 1024u;
    static constexpr std::size_t kMaxFramePayload = 900u * 1024u;

    TransferFrameType type = TransferFrameType::Error;
    std::string transfer_id;
    std::uint32_t entry_index = 0;
    std::uint64_t offset = 0;
    std::string payload;

    bool validate(std::string* error = nullptr) const;
    std::string serialize() const;

    static bool deserialize(const std::string& wire, TransferFrame& frame,
                            std::string* error = nullptr);
};

} // namespace inputleap
