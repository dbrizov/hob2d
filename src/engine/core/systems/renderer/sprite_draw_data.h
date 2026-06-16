#pragma once

#include "engine/math/vector2.h"
#include "material.h"
#include "texture.h"

namespace hob {
    using SpriteDrawHandle = int64_t;
    constexpr SpriteDrawHandle INVALID_SPRITE_DRAW_HANDLE = -1;

    struct SpriteDrawData {
        TextureRef texture;
        Material material;
        Vector2 world_pos; // pivot point, world meters
        Vector2 size; // quad size, world meters (texture/ppm * scale)
        Vector2 pivot = Vector2(0.5f, 0.5f); // pivot as a 0..1 fraction of the quad
        float rotation = 0.0f; // world rotation, radians (y-up CCW)
        int z_index = 0;
    };
} // namespace hob
