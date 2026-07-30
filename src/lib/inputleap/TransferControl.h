/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "inputleap/TransferProgress.h"

#include <string>

namespace inputleap {

enum class TransferControlAction {
    Cancel,
    Pause,
    Resume,
    Retry
};

const char* transfer_state_name(TransferState state);
const char* transfer_control_action_name(TransferControlAction action);

void log_transfer_progress(const TransferProgressSnapshot& snapshot,
                           const char* direction,
                           const std::string& peer = {});
void log_transfer_result(const std::string& transfer_id,
                         const std::string& path);

bool consume_transfer_control(const std::string& transfer_id,
                              TransferControlAction action);

} // namespace inputleap
