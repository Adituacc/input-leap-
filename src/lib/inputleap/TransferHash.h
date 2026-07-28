/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "io/filesystem.h"

#include <cstddef>
#include <string>

namespace inputleap {

std::string sha256_bytes(const void* data, std::size_t size);
std::string sha256_file(const fs::path& path);
std::string create_transfer_id();

} // namespace inputleap
