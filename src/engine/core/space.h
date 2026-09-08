#pragma once

#include <optional>
#include <string_view>

#include "engine/core/systems/scripting/lua_schema_keys.h"

namespace hob {
    enum class Space : uint8_t {
        Space2D,
        Space3D,
    };

    inline const char* space_to_key(Space space) {
        return space == Space::Space3D ? space_key::SPACE_3D : space_key::SPACE_2D;
    }

    inline std::optional<Space> space_from_key(std::string_view key) {
        if (key == space_key::SPACE_2D) {
            return Space::Space2D;
        }

        if (key == space_key::SPACE_3D) {
            return Space::Space3D;
        }

        return std::nullopt;
    }
} // namespace hob
