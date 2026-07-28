/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "inputleap/TransferCatalog.h"
#include "inputleap/TransferFrame.h"
#include "inputleap/TransferProgress.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace inputleap {

class TransferSender {
public:
    using FrameSink = std::function<void(const TransferFrame&)>;

    explicit TransferSender(TransferProgress* progress = nullptr);

    void send(const TransferPlan& plan, const FrameSink& sink,
              const std::vector<std::uint64_t>& resume_offsets = {});

private:
    void wait_if_paused() const;

    TransferProgress* progress_;
};

} // namespace inputleap
