#pragma once

#include <limits>

#include "engine/math/matrix4x4.h"
#include "material.h"
#include "mesh.h"

namespace hob {
    using MeshDrawId = int64_t;
    constexpr MeshDrawId INVALID_MESH_DRAW_ID = -1;

    using MeshDrawIndex = uint32_t;
    constexpr MeshDrawIndex INVALID_MESH_DRAW_INDEX = std::numeric_limits<MeshDrawIndex>::max();

    struct MeshDrawData {
        const Mesh* mesh = nullptr;
        const Material* material = nullptr;
        Matrix4x4 world_matrix = Matrix4x4::identity();

        const Shader* get_shader() const {
            return material != nullptr ? material->get_shader() : nullptr;
        }
    };
} // namespace hob
