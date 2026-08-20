/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "inputleap/protocol_types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace inputleap {

struct CursorRect {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
};

inline std::int32_t clamp_cursor_coordinate(
    std::int32_t value, std::int32_t origin, std::int32_t size)
{
    if (size <= 1) {
        return origin;
    }
    return (std::max)(origin, (std::min)(value, origin + size - 1));
}

inline double cursor_edge_fraction(
    std::int32_t value, std::int32_t origin, std::int32_t size)
{
    if (size <= 1) {
        return 0.5;
    }
    const auto clamped = clamp_cursor_coordinate(value, origin, size);
    return (static_cast<double>(clamped - origin) + 0.5) /
           static_cast<double>(size);
}

inline std::int32_t cursor_coordinate_from_fraction(
    double fraction, std::int32_t origin, std::int32_t size)
{
    if (size <= 1) {
        return origin;
    }
    const auto bounded = (std::max)(0.0, (std::min)(fraction, 1.0));
    const auto offset = static_cast<std::int32_t>(
        std::floor(bounded * static_cast<double>(size)));
    return clamp_cursor_coordinate(origin + offset, origin, size);
}

inline void inset_cursor_on_entry(
    const CursorRect& rect, EDirection direction, std::int32_t inset,
    std::int32_t& x, std::int32_t& y)
{
    x = clamp_cursor_coordinate(x, rect.x, rect.width);
    y = clamp_cursor_coordinate(y, rect.y, rect.height);

    if (inset <= 0 || rect.width <= 0 || rect.height <= 0) {
        return;
    }

    const auto xInset = (std::min)(inset, (rect.width - 1) / 2);
    const auto yInset = (std::min)(inset, (rect.height - 1) / 2);
    switch (direction) {
    case kLeft:
        x = (std::min)(x, rect.x + rect.width - 1 - xInset);
        break;
    case kRight:
        x = (std::max)(x, rect.x + xInset);
        break;
    case kTop:
        y = (std::min)(y, rect.y + rect.height - 1 - yInset);
        break;
    case kBottom:
        y = (std::max)(y, rect.y + yInset);
        break;
    default:
        break;
    }
}

inline bool motion_reverses_handoff(
    EDirection direction, std::int32_t dx, std::int32_t dy)
{
    switch (direction) {
    case kLeft: return dx > 0;
    case kRight: return dx < 0;
    case kTop: return dy > 0;
    case kBottom: return dy < 0;
    default: return false;
    }
}

inline bool motion_continues_handoff(
    EDirection direction, std::int32_t dx, std::int32_t dy)
{
    switch (direction) {
    case kLeft: return dx < 0;
    case kRight: return dx > 0;
    case kTop: return dy < 0;
    case kBottom: return dy > 0;
    default: return false;
    }
}

class CursorHandoffGuard {
public:
    static constexpr double kGuardSeconds = 0.12;

    void begin(EDirection direction)
    {
        direction_ = direction;
        active_ = direction != kNoDirection;
    }

    void cancel()
    {
        active_ = false;
        direction_ = kNoDirection;
    }

    bool should_suppress(
        std::int32_t dx, std::int32_t dy, double elapsed_seconds)
    {
        if (!active_) {
            return false;
        }
        if (elapsed_seconds > kGuardSeconds) {
            cancel();
            return false;
        }
        if (motion_continues_handoff(direction_, dx, dy)) {
            cancel();
            return false;
        }
        return motion_reverses_handoff(direction_, dx, dy);
    }

    bool active() const { return active_; }

private:
    EDirection direction_ = kNoDirection;
    bool active_ = false;
};

} // namespace inputleap
