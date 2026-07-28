/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/TransferReceiver.h"

#include "inputleap/TransferHash.h"

#include <fstream>
#include <set>
#include <stdexcept>

namespace inputleap {

namespace {

constexpr const char* kManifestFilename = "manifest.ilt2";
constexpr const char* kFilesDirectory = "files";

std::string first_component(const std::string& path)
{
    const auto separator = path.find('/');
    return separator == std::string::npos ? path : path.substr(0, separator);
}

void write_manifest_file(const fs::path& path, const std::string& wire)
{
    std::ofstream stream;
    open_utf8_path(stream, path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        throw std::runtime_error("failed to create transfer resume manifest");
    }
    stream.write(wire.data(), static_cast<std::streamsize>(wire.size()));
    if (!stream.good()) {
        throw std::runtime_error("failed to write transfer resume manifest");
    }
}

std::string read_manifest_file(const fs::path& path)
{
    std::ifstream stream;
    open_utf8_path(stream, path, std::ios::in | std::ios::binary);
    if (!stream.is_open()) {
        throw std::runtime_error("failed to open transfer resume manifest");
    }
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

} // namespace

TransferReceiver::TransferReceiver(TransferProgress* progress) :
    progress_(progress)
{
}

TransferReceiver::~TransferReceiver() = default;

void TransferReceiver::begin(const TransferManifest& manifest,
                             const fs::path& destination)
{
    if (active_) {
        throw std::logic_error("a transfer is already active");
    }
    std::string error;
    if (!manifest.validate(&error)) {
        throw std::invalid_argument(error);
    }

    fs::create_directories(destination);
    if (!fs::is_directory(destination)) {
        throw std::invalid_argument("transfer destination is not a directory");
    }

    destination_ = destination;
    partial_base_ = destination_ / ".inputleap-partials";
    if (fs::exists(partial_base_) && fs::is_symlink(partial_base_)) {
        throw std::runtime_error("transfer partial directory cannot be a symbolic link");
    }
    fs::create_directories(partial_base_);

    staging_root_ = partial_base_ / fs::u8path(manifest.transfer_id());
    const auto manifest_path = staging_root_ / kManifestFilename;
    const auto wire = manifest.serialize();

    if (fs::exists(staging_root_)) {
        if (fs::is_symlink(staging_root_) || !fs::is_directory(staging_root_) ||
            !fs::exists(manifest_path) || read_manifest_file(manifest_path) != wire) {
            throw std::runtime_error("existing transfer resume state is invalid");
        }
    }
    else {
        fs::create_directories(staging_root_ / kFilesDirectory);
        write_manifest_file(manifest_path, wire);
    }

    manifest_ = manifest;
    offsets_.assign(manifest_.entries().size(), 0);
    verified_.assign(manifest_.entries().size(), false);
    for (std::size_t index = 0; index < manifest_.entries().size(); ++index) {
        const auto& entry = manifest_.entries()[index];
        if (entry.kind == TransferEntryKind::Directory) {
            fs::create_directories(entry_staging_path(index));
            verified_[index] = true;
            continue;
        }
        const auto path = entry_staging_path(index);
        if (fs::exists(path)) {
            if (fs::is_symlink(path) || !fs::is_regular_file(path)) {
                throw std::runtime_error("invalid partial transfer entry");
            }
            offsets_[index] = static_cast<std::uint64_t>(fs::file_size(path));
            if (offsets_[index] > entry.size) {
                throw std::runtime_error("partial transfer entry exceeds manifest size");
            }
        }
        else if (entry.size == 0) {
            fs::create_directories(path.parent_path());
            std::ofstream stream;
            open_utf8_path(stream, path,
                           std::ios::out | std::ios::binary | std::ios::trunc);
            if (!stream.is_open()) {
                throw std::runtime_error("failed to create empty transfer entry");
            }
        }
    }

    active_ = true;
    if (progress_ != nullptr) {
        progress_->begin(manifest_);
        for (const auto offset : offsets_) {
            progress_->add_bytes(offset);
        }
    }
}

std::vector<std::uint64_t> TransferReceiver::resume_offsets() const
{
    if (!active_) {
        throw std::logic_error("no transfer is active");
    }
    return offsets_;
}

fs::path TransferReceiver::entry_staging_path(std::size_t entry_index) const
{
    return staging_root_ / kFilesDirectory /
           fs::u8path(manifest_.entries().at(entry_index).relative_path);
}

void TransferReceiver::write_chunk(std::size_t entry_index, std::uint64_t offset,
                                   const void* data, std::size_t size)
{
    if (!active_ || entry_index >= manifest_.entries().size()) {
        throw std::logic_error("invalid transfer entry");
    }
    if (size > kMaxChunkSize || (size != 0 && data == nullptr)) {
        throw std::invalid_argument("invalid transfer chunk");
    }
    const auto& entry = manifest_.entries()[entry_index];
    if (entry.kind == TransferEntryKind::Directory) {
        throw std::invalid_argument("cannot write data to a directory entry");
    }
    if (offset != offsets_[entry_index] ||
        size > entry.size - offsets_[entry_index]) {
        throw std::invalid_argument("transfer chunk offset or size is invalid");
    }
    if (progress_ != nullptr && progress_->should_cancel()) {
        throw std::runtime_error("transfer was cancelled");
    }

    const auto path = entry_staging_path(entry_index);
    fs::create_directories(path.parent_path());
    std::ofstream stream;
    open_utf8_path(stream, path, std::ios::out | std::ios::binary | std::ios::app);
    if (!stream.is_open()) {
        throw std::runtime_error("failed to open partial transfer entry");
    }
    stream.write(static_cast<const char*>(data),
                 static_cast<std::streamsize>(size));
    if (!stream.good()) {
        throw std::runtime_error("failed to write transfer chunk");
    }
    offsets_[entry_index] += size;
    if (progress_ != nullptr) {
        progress_->set_current_entry(entry_index);
        progress_->add_bytes(size);
    }
}

void TransferReceiver::finish_entry(std::size_t entry_index)
{
    if (!active_ || entry_index >= manifest_.entries().size()) {
        throw std::logic_error("invalid transfer entry");
    }
    const auto& entry = manifest_.entries()[entry_index];
    if (entry.kind != TransferEntryKind::Directory) {
        if (offsets_[entry_index] != entry.size) {
            throw std::runtime_error("transfer entry is incomplete");
        }
        if (sha256_file(entry_staging_path(entry_index)) != entry.sha256) {
            throw std::runtime_error("transfer entry failed SHA-256 verification");
        }
    }
    if (!verified_[entry_index] && progress_ != nullptr) {
        progress_->finish_entry();
    }
    verified_[entry_index] = true;
}

fs::path TransferReceiver::choose_destination(const std::string& name) const
{
    fs::path candidate = destination_ / fs::u8path(name);
    if (!fs::exists(candidate)) {
        return candidate;
    }

    const fs::path original = fs::u8path(name);
    const auto stem = original.stem().u8string();
    const auto extension = original.extension().u8string();
    for (unsigned suffix = 1; suffix < 10000; ++suffix) {
        candidate = destination_ / fs::u8path(
            stem + " (" + std::to_string(suffix) + ")" + extension);
        if (!fs::exists(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("could not choose a unique transfer destination");
}

std::vector<fs::path> TransferReceiver::complete()
{
    if (!active_) {
        throw std::logic_error("no transfer is active");
    }
    for (std::size_t index = 0; index < manifest_.entries().size(); ++index) {
        if (!verified_[index]) {
            finish_entry(index);
        }
    }

    std::set<std::string> roots;
    for (const auto& entry : manifest_.entries()) {
        roots.insert(first_component(entry.relative_path));
    }

    std::vector<fs::path> results;
    const auto files_root = staging_root_ / kFilesDirectory;
    for (const auto& root : roots) {
        const auto source = files_root / fs::u8path(root);
        const auto target = choose_destination(root);
        fs::rename(source, target);
        results.push_back(target);
    }

    fs::remove(staging_root_ / kManifestFilename);
    fs::remove(staging_root_ / kFilesDirectory);
    fs::remove(staging_root_);
    if (fs::is_empty(partial_base_)) {
        fs::remove(partial_base_);
    }
    if (progress_ != nullptr) {
        progress_->complete();
    }
    reset_state();
    return results;
}

void TransferReceiver::cancel()
{
    if (active_) {
        fs::remove_all(staging_root_);
        if (fs::exists(partial_base_) && fs::is_empty(partial_base_)) {
            fs::remove(partial_base_);
        }
        if (progress_ != nullptr) {
            progress_->cancel();
        }
        reset_state();
    }
}

std::vector<fs::path> TransferReceiver::handle_frame(
    const TransferFrame& frame, const fs::path& destination)
{
    std::string error;
    if (!frame.validate(&error)) {
        throw std::invalid_argument(error);
    }

    if (frame.type == TransferFrameType::Manifest) {
        TransferManifest manifest;
        if (!TransferManifest::deserialize(frame.payload, manifest, &error)) {
            throw std::invalid_argument(error);
        }
        begin(manifest, destination);
        return {};
    }

    if (!active_ || frame.transfer_id != manifest_.transfer_id()) {
        throw std::logic_error("transfer frame does not match an active transfer");
    }

    switch (frame.type) {
    case TransferFrameType::Chunk:
        write_chunk(frame.entry_index, frame.offset, frame.payload.data(),
                    frame.payload.size());
        break;
    case TransferFrameType::EntryComplete:
        finish_entry(frame.entry_index);
        break;
    case TransferFrameType::TransferComplete:
        return complete();
    case TransferFrameType::Cancel:
        cancel();
        break;
    case TransferFrameType::Manifest:
        break;
    case TransferFrameType::ResumeRequest:
    case TransferFrameType::ResumeState:
    case TransferFrameType::Error:
        throw std::invalid_argument("unsupported receiver transfer frame");
    }
    return {};
}

void TransferReceiver::reset_state()
{
    manifest_ = {};
    destination_.clear();
    partial_base_.clear();
    staging_root_.clear();
    offsets_.clear();
    verified_.clear();
    active_ = false;
}

} // namespace inputleap
