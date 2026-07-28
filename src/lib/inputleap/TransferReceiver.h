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
#include "inputleap/TransferProgress.h"
#include "inputleap/TransferFrame.h"
#include "io/filesystem.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace inputleap {

class TransferReceiver {
public:
    static constexpr std::size_t kMaxChunkSize = 1024u * 1024u;

    explicit TransferReceiver(TransferProgress* progress = nullptr);
    ~TransferReceiver();

    void begin(const TransferManifest& manifest, const fs::path& destination);
    std::vector<std::uint64_t> resume_offsets() const;
    void write_chunk(std::size_t entry_index, std::uint64_t offset,
                     const void* data, std::size_t size);
    void finish_entry(std::size_t entry_index);
    std::vector<fs::path> complete();
    void cancel();
    std::vector<fs::path> handle_frame(const TransferFrame& frame,
                                       const fs::path& destination);

    bool active() const { return active_; }

private:
    fs::path entry_staging_path(std::size_t entry_index) const;
    fs::path choose_destination(const std::string& name) const;
    void reset_state();

    TransferManifest manifest_;
    fs::path destination_;
    fs::path partial_base_;
    fs::path staging_root_;
    std::vector<std::uint64_t> offsets_;
    std::vector<bool> verified_;
    TransferProgress* progress_;
    bool active_ = false;
};

} // namespace inputleap
