/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace inputleap {

enum class TransferEntryKind : std::uint8_t {
    File = 0,
    Directory = 1,
    Image = 2,
    InternetShortcut = 3
};

struct TransferEntry {
    TransferEntryKind kind = TransferEntryKind::File;
    std::string relative_path;
    std::uint64_t size = 0;
    std::string sha256;
};

class TransferManifest {
public:
    static constexpr std::uint8_t kWireVersion = 2;
    static constexpr std::size_t kTransferIdLength = 32;
    static constexpr std::size_t kSha256Length = 64;
    static constexpr std::size_t kMaxEntries = 2048;
    static constexpr std::size_t kMaxRelativePathLength = 1024;
    static constexpr std::size_t kMaxSerializedSize = 900u * 1024u;
    static constexpr std::uint64_t kMaxTransferSize =
        64ull * 1024ull * 1024ull * 1024ull;

    const std::string& transfer_id() const { return transfer_id_; }
    void set_transfer_id(std::string value) { transfer_id_ = std::move(value); }

    const std::vector<TransferEntry>& entries() const { return entries_; }
    std::vector<TransferEntry>& entries() { return entries_; }

    std::uint64_t total_size() const;
    bool validate(std::string* error = nullptr) const;
    std::string serialize() const;

    static bool deserialize(const std::string& wire, TransferManifest& manifest,
                            std::string* error = nullptr);
    static bool is_safe_relative_path(const std::string& path);
    static bool is_sha256(const std::string& value);
    static bool is_transfer_id(const std::string& value);

private:
    std::string transfer_id_;
    std::vector<TransferEntry> entries_;
};

} // namespace inputleap
