/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/TransferControl.h"

#include "base/Log.h"
#include "common/DataDirectories.h"
#include "inputleap/TransferManifest.h"

#include <cstdio>
#include <iomanip>
#include <sstream>

namespace inputleap {

namespace {

std::string encode_field(const std::string& value)
{
    std::ostringstream encoded;
    encoded << std::hex << std::uppercase;
    for (const auto character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if ((byte >= 'a' && byte <= 'z') ||
            (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9') ||
            byte == '-' || byte == '_' || byte == '.' || byte == '/') {
            encoded << character;
        }
        else {
            encoded << '%' << std::setw(2) << std::setfill('0')
                    << static_cast<unsigned int>(byte);
        }
    }
    return encoded.str();
}

fs::path control_path(const std::string& transfer_id,
                      TransferControlAction action)
{
    return DataDirectories::profile() / "transfer-control" /
           fs::u8path(transfer_id + "." +
                      transfer_control_action_name(action));
}

} // namespace

const char* transfer_state_name(TransferState state)
{
    switch (state) {
    case TransferState::Pending: return "pending";
    case TransferState::Running: return "running";
    case TransferState::Paused: return "paused";
    case TransferState::Completed: return "completed";
    case TransferState::Cancelled: return "cancelled";
    case TransferState::Failed: return "failed";
    }
    return "failed";
}

const char* transfer_control_action_name(TransferControlAction action)
{
    switch (action) {
    case TransferControlAction::Cancel: return "cancel";
    case TransferControlAction::Pause: return "pause";
    case TransferControlAction::Resume: return "resume";
    case TransferControlAction::Retry: return "retry";
    }
    return "unknown";
}

void log_transfer_progress(const TransferProgressSnapshot& snapshot,
                           const char* direction, const std::string& peer)
{
    LOG_PRINT(
        "TRANSFER_PROGRESS id=%s direction=%s state=%s entries=%zu "
        "completed=%zu bytes=%llu total=%llu speed=%.0f path=%s peer=%s "
        "error=%s",
        snapshot.transfer_id.c_str(), direction,
        transfer_state_name(snapshot.state), snapshot.entry_count,
        snapshot.completed_entries,
        static_cast<unsigned long long>(snapshot.transferred_bytes),
        static_cast<unsigned long long>(snapshot.total_bytes),
        snapshot.bytes_per_second, encode_field(snapshot.current_path).c_str(),
        encode_field(peer).c_str(), encode_field(snapshot.error).c_str());
}

void log_transfer_result(const std::string& transfer_id,
                         const std::string& path)
{
    LOG_PRINT("TRANSFER_RESULT id=%s path=%s", transfer_id.c_str(),
              encode_field(path).c_str());
}

bool consume_transfer_control(const std::string& transfer_id,
                              TransferControlAction action)
{
    if (!TransferManifest::is_transfer_id(transfer_id)) {
        return false;
    }
    const auto path = control_path(transfer_id, action);
    std::error_code error;
    const bool exists = fs::exists(path, error);
    if (error || !exists) {
        return false;
    }
    fs::remove(path, error);
    if (error) {
        LOG_WARN("failed removing transfer control request \"%s\": %s",
                 path.u8string().c_str(), error.message().c_str());
    }
    return true;
}

} // namespace inputleap
