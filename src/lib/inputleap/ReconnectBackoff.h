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
#include <cstddef>
#include <iterator>

namespace inputleap {

class ReconnectBackoff {
public:
    double next_delay()
    {
        static constexpr double delays[] = {0.5, 1.0, 2.0, 5.0, 10.0};
        const auto index = (std::min)(attempt_, std::size(delays) - 1);
        ++attempt_;
        return delays[index];
    }

    void reset() { attempt_ = 0; }
    std::size_t attempt() const { return attempt_; }

private:
    std::size_t attempt_ = 0;
};

} // namespace inputleap
