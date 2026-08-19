/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/TransferManifest.h"
#include "inputleap/DragPayload.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>

namespace inputleap {

namespace {

constexpr char kMagic[] = {'I', 'L', 'T', '2'};

void set_error(std::string* error, const std::string& value)
{
    if (error != nullptr) {
        *error = value;
    }
}

bool is_hex(const std::string& value)
{
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
    });
}

std::string portable_path_key(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) {
                       return c >= 'A' && c <= 'Z'
                                  ? static_cast<char>(c - 'A' + 'a')
                                  : static_cast<char>(c);
                   });
    return value;
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

bool take_bytes(const std::string& wire, std::size_t& offset, std::size_t count,
                std::string& value)
{
    if (count > wire.size() || offset > wire.size() - count) {
        return false;
    }
    value.assign(wire.data() + offset, count);
    offset += count;
    return true;
}

bool take_u8(const std::string& wire, std::size_t& offset, std::uint8_t& value)
{
    if (offset >= wire.size()) {
        return false;
    }
    value = static_cast<std::uint8_t>(wire[offset++]);
    return true;
}

bool take_u32(const std::string& wire, std::size_t& offset, std::uint32_t& value)
{
    if (offset > wire.size() || wire.size() - offset < 4) {
        return false;
    }
    const auto* data =
        reinterpret_cast<const unsigned char*>(wire.data() + offset);
    value = (static_cast<std::uint32_t>(data[0]) << 24) |
            (static_cast<std::uint32_t>(data[1]) << 16) |
            (static_cast<std::uint32_t>(data[2]) << 8) |
            static_cast<std::uint32_t>(data[3]);
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

bool known_kind(TransferEntryKind kind)
{
    switch (kind) {
    case TransferEntryKind::File:
    case TransferEntryKind::Directory:
    case TransferEntryKind::Image:
    case TransferEntryKind::InternetShortcut:
        return true;
    }
    return false;
}

} // namespace

std::uint64_t TransferManifest::total_size() const
{
    std::uint64_t result = 0;
    for (const auto& entry : entries_) {
        if (entry.size > std::numeric_limits<std::uint64_t>::max() - result) {
            return std::numeric_limits<std::uint64_t>::max();
        }
        result += entry.size;
    }
    return result;
}

bool TransferManifest::is_safe_relative_path(const std::string& path)
{
    if (path.empty() || path.size() > kMaxRelativePathLength ||
        path.front() == '/' || path.front() == '\\' ||
        path.find('\\') != std::string::npos ||
        path.find(':') != std::string::npos ||
        path.find('\0') != std::string::npos) {
        return false;
    }

    std::size_t start = 0;
    while (start <= path.size()) {
        const auto separator = path.find('/', start);
        const auto length = separator == std::string::npos
                                ? path.size() - start
                                : separator - start;
        if (length == 0) {
            return false;
        }
        const auto component = path.substr(start, length);
        if (component == "." || component == ".." ||
            std::any_of(component.begin(), component.end(), [](unsigned char c) {
                return c < 0x20;
            })) {
            return false;
        }
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }
    return true;
}

bool TransferManifest::is_sha256(const std::string& value)
{
    return value.size() == kSha256Length && is_hex(value);
}

bool TransferManifest::is_transfer_id(const std::string& value)
{
    return value.size() == kTransferIdLength && is_hex(value);
}

bool TransferManifest::validate(std::string* error) const
{
    if (!is_transfer_id(transfer_id_)) {
        set_error(error, "transfer ID must be 32 hexadecimal characters");
        return false;
    }
    if (entries_.empty() || entries_.size() > kMaxEntries) {
        set_error(error, "manifest entry count is outside the permitted range");
        return false;
    }

    std::uint64_t total = 0;
    std::map<std::string, TransferEntryKind> paths;
    for (const auto& entry : entries_) {
        if (!known_kind(entry.kind)) {
            set_error(error, "manifest contains an unknown entry kind");
            return false;
        }
        if (!is_safe_relative_path(entry.relative_path)) {
            set_error(error, "manifest contains an unsafe relative path");
            return false;
        }
        std::size_t component_start = 0;
        while (component_start < entry.relative_path.size()) {
            const auto separator =
                entry.relative_path.find('/', component_start);
            const auto component = entry.relative_path.substr(
                component_start,
                separator == std::string::npos
                    ? std::string::npos
                    : separator - component_start);
            if (sanitize_drag_filename(component) != component) {
                set_error(error, "manifest path is not portable across platforms");
                return false;
            }
            if (separator == std::string::npos) {
                break;
            }
            component_start = separator + 1;
        }
        if (!paths.emplace(portable_path_key(entry.relative_path), entry.kind)
                 .second) {
            set_error(error, "manifest contains paths that collide across platforms");
            return false;
        }
        if (entry.kind == TransferEntryKind::Directory) {
            if (entry.size != 0 || !entry.sha256.empty()) {
                set_error(error, "directory entries cannot contain data");
                return false;
            }
        }
        else if (!is_sha256(entry.sha256)) {
            set_error(error, "file entries require a SHA-256 digest");
            return false;
        }
        if (entry.size > kMaxTransferSize - total) {
            set_error(error, "manifest exceeds the maximum transfer size");
            return false;
        }
        total += entry.size;
    }

    for (auto current = paths.begin(); current != paths.end(); ++current) {
        if (current->second == TransferEntryKind::Directory) {
            continue;
        }
        auto next = std::next(current);
        if (next != paths.end() &&
            next->first.compare(0, current->first.size(), current->first) == 0 &&
            next->first.size() > current->first.size() &&
            next->first[current->first.size()] == '/') {
            set_error(error, "manifest places an entry below a non-directory");
            return false;
        }
    }
    return true;
}

std::string TransferManifest::serialize() const
{
    std::string error;
    if (!validate(&error)) {
        throw std::invalid_argument(error);
    }

    std::string wire;
    wire.reserve(64 + entries_.size() * 128);
    wire.append(kMagic, sizeof(kMagic));
    wire.push_back(static_cast<char>(kWireVersion));
    wire.append(transfer_id_);
    append_u32(wire, static_cast<std::uint32_t>(entries_.size()));

    for (const auto& entry : entries_) {
        wire.push_back(static_cast<char>(entry.kind));
        append_u32(wire, static_cast<std::uint32_t>(entry.relative_path.size()));
        append_u64(wire, entry.size);
        wire.append(entry.relative_path);
        if (entry.kind != TransferEntryKind::Directory) {
            wire.append(entry.sha256);
        }
    }

    if (wire.size() > kMaxSerializedSize) {
        throw std::length_error("serialized transfer manifest is too large");
    }
    return wire;
}

bool TransferManifest::deserialize(const std::string& wire,
                                   TransferManifest& manifest,
                                   std::string* error)
{
    if (wire.size() > kMaxSerializedSize) {
        set_error(error, "serialized transfer manifest is too large");
        return false;
    }
    if (wire.size() < sizeof(kMagic) + 1 + kTransferIdLength + 4 ||
        !std::equal(std::begin(kMagic), std::end(kMagic), wire.begin())) {
        set_error(error, "invalid transfer manifest header");
        return false;
    }

    TransferManifest parsed;
    std::size_t offset = sizeof(kMagic);
    std::uint8_t version = 0;
    if (!take_u8(wire, offset, version) || version != kWireVersion ||
        !take_bytes(wire, offset, kTransferIdLength, parsed.transfer_id_)) {
        set_error(error, "unsupported or truncated transfer manifest");
        return false;
    }

    std::uint32_t count = 0;
    if (!take_u32(wire, offset, count) || count == 0 || count > kMaxEntries) {
        set_error(error, "invalid transfer manifest entry count");
        return false;
    }

    parsed.entries_.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint8_t raw_kind = 0;
        std::uint32_t path_length = 0;
        TransferEntry entry;
        if (!take_u8(wire, offset, raw_kind) ||
            !take_u32(wire, offset, path_length) ||
            !take_u64(wire, offset, entry.size) ||
            path_length == 0 || path_length > kMaxRelativePathLength ||
            !take_bytes(wire, offset, path_length, entry.relative_path)) {
            set_error(error, "truncated transfer manifest entry");
            return false;
        }
        entry.kind = static_cast<TransferEntryKind>(raw_kind);
        if (entry.kind != TransferEntryKind::Directory &&
            !take_bytes(wire, offset, kSha256Length, entry.sha256)) {
            set_error(error, "truncated transfer manifest digest");
            return false;
        }
        parsed.entries_.push_back(std::move(entry));
    }

    if (offset != wire.size()) {
        set_error(error, "transfer manifest contains trailing data");
        return false;
    }
    if (!parsed.validate(error)) {
        return false;
    }

    manifest = std::move(parsed);
    return true;
}

} // namespace inputleap
