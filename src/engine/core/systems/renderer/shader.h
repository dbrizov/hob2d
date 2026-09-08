#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "engine/core/asset.h"
#include "engine/core/string_hash.h"
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
        LightDirection,
        LightColor,
        AmbientColor,
        CameraPosition,
        SkyColor,
        GroundColor,
        LightViewProj,
        ShadowParams,
        ScreenParams,
        Count,
    };

    constexpr uint32_t ENGINE_BUILTIN_COUNT = static_cast<uint32_t>(EngineBuiltin::Count);
    constexpr uint32_t ENGINE_CBUFFER_MAX_BYTES = 256;

    // Slot of the per-draw sprite texture (sprite_tex at t0, space2).
    // Any other reflected texture binding is a material-provided texture.
    constexpr uint32_t SPRITE_TEXTURE_SLOT = 0;
    constexpr uint32_t MAX_MATERIAL_TEXTURE_SLOTS = 16;

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

    // Which engine-owned vertex stream a shader consumes, inferred from its vertex inputs at build time.
    enum class VertexLayout {
        Sprite,
        Mesh,
    };

    const char* vertex_layout_to_string(VertexLayout layout);

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

    using ShaderParamMap = std::unordered_map<std::string, ShaderParam, StringHash, std::equal_to<>>;

    class Shader : public Asset {
        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUGraphicsPipeline* m_pipeline = nullptr;
        std::string m_path;
        BlendMode m_blend_mode = BlendMode::Alpha;
        CullMode m_cull_mode = CullMode::None;
        VertexLayout m_vertex_layout = VertexLayout::Sprite;

        uint32_t m_engine_slot = INVALID_SHADER_SLOT; // engine-filled cbuffer
        uint32_t m_engine_size = 0;
        std::array<int32_t, ENGINE_BUILTIN_COUNT> m_engine_offsets{}; // per-built-in byte offset, -1 if not declared

        uint32_t m_shadow_map_slot = INVALID_SHADER_SLOT; // engine-bound sun shadow map, never a material slot
        uint32_t m_ssao_slot = INVALID_SHADER_SLOT; // engine-bound ambient occlusion, never a material slot
        uint32_t m_material_slot = INVALID_SHADER_SLOT; // user-facing "Material" cbuffer
        uint32_t m_material_size = 0;
        ShaderParamMap m_params;
        std::vector<uint8_t> m_default_params;
        std::vector<ShaderTexture> m_textures;

    public:
        // clang-format off
        Shader(SDL_GPUDevice* device, SDL_GPUGraphicsPipeline* pipeline, std::string relative_path, BlendMode blend, CullMode cull, VertexLayout vertex_layout);
        ~Shader() override;
        // clang-format on

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        Shader(Shader&&) = delete;
        Shader& operator=(Shader&&) = delete;

        const std::string& get_path() const;
        BlendMode get_blend_mode() const;
        CullMode get_cull_mode() const;
        VertexLayout get_vertex_layout() const;
        SDL_GPUGraphicsPipeline* get_pipeline() const;
        void replace_pipeline(SDL_GPUGraphicsPipeline* pipeline);
        SDL_GPUGraphicsPipeline* detach_pipeline();

        uint32_t get_engine_slot() const;
        uint32_t get_engine_size() const;
        int32_t get_engine_offset(EngineBuiltin builtin) const;
        void set_engine_layout(uint32_t slot, uint32_t size, const std::array<int32_t, ENGINE_BUILTIN_COUNT>& offsets);

        uint32_t get_shadow_map_slot() const;
        void set_shadow_map_slot(uint32_t slot);

        uint32_t get_ssao_slot() const;
        void set_ssao_slot(uint32_t slot);

        uint32_t get_material_slot() const;
        uint32_t get_material_size() const;
        void set_material_layout(uint32_t slot, uint32_t size, ShaderParamMap params);

        const ShaderParamMap& get_params() const;
        const std::vector<uint8_t>& get_default_params() const;
        void set_default_params(std::vector<uint8_t> defaults);
        bool set_default_param(std::string_view name, const float* values, uint32_t count);
        const ShaderParam* find_param(std::string_view name) const;

        const std::vector<ShaderTexture>& get_textures() const;
        void set_textures(std::vector<ShaderTexture> textures);
        const ShaderTexture* find_texture(std::string_view name) const;
    };
} // namespace hob
