/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 */

#pragma once

#include <cstdint>
#include <utility>

namespace inputleap {

inline std::pair<std::int32_t, std::int32_t>
apply_scroll_direction(std::int32_t x_delta, std::int32_t y_delta, bool inverted)
{
    if (inverted) {
        return {-x_delta, -y_delta};
    }
    return {x_delta, y_delta};
}

} // namespace inputleap
