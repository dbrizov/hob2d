#pragma once

#include <string>

#include "engine/math/vector2.h"

namespace hob {
    enum class AspectMode {
        keep_width,
        keep_height,
        expand,
        shrink,
    };

    AspectMode to_aspect_mode(const std::string& value);

    Vector2 compute_logical_size(int window_width, int window_height, const Vector2& reference_size, AspectMode mode);
} // namespace hob
