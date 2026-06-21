#include "aspect_mode.h"

#include "engine/core/logging.h"

namespace hob {
    AspectMode to_aspect_mode(const std::string& value) {
        if (value == "keep_width") {
            return AspectMode::keep_width;
        }
        if (value == "keep_height") {
            return AspectMode::keep_height;
        }
        if (value == "expand") {
            return AspectMode::expand;
        }
        if (value == "shrink") {
            return AspectMode::shrink;
        }

        log::engine.error("Unknown aspect_mode '{}', defaulting to 'expand'", value);
        return AspectMode::expand;
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
