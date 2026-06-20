#pragma once

#include <string>

namespace hob {
    enum class UiScreenMatchMode {
        match_width,
        match_height,
        expand,
        shrink,
    };

    UiScreenMatchMode to_screen_match_mode(const std::string& value);
} // namespace hob
