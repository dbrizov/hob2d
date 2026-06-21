#include "screen_match_mode.h"

#include "engine/core/logging.h"

namespace hob {
    ScreenMatchMode to_screen_match_mode(const std::string& value) {
        if (value == "match_width") {
            return ScreenMatchMode::match_width;
        }
        if (value == "match_height") {
            return ScreenMatchMode::match_height;
        }
        if (value == "expand") {
            return ScreenMatchMode::expand;
        }
        if (value == "shrink") {
            return ScreenMatchMode::shrink;
        }

        log::engine.error("Unknown screen_match_mode '{}', defaulting to 'expand'", value);
        return ScreenMatchMode::expand;
    }

    Vector2 compute_logical_size(int window_width,
                                 int window_height,
                                 const Vector2& reference_size,
                                 ScreenMatchMode mode) {
        if (window_width <= 0 || window_height <= 0) {
            return reference_size;
        }

        const float window_aspect = static_cast<float>(window_width) / static_cast<float>(window_height);
        const float reference_aspect = reference_size.x / reference_size.y;

        if (mode == ScreenMatchMode::expand) {
            mode = (window_aspect > reference_aspect) ? ScreenMatchMode::match_height : ScreenMatchMode::match_width;
        }
        else if (mode == ScreenMatchMode::shrink) {
            mode = (window_aspect > reference_aspect) ? ScreenMatchMode::match_width : ScreenMatchMode::match_height;
        }

        if (mode == ScreenMatchMode::match_width) {
            return Vector2(reference_size.x, reference_size.x / window_aspect);
        }

        return Vector2(reference_size.y * window_aspect, reference_size.y);
    }
} // namespace hob
