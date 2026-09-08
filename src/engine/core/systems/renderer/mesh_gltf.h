#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "mesh.h"

namespace hob::mesh_gltf {
    std::optional<MeshData> load(const std::filesystem::path& full_path, std::string& error);
} // namespace hob::mesh_gltf
