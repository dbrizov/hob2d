#pragma once

#include <string>

#include "engine/math/vector2.h"

namespace hob {
    enum class ScreenMatchMode {
        match_width,
        match_height,
        expand,
        shrink,
    };

    ScreenMatchMode to_screen_match_mode(const std::string& value);

    Vector2 compute_logical_size(int window_width,
                                 int window_height,
                                 const Vector2& reference_size,
                                 ScreenMatchMode mode);
} // namespace hob
