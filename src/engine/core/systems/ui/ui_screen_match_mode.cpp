#include "ui_screen_match_mode.h"

#include "engine/core/logging.h"

namespace hob {
    UiScreenMatchMode to_screen_match_mode(const std::string& value) {
        if (value == "match_width") {
            return UiScreenMatchMode::match_width;
        }
        if (value == "match_height") {
            return UiScreenMatchMode::match_height;
        }
        if (value == "expand") {
            return UiScreenMatchMode::expand;
        }
        if (value == "shrink") {
            return UiScreenMatchMode::shrink;
        }

        log::ui.error("UiSystem: unknown screen_match_mode '{}', defaulting to 'expand'", value);
        return UiScreenMatchMode::expand;
    }
} // namespace hob
