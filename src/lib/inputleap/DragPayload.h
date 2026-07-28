/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include <string>

namespace inputleap {

bool is_supported_drag_url(const std::string& value);
std::string make_windows_internet_shortcut(const std::string& url);
std::string sanitize_drag_filename(const std::string& value,
                                   const std::string& fallback = "InputLeap Drop");

} // namespace inputleap
