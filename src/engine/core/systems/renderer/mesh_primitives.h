#pragma once

#include <optional>
#include <string_view>

#include "mesh.h"

namespace hob::mesh_primitives {
    constexpr std::string_view CUBE = "cube";
    constexpr std::string_view PLANE = "plane";
    constexpr std::string_view SPHERE = "sphere";
    constexpr std::string_view CAPSULE = "capsule";

    MeshData make_cube();
    MeshData make_plane();
    MeshData make_sphere(uint32_t rings = 16, uint32_t segments = 32);
    MeshData make_capsule(float radius = 0.5f, float height = 2.0f, uint32_t rings = 8, uint32_t segments = 24);

    std::optional<MeshData> make_by_name(std::string_view name);
} // namespace hob::mesh_primitives
