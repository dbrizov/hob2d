#include "lua_bind_helpers.h"

#include "engine/core/systems/renderer/renderer.h"

namespace hob {
    TextureRef resolve_texture(Renderer& renderer, const sol::object& value) {
        if (value.is<TextureRef>()) {
            return value.as<TextureRef>();
        }
        if (value.is<std::string>()) {
            return renderer.get_or_load_texture(value.as<std::string>());
        }
        return nullptr;
    }
} // namespace hob
