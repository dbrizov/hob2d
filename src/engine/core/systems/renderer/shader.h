#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "engine/math/constants.h"
#include "shader_reflection.h"

namespace hob {
    class Shader;
    using ShaderRef = std::shared_ptr<Shader>;

    constexpr uint32_t INVALID_SHADER_SLOT = MAX_UINT32;

    // Built-ins the engine fills into a shader's `Engine` cbuffer, matched by member name at build time.
    enum class EngineBuiltin {
        TexelSize,
        GameTime,
        RealTime,
        Count,
    };

    constexpr uint32_t ENGINE_BUILTIN_COUNT = static_cast<uint32_t>(EngineBuiltin::Count);
    constexpr uint32_t ENGINE_CBUFFER_MAX_BYTES = 256;

    // Slot of the per-draw sprite texture (sprite_tex at t0, space2).
    // Any other reflected texture binding is a material-provided texture.
    constexpr uint32_t SPRITE_TEXTURE_SLOT = 0;

    enum class BlendMode {
        Alpha,
        Additive,
        Premultiplied,
        Opaque,
    };

    enum class CullMode {
        None,
        Back,
        Front,
    };

    const char* blend_mode_to_string(BlendMode mode);
    bool blend_mode_from_string(std::string_view str, BlendMode& out);

    const char* cull_mode_to_string(CullMode mode);
    bool cull_mode_from_string(std::string_view str, CullMode& out);

    struct ShaderParam {
        ShaderParamType type = ShaderParamType::Unknown;
        uint32_t offset = 0;
        uint32_t size = 0;
    };

    struct ShaderTexture {
        std::string name;
        uint32_t slot = 0;
    };

    class Shader {
        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUGraphicsPipeline* m_pipeline = nullptr;
        std::string m_path;
        BlendMode m_blend_mode = BlendMode::Alpha;
        CullMode m_cull_mode = CullMode::None;

        uint32_t m_engine_slot = INVALID_SHADER_SLOT; // engine-filled cbuffer
        uint32_t m_engine_size = 0;
        std::array<int32_t, ENGINE_BUILTIN_COUNT> m_engine_offsets{}; // per-built-in byte offset, -1 if not declared

        uint32_t m_material_slot = INVALID_SHADER_SLOT; // user-facing "Material" cbuffer
        uint32_t m_material_size = 0;
        std::unordered_map<std::string, ShaderParam> m_params;
        std::vector<uint8_t> m_default_params;
        std::vector<ShaderTexture> m_textures;

    public:
        // clang-format off
        Shader(SDL_GPUDevice* device, SDL_GPUGraphicsPipeline* pipeline, std::string path, BlendMode blend, CullMode cull);
        ~Shader();
        // clang-format on

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        Shader(Shader&&) = delete;
        Shader& operator=(Shader&&) = delete;

        const std::string& get_path() const;
        BlendMode get_blend_mode() const;
        CullMode get_cull_mode() const;
        SDL_GPUGraphicsPipeline* get_pipeline() const;

        uint32_t get_engine_slot() const;
        uint32_t get_engine_size() const;
        int32_t get_engine_offset(EngineBuiltin builtin) const;
        void set_engine_layout(uint32_t slot, uint32_t size, const std::array<int32_t, ENGINE_BUILTIN_COUNT>& offsets);

        uint32_t get_material_slot() const;
        uint32_t get_material_size() const;
        void set_material_layout(uint32_t slot, uint32_t size, std::unordered_map<std::string, ShaderParam> params);

        const std::unordered_map<std::string, ShaderParam>& get_params() const;
        const std::vector<uint8_t>& get_default_params() const;
        void set_default_params(std::vector<uint8_t> defaults);
        bool set_default_param(const std::string& name, const float* values, uint32_t count);
        const ShaderParam* find_param(const std::string& name) const;

        const std::vector<ShaderTexture>& get_textures() const;
        void set_textures(std::vector<ShaderTexture> textures);
        const ShaderTexture* find_texture(const std::string& name) const;
    };
} // namespace hob
