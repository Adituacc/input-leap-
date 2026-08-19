/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include <algorithm>
#include <cstdint>

namespace inputleap {

// Converts a desktop coordinate to the 0..65535 range used by Windows
// absolute mouse input. Values outside the desktop are clamped so signed
// coordinates from monitors left or above the primary display cannot wrap.
inline std::uint32_t normalize_absolute_cursor_coordinate(
    std::int32_t position, std::int32_t origin, std::int32_t size)
{
    if (size <= 1) {
        return 0;
    }

    constexpr std::int64_t kNormalizedMaximum = 65535;
    const auto first = static_cast<std::int64_t>(origin);
    const auto span = static_cast<std::int64_t>(size) - 1;
    const auto last = first + span;
    const auto clamped = (std::max)(first, (std::min)(
        static_cast<std::int64_t>(position), last));
    const auto offset = clamped - first;

    return static_cast<std::uint32_t>(
        (offset * kNormalizedMaximum + span / 2) / span);
}

} // namespace inputleap
