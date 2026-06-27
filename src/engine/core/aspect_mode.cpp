#include "aspect_mode.h"

#include "engine/core/logging.h"

namespace hob {
    bool aspect_mode_from_string(std::string_view value, AspectMode& out) {
        if (value == "keep_width") {
            out = AspectMode::keep_width;
        }
        else if (value == "keep_height") {
            out = AspectMode::keep_height;
        }
        else if (value == "expand") {
            out = AspectMode::expand;
        }
        else if (value == "shrink") {
            out = AspectMode::shrink;
        }
        else {
            return false;
        }
        return true;
    }

    Vector2 compute_logical_size(int window_width, int window_height, const Vector2& reference_size, AspectMode mode) {
        if (window_width <= 0 || window_height <= 0) {
            return reference_size;
        }

        const float window_aspect = static_cast<float>(window_width) / static_cast<float>(window_height);
        const float reference_aspect = reference_size.x / reference_size.y;

        if (mode == AspectMode::expand) {
            mode = (window_aspect > reference_aspect) ? AspectMode::keep_height : AspectMode::keep_width;
        }
        else if (mode == AspectMode::shrink) {
            mode = (window_aspect > reference_aspect) ? AspectMode::keep_width : AspectMode::keep_height;
        }

        if (mode == AspectMode::keep_width) {
            return Vector2(reference_size.x, reference_size.x / window_aspect);
        }

        return Vector2(reference_size.y * window_aspect, reference_size.y);
    }
} // namespace hob
