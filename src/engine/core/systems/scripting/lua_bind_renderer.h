#pragma once

#include <sol/sol.hpp>

#include "engine/core/systems/renderer/texture.h"

namespace hob {
    class Renderer;

    TextureRef resolve_texture(Renderer& renderer, const sol::object& value);
} // namespace hob
