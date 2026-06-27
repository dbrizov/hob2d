#pragma once

#include <string>
#include <vector>

namespace hob {
    enum class ShaderParamType {
        Unknown,
        Bool,
        Float,
        Float2,
        Float3,
        Float4,
        Float3x3,
        Float4x4,
    };

    const char* to_string(ShaderParamType type);

    // Number of scalar components (Float->1, Float2->2, Float4x4->16, Unknown->0).
    uint32_t component_count(ShaderParamType type);

    struct ShaderUniformMember {
        std::string name;
        ShaderParamType type = ShaderParamType::Unknown;
        uint32_t offset = 0;
        uint32_t size = 0;
        uint32_t array_count = 0; // 0 = not an array, else element count
    };

    struct ShaderUniformBlock {
        std::string name; // resource/instance name (e.g. "Engine", "Material")
        std::string type_name; // type name as reported by the reflector (for diagnostics)
        uint32_t set = 0; // descriptor set (HLSL spaceN)
        uint32_t binding = 0; // binding (HLSL bN)
        uint32_t size = 0; // total block size in bytes
        std::vector<ShaderUniformMember> members;
    };

    struct ShaderTextureBinding {
        std::string name;
        uint32_t set = 0; // HLSL spaceN
        uint32_t binding = 0; // HLSL tN
    };

    struct ShaderVertexInput {
        std::string name;
        ShaderParamType type = ShaderParamType::Unknown;
        uint32_t location = 0;
    };

    struct ShaderReflection {
        std::vector<ShaderUniformBlock> uniform_blocks;
        std::vector<ShaderTextureBinding> textures;
        std::vector<ShaderVertexInput> vertex_inputs;
    };

    bool reflect_spirv(const void* spirv, size_t spirv_size, ShaderReflection& out);
} // namespace hob
