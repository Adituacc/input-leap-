/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "inputleap/mouse_types.h"

#include <set>

namespace inputleap {

class PressedMouseButtons {
public:
    void press(ButtonID button) { buttons_.insert(button); }
    void release(ButtonID button) { buttons_.erase(button); }
    const std::set<ButtonID>& buttons() const { return buttons_; }
    bool empty() const { return buttons_.empty(); }
    void clear() { buttons_.clear(); }

private:
    std::set<ButtonID> buttons_;
};

} // namespace inputleap
