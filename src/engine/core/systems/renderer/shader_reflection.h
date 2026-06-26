#pragma once

#include <string>
#include <vector>

namespace hob {
    enum class ShaderParamType {
        Unknown,
        Float,
        Float2,
        Float3,
        Float4,
        Int,
        Int2,
        Int3,
        Int4,
        UInt,
        UInt2,
        UInt3,
        UInt4,
        Bool,
        Float3x3,
        Float4x4,
    };

    const char* to_string(ShaderParamType type);

    // Number of scalar components (Float->1, Float2->2, Float4x4->16, Unknown->0).
    uint32_t component_count(ShaderParamType type);

    // One member of a uniform (HLSL `cbuffer`) block, e.g. `float4 tint;`.
    struct ShaderUniformMember {
        ShaderParamType type = ShaderParamType::Unknown;
        std::string name; // member name as declared in HLSL
        uint32_t offset = 0; // byte offset within the block
        uint32_t size = 0; // size in bytes (excluding trailing padding)
        uint32_t array_count = 0; // 0 = not an array, else element count
    };

    // A uniform buffer (HLSL `cbuffer`) block.
    struct ShaderUniformBlock {
        std::string name; // resource/instance name (e.g. "Engine", "Material")
        std::string type_name; // type name as reported by the reflector (for diagnostics)
        uint32_t set = 0; // descriptor set (HLSL spaceN)
        uint32_t binding = 0; // binding (HLSL bN)
        uint32_t size = 0; // total block size in bytes
        std::vector<ShaderUniformMember> members;
    };

    // A texture / sampled-image binding (HLSL `Texture2D`).
    struct ShaderTextureBinding {
        std::string name;
        uint32_t set = 0; // HLSL spaceN
        uint32_t binding = 0; // HLSL tN
    };

    // A vertex shader input variable.
    struct ShaderVertexInput {
        ShaderParamType type = ShaderParamType::Unknown;
        std::string name;
        uint32_t location = 0;
    };

    // Reflection result for a single shader stage.
    struct ShaderReflection {
        std::vector<ShaderUniformBlock> uniform_blocks;
        std::vector<ShaderTextureBinding> textures;
        std::vector<ShaderVertexInput> vertex_inputs;
    };

    // Reflects a SPIR-V bytecode blob. Returns false on failure (out is left empty).
    bool reflect_spirv(const void* spirv, size_t spirv_size, ShaderReflection& out);
} // namespace hob
