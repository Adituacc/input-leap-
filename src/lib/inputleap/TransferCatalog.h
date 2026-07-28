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
#include "io/filesystem.h"

#include <vector>

namespace inputleap {

struct TransferPlan {
    TransferManifest manifest;
    // Aligned with manifest.entries(). Directory entries contain an empty path.
    std::vector<fs::path> sources;
};

class TransferCatalog {
public:
    static TransferManifest from_paths(const std::vector<fs::path>& paths);
    static TransferPlan plan_from_paths(const std::vector<fs::path>& paths);
};

} // namespace inputleap
