/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace inputleap {

class MSWindowsDragSource {
public:
    static bool drag_files(const std::vector<std::string>& paths);
    static bool drag_files(
        const std::vector<std::string>& paths,
        const std::function<bool()>& left_button_down);
};

} // namespace inputleap
