#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include "engine/core/logging.h"
#include "engine/core/path_utils.h"
#include "engine/core/systems/window.h"
#include "mesh.h"
#include "renderer.h"
#include "shader_reflection.h"

namespace hob {
    namespace {
        std::string read_text_file(const std::filesystem::path& full_path) {
            std::ifstream f(full_path, std::ios::binary | std::ios::ate);
            if (!f) {
                return {};
            }

            const std::streamsize size = f.tellg();
            f.seekg(0);
            std::string contents(static_cast<size_t>(size), '\0');
            f.read(contents.data(), size);
            return contents;
        }

        void log_shader_reflection(const std::string& label, const ShaderReflection& refl) {
            for (const ShaderUniformBlock& block : refl.uniform_blocks) {
                log::renderer.info("[reflect] {} cbuffer '{}' (type '{}') set={} binding={} size={}",
                                   label,
                                   block.name,
                                   block.type_name,
                                   block.set,
                                   block.binding,
                                   block.size);
                for (const ShaderUniformMember& m : block.members) {
                    log::renderer.info(
                        "[reflect]     {}: {} offset={} size={}", m.name, to_string(m.type), m.offset, m.size);
                }
            }
            for (const ShaderTextureBinding& tex : refl.textures) {
                log::renderer.info(
                    "[reflect] {} texture '{}' set={} binding={}", label, tex.name, tex.set, tex.binding);
            }
            for (const ShaderVertexInput& vi : refl.vertex_inputs) {
                log::renderer.info(
                    "[reflect] {} vertex_input '{}' {} location={}", label, vi.name, to_string(vi.type), vi.location);
            }
        }

        struct EngineBuiltinInfo {
            const char* name;
            ShaderParamType type;
        };

        // Indexed by EngineBuiltin; matched against a shader's reflected `Engine` members by name.
        constexpr EngineBuiltinInfo ENGINE_BUILTINS[] = {
            {"texel_size", ShaderParamType::Float2},
            {"game_time", ShaderParamType::Float},
            {"real_time", ShaderParamType::Float},
            {"light_direction", ShaderParamType::Float3},
            {"light_color", ShaderParamType::Float3},
            {"ambient_color", ShaderParamType::Float3},
            {"camera_position", ShaderParamType::Float3},
            {"sky_color", ShaderParamType::Float3},
            {"ground_color", ShaderParamType::Float3},
            {"light_view_proj", ShaderParamType::Float4x4},
            {"shadow_params", ShaderParamType::Float4},
            {"screen_params", ShaderParamType::Float4},
        };
        static_assert(std::size(ENGINE_BUILTINS) == ENGINE_BUILTIN_COUNT);

        void resolve_engine_layout(const std::string& relative_path, const ShaderUniformBlock& block, Shader& shader) {
            if (block.size > ENGINE_CBUFFER_MAX_BYTES) {
                log::renderer.error("Shader '{}' Engine cbuffer is {} bytes; max supported is {} (left unfilled)",
                                    relative_path,
                                    block.size,
                                    ENGINE_CBUFFER_MAX_BYTES);
                return;
            }

            std::array<int32_t, ENGINE_BUILTIN_COUNT> offsets{};
            offsets.fill(-1);

            for (const ShaderUniformMember& member : block.members) {
                int32_t index = -1;
                for (int32_t i = 0; i < ENGINE_BUILTIN_COUNT; ++i) {
                    if (member.name == ENGINE_BUILTINS[i].name) {
                        index = i;
                        break;
                    }
                }

                if (index < 0) {
                    log::renderer.error("Shader '{}' Engine member '{}' is not a known engine built-in (stays zero)",
                                        relative_path,
                                        member.name);
                    continue;
                }

                if (member.type != ENGINE_BUILTINS[index].type) {
                    log::renderer.error("Shader '{}' Engine built-in '{}' is {}; the engine provides {}",
                                        relative_path,
                                        member.name,
                                        to_string(member.type),
                                        to_string(ENGINE_BUILTINS[index].type));
                    continue;
                }

                offsets[index] = static_cast<int32_t>(member.offset);
            }

            // Round up to the 16-byte cbuffer footprint; reflection may report the unpadded size.
            const uint32_t size = (block.size + 15u) & ~15u;
            shader.set_engine_layout(block.binding, size, offsets);
        }

        void resolve_material_layout(const ShaderUniformBlock& block, Shader& shader) {
            ShaderParamMap params;
            params.reserve(block.members.size());
            for (const ShaderUniformMember& member : block.members) {
                params.emplace(member.name, ShaderParam{member.type, member.offset, member.size});
            }

            // Round up to the 16-byte cbuffer footprint; reflection may report the unpadded size.
            const uint32_t size = (block.size + 15u) & ~15u;
            shader.set_material_layout(block.binding, size, std::move(params));
        }

        bool block_is_named(const ShaderUniformBlock& block, std::string_view name) {
            if (block.name == name || block.type_name == name) {
                return true;
            }
            const size_t dot = block.type_name.find_last_of('.');
            return dot != std::string::npos && std::string_view(block.type_name).substr(dot + 1) == name;
        }

        void apply_blend_state(SDL_GPUColorTargetBlendState& blend, BlendMode mode) {
            if (mode == BlendMode::Opaque) {
                blend.enable_blend = false;
                blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
                blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
                blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
                blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
                return;
            }

            blend.enable_blend = true;
            blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
            blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;

            switch (mode) {
                case BlendMode::Additive:
                    blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
                    blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                    blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                    break;
                case BlendMode::Premultiplied:
                    blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                    blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
                    blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
                    break;
                case BlendMode::Alpha:
                default:
                    blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
                    blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
                    blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
                    break;
            }
        }

        // Render state is baked into the pipeline, so each (path, blend, cull) combo is a distinct shader.
        std::string shader_cache_key(const std::string& relative_path, BlendMode blend, CullMode cull) {
            return relative_path + '|' + std::to_string(static_cast<int32_t>(blend)) + '|' +
                   std::to_string(static_cast<int32_t>(cull));
        }

        SDL_GPUCullMode to_sdl_cull(CullMode mode) {
            switch (mode) {
                case CullMode::Back:
                    return SDL_GPU_CULLMODE_BACK;
                case CullMode::Front:
                    return SDL_GPU_CULLMODE_FRONT;
                case CullMode::None:
                default:
                    return SDL_GPU_CULLMODE_NONE;
            }
        }

        SDL_GPUVertexElementFormat to_sdl_vertex_format(ShaderParamType type) {
            switch (type) {
                case ShaderParamType::Float:
                    return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
                case ShaderParamType::Float2:
                    return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
                case ShaderParamType::Float3:
                    return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
                case ShaderParamType::Float4:
                    return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
                default:
                    return SDL_GPU_VERTEXELEMENTFORMAT_INVALID;
            }
        }

        struct VertexSlot {
            ShaderParamType type;
            uint32_t location;
            uint32_t offset;
        };

        constexpr VertexSlot SPRITE_VERTEX_LAYOUT[] = {
            {ShaderParamType::Float2, 0, 0},
            {ShaderParamType::Float2, 1, 2 * sizeof(float)},
        };
        constexpr uint32_t SPRITE_VERTEX_STRIDE = 4 * sizeof(float);

        constexpr VertexSlot MESH_VERTEX_LAYOUT[] = {
            {ShaderParamType::Float3, 0, 0},
            {ShaderParamType::Float3, 1, 3 * sizeof(float)},
            {ShaderParamType::Float2, 2, 6 * sizeof(float)},
            {ShaderParamType::Float4, 3, 8 * sizeof(float)},
        };
        constexpr uint32_t MESH_VERTEX_STRIDE = 12 * sizeof(float);
        static_assert(MESH_VERTEX_STRIDE == sizeof(MeshVertex));

        // DXC prunes vertex inputs the shader never reads, so a mesh shader declares any subset of
        // the stream and is matched by location rather than by the full attribute list.
        bool match_mesh_vertex_layout(const std::vector<ShaderVertexInput>& inputs,
                                      std::vector<SDL_GPUVertexAttribute>& out_attrs) {
            if (inputs.empty() || inputs.size() > std::size(MESH_VERTEX_LAYOUT)) {
                return false;
            }

            std::vector<SDL_GPUVertexAttribute> attrs;
            bool has_position = false;
            for (const ShaderVertexInput& in : inputs) {
                const VertexSlot* slot = nullptr;
                for (const VertexSlot& candidate : MESH_VERTEX_LAYOUT) {
                    if (candidate.location == in.location) {
                        slot = &candidate;
                        break;
                    }
                }

                if (slot == nullptr || slot->type != in.type) {
                    return false;
                }

                has_position = has_position || in.location == 0;

                SDL_GPUVertexAttribute attr{};
                attr.location = in.location;
                attr.buffer_slot = 0;
                attr.format = to_sdl_vertex_format(in.type);
                attr.offset = slot->offset;
                attrs.push_back(attr);
            }

            if (!has_position) {
                return false;
            }

            out_attrs = std::move(attrs);
            return true;
        }

        bool match_vertex_layout(const std::vector<ShaderVertexInput>& inputs,
                                 const VertexSlot* slots,
                                 size_t slot_count,
                                 std::vector<SDL_GPUVertexAttribute>& out_attrs) {
            if (inputs.size() != slot_count) {
                return false;
            }

            std::vector<SDL_GPUVertexAttribute> attrs;
            for (size_t i = 0; i < slot_count; ++i) {
                const VertexSlot& slot = slots[i];
                const ShaderVertexInput* match = nullptr;
                for (const ShaderVertexInput& in : inputs) {
                    if (in.location == slot.location) {
                        match = &in;
                        break;
                    }
                }
                if (!match || match->type != slot.type) {
                    return false;
                }

                SDL_GPUVertexAttribute attr{};
                attr.location = match->location;
                attr.buffer_slot = 0;
                attr.format = to_sdl_vertex_format(match->type);
                attr.offset = slot.offset;
                attrs.push_back(attr);
            }

            out_attrs = std::move(attrs);
            return true;
        }

        bool infer_vertex_layout(const std::string& relative_path,
                                 const std::vector<ShaderVertexInput>& inputs,
                                 std::vector<SDL_GPUVertexAttribute>& out_attrs,
                                 VertexLayout& out_layout,
                                 uint32_t& out_stride) {
            if (match_vertex_layout(inputs, SPRITE_VERTEX_LAYOUT, std::size(SPRITE_VERTEX_LAYOUT), out_attrs)) {
                out_layout = VertexLayout::Sprite;
                out_stride = SPRITE_VERTEX_STRIDE;
                return true;
            }

            if (match_mesh_vertex_layout(inputs, out_attrs)) {
                out_layout = VertexLayout::Mesh;
                out_stride = MESH_VERTEX_STRIDE;
                return true;
            }

            log::renderer.error("Shader '{}' has {} vertex input(s) matching neither the sprite quad (pos float2 @0, "
                                "uv float2 @1) nor the mesh stream (pos float3 @0, normal float3 @1, uv float2 @2, "
                                "optional tangent float4 @3)",
                                relative_path,
                                inputs.size());
            return false;
        }
    } // namespace

    ShaderRef Renderer::get_or_build_shader(const std::string& relative_path, BlendMode blend, CullMode cull) {
        if (relative_path.empty()) {
            return m_default_shader;
        }

        const std::string normalized_path = std::filesystem::path(relative_path).lexically_normal().generic_string();
        const std::string key = shader_cache_key(normalized_path, blend, cull);

        auto it = m_shaders.find(key);
        if (it != m_shaders.end()) {
            if (m_cvar_log_shaders) {
                log::renderer.info("Renderer::get_or_build_shader cache hit: '{}' [{}, {}] (rc={})",
                                   normalized_path,
                                   blend_mode_to_string(blend),
                                   cull_mode_to_string(cull),
                                   it->second.use_count());
            }
            return it->second;
        }

        ShaderRef shader = build_shader(normalized_path, m_offscreen_format, blend, cull);
        if (!shader) {
            shader = m_default_shader;
        }

        if (m_cvar_log_shaders) {
            log::renderer.info("Renderer::get_or_build_shader built: '{}' [{}, {}]",
                               normalized_path,
                               blend_mode_to_string(blend),
                               cull_mode_to_string(cull));
        }

        m_shaders.emplace(key, shader);
        return shader;
    }

    ShaderRef Renderer::get_default_shader() const {
        return m_default_shader;
    }

    SDL_GPUShader* Renderer::load_shader(const std::filesystem::path& hlsl_path,
                                         SDL_ShaderCross_ShaderStage stage,
                                         ShaderReflection* out_reflection) {
        const std::string source = read_text_file(hlsl_path);
        if (source.empty()) {
            log::renderer.error("Failed to read shader: {}", hlsl_path.string());
            return nullptr;
        }

        SDL_ShaderCross_HLSL_Info hlsl_info{};
        hlsl_info.source = source.c_str();
        hlsl_info.entrypoint = "main";
        hlsl_info.shader_stage = stage;

        size_t spirv_size = 0;
        void* spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlsl_info, &spirv_size);
        if (!spirv) {
            log::renderer.error("CompileSPIRVFromHLSL failed for {}: {}", hlsl_path.string(), SDL_GetError());
            return nullptr;
        }

        SDL_ShaderCross_GraphicsShaderMetadata* meta =
            SDL_ShaderCross_ReflectGraphicsSPIRV(static_cast<Uint8*>(spirv), spirv_size, 0);
        if (!meta) {
            log::renderer.error("ReflectGraphicsSPIRV failed for {}: {}", hlsl_path.string(), SDL_GetError());
            SDL_free(spirv);
            return nullptr;
        }

        ShaderReflection reflection;
        if (reflect_spirv(spirv, spirv_size, reflection)) {
            if (m_cvar_log_shader_reflection) {
                log_shader_reflection(hlsl_path.filename().string(), reflection);
            }
        }
        else {
            log::renderer.error("SPIRV-Reflect failed for {}", hlsl_path.string());
        }

        if (out_reflection) {
            *out_reflection = std::move(reflection);
        }

        SDL_ShaderCross_SPIRV_Info sp_info{};
        sp_info.bytecode = static_cast<Uint8*>(spirv);
        sp_info.bytecode_size = spirv_size;
        sp_info.entrypoint = "main";
        sp_info.shader_stage = stage;

        SDL_GPUShader* shader =
            SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(m_gpu_device, &sp_info, &meta->resource_info, 0);

        SDL_free(spirv);
        SDL_free(meta);

        if (!shader) {
            log::renderer.error("CompileGraphicsShaderFromSPIRV failed for {}: {}", hlsl_path.string(), SDL_GetError());
            return nullptr;
        }

        return shader;
    }

    MaterialRef Renderer::create_material(ShaderRef shader) {
        MaterialRef material = std::make_shared<Material>(shader ? std::move(shader) : m_default_shader);
        track_material(material);
        if (m_cvar_log_materials) {
            const Shader* s = material->get_shader();
            log::renderer.info("Renderer::create_material: shader '{}'", s ? s->get_path() : "<none>");
        }
        return material;
    }

    MaterialRef Renderer::clone_material(const Material& source) {
        MaterialRef material = source.clone();
        material->set_name(source.get_name() + " (clone)");
        track_material(material);
        if (m_cvar_log_materials) {
            log::renderer.info("Renderer::clone_material: from '{}'",
                               source.get_name().empty() ? "<inline>" : source.get_name().c_str());
        }
        return material;
    }

    MaterialRef Renderer::get_default_material() const {
        return m_default_material;
    }

    SDL_GPUSampler* Renderer::get_or_create_sampler(const SamplerDesc& desc) {
        const uint32_t key = desc.key();
        auto it = m_samplers.find(key);
        if (it != m_samplers.end()) {
            return it->second;
        }

        const SDL_GPUSamplerCreateInfo info = to_sdl_sampler_create_info(desc);
        SDL_GPUSampler* sampler = SDL_CreateGPUSampler(m_gpu_device, &info);
        if (!sampler) {
            log::renderer.error("SDL_CreateGPUSampler (filter={}, wrap={}) failed: {}",
                                texture_filter_to_string(desc.filter),
                                texture_wrap_to_string(desc.wrap),
                                SDL_GetError());
            return m_default_sampler;
        }

        m_samplers.emplace(key, sampler);
        return sampler;
    }

    const SamplerDesc& Renderer::get_default_sampler_desc() const {
        return m_default_sampler_desc;
    }

    SDL_GPUTextureFormat Renderer::probe_depth_format() const {
        constexpr SDL_GPUTextureFormat candidates[] = {
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTUREFORMAT_D24_UNORM, SDL_GPU_TEXTUREFORMAT_D16_UNORM};
        for (const SDL_GPUTextureFormat candidate : candidates) {
            if (SDL_GPUTextureSupportsFormat(
                    m_gpu_device, candidate, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
                log::renderer.info("Renderer: depth format {}", static_cast<int32_t>(candidate));
                return candidate;
            }
        }

        log::renderer.error("Renderer: no supported depth format; 3D rendering will fail");
        return SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    }

    SDL_GPUTextureFormat Renderer::probe_hdr_format() const {
        constexpr SDL_GPUTextureUsageFlags usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        if (SDL_GPUTextureSupportsFormat(
                m_gpu_device, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, SDL_GPU_TEXTURETYPE_2D, usage)) {
            return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        }

        log::renderer.error("Renderer: R16G16B16A16_FLOAT color targets unsupported; 3D renders in LDR");
        return m_offscreen_format;
    }

    SDL_GPUSampleCount Renderer::probe_msaa_sample_count(int32_t requested_samples) const {
        SDL_GPUSampleCount requested = SDL_GPU_SAMPLECOUNT_1;
        switch (requested_samples) {
            case 2:
                requested = SDL_GPU_SAMPLECOUNT_2;
                break;
            case 4:
                requested = SDL_GPU_SAMPLECOUNT_4;
                break;
            case 8:
                requested = SDL_GPU_SAMPLECOUNT_8;
                break;
            default:
                break;
        }

        while (requested != SDL_GPU_SAMPLECOUNT_1) {
            if (SDL_GPUTextureSupportsSampleCount(m_gpu_device, m_hdr_format, requested) &&
                SDL_GPUTextureSupportsSampleCount(m_gpu_device, m_depth_format, requested)) {
                break;
            }
            requested = static_cast<SDL_GPUSampleCount>(static_cast<int32_t>(requested) - 1);
        }

        log::renderer.info(
            "Renderer: MSAA {}x (requested {}x)", 1 << static_cast<int32_t>(requested), requested_samples);
        return requested;
    }

    SDL_GPUTexture* Renderer::create_depth_target(uint32_t width,
                                                  uint32_t height,
                                                  SDL_GPUSampleCount sample_count,
                                                  bool sampled) const {
        SDL_GPUTextureCreateInfo tci{};
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = m_depth_format;
        tci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        if (sampled) {
            tci.usage |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
        }
        tci.width = std::max(1u, width);
        tci.height = std::max(1u, height);
        tci.layer_count_or_depth = 1;
        tci.num_levels = 1;
        tci.sample_count = sample_count;

        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetFloatProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_DEPTH_FLOAT, 1.0f);
        tci.props = props;

        SDL_GPUTexture* texture = SDL_CreateGPUTexture(m_gpu_device, &tci);

        SDL_DestroyProperties(props);

        if (!texture) {
            log::renderer.error("SDL_CreateGPUTexture (depth target) failed: {}", SDL_GetError());
        }

        return texture;
    }

    SDL_GPUTexture* Renderer::create_color_target(uint32_t width, uint32_t height) const {
        SDL_GPUTextureCreateInfo tci{};
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = m_offscreen_format;
        tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tci.width = std::max(1u, width);
        tci.height = std::max(1u, height);
        tci.layer_count_or_depth = 1;
        tci.num_levels = 1;
        tci.sample_count = SDL_GPU_SAMPLECOUNT_1;

        // Bake the optimized clear value (used by the D3D12 backend) to match the per-frame
        // LOADOP_CLEAR to CLEAR_COLOR in render_world_pass. Without it the clear takes a slower
        // path and the D3D12 debug layer warns (#820 CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE).
        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetFloatProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_R_FLOAT, CLEAR_COLOR.r);
        SDL_SetFloatProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_G_FLOAT, CLEAR_COLOR.g);
        SDL_SetFloatProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_B_FLOAT, CLEAR_COLOR.b);
        SDL_SetFloatProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_A_FLOAT, CLEAR_COLOR.a);
        tci.props = props;

        SDL_GPUTexture* texture = SDL_CreateGPUTexture(m_gpu_device, &tci);

        SDL_DestroyProperties(props);

        if (!texture) {
            log::renderer.error("SDL_CreateGPUTexture (color target) failed: {}", SDL_GetError());
        }

        return texture;
    }

    SDL_GPUTexture* Renderer::create_hdr_target(uint32_t width,
                                                uint32_t height,
                                                SDL_GPUSampleCount sample_count) const {
        const bool is_multisampled = sample_count != SDL_GPU_SAMPLECOUNT_1;

        SDL_GPUTextureCreateInfo tci{};
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = m_hdr_format;
        tci.usage = is_multisampled ? SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
                                    : (SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
        tci.width = std::max(1u, width);
        tci.height = std::max(1u, height);
        tci.layer_count_or_depth = 1;
        tci.num_levels = 1;
        tci.sample_count = sample_count;

        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetFloatProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_R_FLOAT, HDR_CLEAR_COLOR.r);
        SDL_SetFloatProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_G_FLOAT, HDR_CLEAR_COLOR.g);
        SDL_SetFloatProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_B_FLOAT, HDR_CLEAR_COLOR.b);
        SDL_SetFloatProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_A_FLOAT, HDR_CLEAR_COLOR.a);
        tci.props = props;

        SDL_GPUTexture* texture = SDL_CreateGPUTexture(m_gpu_device, &tci);

        SDL_DestroyProperties(props);

        if (!texture) {
            log::renderer.error("SDL_CreateGPUTexture (hdr target) failed: {}", SDL_GetError());
        }

        return texture;
    }

    SDL_GPUTexture* Renderer::create_render_texture(SDL_GPUTextureFormat format,
                                                    uint32_t width,
                                                    uint32_t height,
                                                    const Color& clear_color) const {
        SDL_GPUTextureCreateInfo tci{};
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = format;
        tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tci.width = std::max(1u, width);
        tci.height = std::max(1u, height);
        tci.layer_count_or_depth = 1;
        tci.num_levels = 1;
        tci.sample_count = SDL_GPU_SAMPLECOUNT_1;

        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetFloatProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_R_FLOAT, clear_color.r);
        SDL_SetFloatProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_G_FLOAT, clear_color.g);
        SDL_SetFloatProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_B_FLOAT, clear_color.b);
        SDL_SetFloatProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_A_FLOAT, clear_color.a);
        tci.props = props;

        SDL_GPUTexture* texture = SDL_CreateGPUTexture(m_gpu_device, &tci);

        SDL_DestroyProperties(props);

        if (!texture) {
            log::renderer.error("SDL_CreateGPUTexture (render texture format {}) failed: {}",
                                static_cast<int32_t>(format),
                                SDL_GetError());
        }

        return texture;
    }

    bool Renderer::create_targets_3d(RenderTargets3D& targets, uint32_t width, uint32_t height) const {
        release_targets_3d(targets);

        targets.width = std::max(1u, width);
        targets.height = std::max(1u, height);
        targets.sample_count = m_msaa_sample_count;
        targets.hdr_color = create_hdr_target(targets.width, targets.height, targets.sample_count);
        targets.depth = create_depth_target(targets.width, targets.height, targets.sample_count);
        if (targets.is_multisampled()) {
            targets.hdr_resolved = create_hdr_target(targets.width, targets.height, SDL_GPU_SAMPLECOUNT_1);
        }
        targets.prepass_normal =
            create_render_texture(PREPASS_NORMAL_FORMAT, targets.width, targets.height, Color(0.0f, 0.0f, 0.0f, 0.0f));
        targets.prepass_depth = create_depth_target(targets.width, targets.height, SDL_GPU_SAMPLECOUNT_1, true);
        targets.ssao_raw = create_render_texture(SSAO_FORMAT, targets.width, targets.height, Color::white());
        targets.ssao_blurred = create_render_texture(SSAO_FORMAT, targets.width, targets.height, Color::white());

        targets.fog_half = create_render_texture(m_hdr_format,
                                                 std::max(1u, targets.width / 2),
                                                 std::max(1u, targets.height / 2),
                                                 Color(0.0f, 0.0f, 0.0f, 1.0f));
        targets.hdr_fogged =
            create_render_texture(m_hdr_format, targets.width, targets.height, Color(0.0f, 0.0f, 0.0f, 1.0f));

        bool bloom_valid = true;
        for (uint32_t mip = 0; mip < BLOOM_MIP_COUNT; ++mip) {
            targets.bloom_widths[mip] = std::max(1u, targets.width >> (mip + 1));
            targets.bloom_heights[mip] = std::max(1u, targets.height >> (mip + 1));
            targets.bloom_mips[mip] = create_render_texture(
                m_hdr_format, targets.bloom_widths[mip], targets.bloom_heights[mip], Color(0.0f, 0.0f, 0.0f, 1.0f));
            bloom_valid = bloom_valid && targets.bloom_mips[mip] != nullptr;
        }

        const bool valid = targets.is_valid() && (!targets.is_multisampled() || targets.hdr_resolved != nullptr) &&
                           targets.prepass_normal != nullptr && targets.prepass_depth != nullptr &&
                           targets.ssao_raw != nullptr && targets.ssao_blurred != nullptr && bloom_valid &&
                           targets.fog_half != nullptr && targets.hdr_fogged != nullptr;
        if (!valid) {
            release_targets_3d(targets);
        }

        return valid;
    }

    void Renderer::release_targets_3d(RenderTargets3D& targets) const {
        for (SDL_GPUTexture** texture : {&targets.hdr_color,
                                         &targets.depth,
                                         &targets.hdr_resolved,
                                         &targets.prepass_normal,
                                         &targets.prepass_depth,
                                         &targets.ssao_raw,
                                         &targets.ssao_blurred,
                                         &targets.fog_half,
                                         &targets.hdr_fogged}) {
            if (*texture != nullptr) {
                SDL_ReleaseGPUTexture(m_gpu_device, *texture);
                *texture = nullptr;
            }
        }

        for (SDL_GPUTexture*& mip : targets.bloom_mips) {
            if (mip != nullptr) {
                SDL_ReleaseGPUTexture(m_gpu_device, mip);
                mip = nullptr;
            }
        }

        targets.width = 0;
        targets.height = 0;
    }

    bool Renderer::init_offscreen_targets() {
        if (m_offscreen_color_target) {
            SDL_ReleaseGPUTexture(m_gpu_device, m_offscreen_color_target);
            m_offscreen_color_target = nullptr;
        }

        const uint32_t tex_width =
            std::max(1u, static_cast<uint32_t>(std::round(m_logical_size.x * m_render_scale * m_pixel_density)));
        const uint32_t tex_height =
            std::max(1u, static_cast<uint32_t>(std::round(m_logical_size.y * m_render_scale * m_pixel_density)));

        m_offscreen_color_target = create_color_target(tex_width, tex_height);
        const bool targets_3d_created = create_targets_3d(m_offscreen_targets_3d, tex_width, tex_height);

        return m_offscreen_color_target != nullptr && targets_3d_created;
    }

    void Renderer::rebuild_mesh_shader_pipelines() {
        init_sky_pipeline();

        for (auto& [key, shader] : m_shaders) {
            if (shader->get_vertex_layout() != VertexLayout::Mesh) {
                continue;
            }

            ShaderRef rebuilt =
                build_shader(shader->get_path(), m_offscreen_format, shader->get_blend_mode(), shader->get_cull_mode());
            if (rebuilt) {
                shader->replace_pipeline(rebuilt->detach_pipeline());
            }
        }
    }

    bool Renderer::init_samplers() {
        m_default_sampler = get_or_create_sampler(m_default_sampler_desc);
        if (!m_default_sampler) {
            return false;
        }

        SDL_GPUSamplerCreateInfo blit_info{};
        blit_info.min_filter = SDL_GPU_FILTER_LINEAR;
        blit_info.mag_filter = SDL_GPU_FILTER_LINEAR;
        blit_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        blit_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        blit_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        blit_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

        m_blit_sampler = SDL_CreateGPUSampler(m_gpu_device, &blit_info);
        if (!m_blit_sampler) {
            log::renderer.error("SDL_CreateGPUSampler (blit) failed: {}", SDL_GetError());
            return false;
        }

        return true;
    }

    bool Renderer::init_quad_vbo() {
        // 6 vertices for two triangles covering the unit square [0,1] x [0,1].
        // Layout per vertex: float2 pos, float2 uv. Must stay in sync with SPRITE_VERTEX_LAYOUT/STRIDE.
        // clang-format off
        const float verts[] = {
            0.0f, 0.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 1.0f,
            0.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
        };
        // clang-format on

        SDL_GPUBufferCreateInfo bci{};
        bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bci.size = sizeof(verts);
        m_quad_vbo = SDL_CreateGPUBuffer(m_gpu_device, &bci);
        if (!m_quad_vbo) {
            log::renderer.error("SDL_CreateGPUBuffer (quad) failed: {}", SDL_GetError());
            return false;
        }

        return upload_buffer(m_quad_vbo, verts, sizeof(verts));
    }

    bool Renderer::init_default_sprite_pipeline() {
        const std::string normalized_path =
            std::filesystem::path(DEFAULT_SPRITE_SHADER).lexically_normal().generic_string();

        ShaderRef shader = build_shader(normalized_path, m_offscreen_format, BlendMode::Alpha, CullMode::None);
        if (!shader) {
            return false;
        }

        // The builtin sprite shader is built before Lua runs, so its defaults are seeded here.
        const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        const float alpha_threshold = 0.1f;
        shader->set_default_param("tint", white, 4);
        shader->set_default_param("alpha_threshold", &alpha_threshold, 1);

        m_default_shader = shader;
        m_shaders.emplace(shader_cache_key(normalized_path, BlendMode::Alpha, CullMode::None), shader);
        m_default_material = create_material(m_default_shader);
        m_default_material->set_name("<default>");
        return true;
    }

    bool Renderer::init_default_mesh_pipeline() {
        const std::string normalized_path =
            std::filesystem::path(DEFAULT_MESH_SHADER).lexically_normal().generic_string();

        ShaderRef shader = build_shader(normalized_path, m_offscreen_format, BlendMode::Opaque, CullMode::Back);
        if (!shader) {
            return false;
        }

        const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        const float use_albedo_texture = 0.0f;
        shader->set_default_param("tint", white, 4);
        shader->set_default_param("use_albedo_texture", &use_albedo_texture, 1);

        m_default_mesh_shader = shader;
        m_shaders.emplace(shader_cache_key(normalized_path, BlendMode::Opaque, CullMode::Back), shader);
        m_default_mesh_material = create_material(m_default_mesh_shader);
        m_default_mesh_material->set_name("<default mesh>");
        return true;
    }

    SDL_GPUGraphicsPipeline* Renderer::create_fullscreen_pipeline(std::string_view vertex_file,
                                                                  std::string_view fragment_file,
                                                                  SDL_GPUTextureFormat target_format,
                                                                  BlendMode blend) {
        const std::filesystem::path shader_dir = PathUtils::get_engine_assets_root() / BUILTIN_SHADERS_DIR;

        SDL_GPUShader* vs = load_shader(shader_dir / vertex_file, SDL_SHADERCROSS_SHADERSTAGE_VERTEX);

        if (!vs) {
            return nullptr;
        }

        SDL_GPUShader* fs = load_shader(shader_dir / fragment_file, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

        if (!fs) {
            SDL_ReleaseGPUShader(m_gpu_device, vs);
            return nullptr;
        }

        SDL_GPUColorTargetDescription ctd{};
        ctd.format = target_format;
        apply_blend_state(ctd.blend_state, blend);

        SDL_GPUGraphicsPipelineCreateInfo gci{};
        gci.vertex_shader = vs;
        gci.fragment_shader = fs;
        // No vertex buffers — blit VS synthesizes verts from SV_VertexID.
        gci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        gci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        gci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        gci.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        gci.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        gci.target_info.color_target_descriptions = &ctd;
        gci.target_info.num_color_targets = 1;
        gci.target_info.has_depth_stencil_target = false;

        SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(m_gpu_device, &gci);

        SDL_ReleaseGPUShader(m_gpu_device, vs);
        SDL_ReleaseGPUShader(m_gpu_device, fs);

        if (!pipeline) {
            log::renderer.error("SDL_CreateGPUGraphicsPipeline ('{}') failed: {}", fragment_file, SDL_GetError());
        }

        return pipeline;
    }

    bool Renderer::init_blit_pipeline() {
        m_blit_pipeline = create_fullscreen_pipeline("blit.vert.hlsl", "blit.frag.hlsl", m_swapchain_format);
        return m_blit_pipeline != nullptr;
    }

    bool Renderer::init_sky_pipeline() {
        const std::filesystem::path shader_dir = PathUtils::get_engine_assets_root() / BUILTIN_SHADERS_DIR;

        SDL_GPUShader* vs = load_shader(shader_dir / "sky.vert.hlsl", SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
        if (!vs) {
            return false;
        }

        SDL_GPUShader* fs = load_shader(shader_dir / "sky.frag.hlsl", SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);
        if (!fs) {
            SDL_ReleaseGPUShader(m_gpu_device, vs);
            return false;
        }

        SDL_GPUColorTargetDescription ctd{};
        ctd.format = m_hdr_format;
        apply_blend_state(ctd.blend_state, BlendMode::Opaque);

        const SDL_GPUStencilOpState stencil_keep{
            SDL_GPU_STENCILOP_KEEP, SDL_GPU_STENCILOP_KEEP, SDL_GPU_STENCILOP_KEEP, SDL_GPU_COMPAREOP_ALWAYS};

        SDL_GPUGraphicsPipelineCreateInfo gci{};
        gci.vertex_shader = vs;
        gci.fragment_shader = fs;
        gci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        gci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        gci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        gci.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        gci.multisample_state.sample_count = m_msaa_sample_count;
        gci.target_info.color_target_descriptions = &ctd;
        gci.target_info.num_color_targets = 1;
        gci.target_info.has_depth_stencil_target = true;
        gci.target_info.depth_stencil_format = m_depth_format;
        gci.depth_stencil_state.enable_depth_test = true;
        gci.depth_stencil_state.enable_depth_write = false;
        gci.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        gci.depth_stencil_state.front_stencil_state = stencil_keep;
        gci.depth_stencil_state.back_stencil_state = stencil_keep;
        gci.depth_stencil_state.compare_mask = 0xFF;
        gci.depth_stencil_state.write_mask = 0xFF;

        if (m_sky_pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_sky_pipeline);
        }
        m_sky_pipeline = SDL_CreateGPUGraphicsPipeline(m_gpu_device, &gci);

        SDL_ReleaseGPUShader(m_gpu_device, vs);
        SDL_ReleaseGPUShader(m_gpu_device, fs);

        if (!m_sky_pipeline) {
            log::renderer.error("SDL_CreateGPUGraphicsPipeline (sky) failed: {}", SDL_GetError());
            return false;
        }

        return true;
    }

    bool Renderer::init_shadow_resources() {
        SDL_GPUTextureCreateInfo tci{};
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = m_depth_format;
        tci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tci.width = SHADOW_MAP_SIZE;
        tci.height = SHADOW_MAP_SIZE;
        tci.layer_count_or_depth = 1;
        tci.num_levels = 1;
        tci.sample_count = SDL_GPU_SAMPLECOUNT_1;

        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetFloatProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_DEPTH_FLOAT, 1.0f);
        tci.props = props;
        m_shadow_map = SDL_CreateGPUTexture(m_gpu_device, &tci);
        SDL_DestroyProperties(props);
        if (!m_shadow_map) {
            log::renderer.error("SDL_CreateGPUTexture (shadow map) failed: {}", SDL_GetError());
            return false;
        }

        SDL_GPUSamplerCreateInfo sci{};
        sci.min_filter = SDL_GPU_FILTER_LINEAR;
        sci.mag_filter = SDL_GPU_FILTER_LINEAR;
        sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sci.enable_compare = true;
        sci.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        m_shadow_sampler = SDL_CreateGPUSampler(m_gpu_device, &sci);
        if (!m_shadow_sampler) {
            log::renderer.error("SDL_CreateGPUSampler (shadow) failed: {}", SDL_GetError());
            return false;
        }

        const std::filesystem::path shader_dir = PathUtils::get_engine_assets_root() / BUILTIN_SHADERS_DIR;
        SDL_GPUShader* vs = load_shader(shader_dir / "shadow_depth.vert.hlsl", SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
        if (!vs) {
            return false;
        }

        SDL_GPUShader* fs = load_shader(shader_dir / "shadow_depth.frag.hlsl", SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);
        if (!fs) {
            SDL_ReleaseGPUShader(m_gpu_device, vs);
            return false;
        }

        SDL_GPUVertexBufferDescription vbd{};
        vbd.slot = 0;
        vbd.pitch = sizeof(MeshVertex);
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attr{};
        attr.location = 0;
        attr.buffer_slot = 0;
        attr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attr.offset = 0;

        const SDL_GPUStencilOpState stencil_keep{
            SDL_GPU_STENCILOP_KEEP, SDL_GPU_STENCILOP_KEEP, SDL_GPU_STENCILOP_KEEP, SDL_GPU_COMPAREOP_ALWAYS};

        SDL_GPUGraphicsPipelineCreateInfo gci{};
        gci.vertex_shader = vs;
        gci.fragment_shader = fs;
        gci.vertex_input_state.vertex_buffer_descriptions = &vbd;
        gci.vertex_input_state.num_vertex_buffers = 1;
        gci.vertex_input_state.vertex_attributes = &attr;
        gci.vertex_input_state.num_vertex_attributes = 1;
        gci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        gci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        gci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        gci.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        gci.rasterizer_state.enable_depth_bias = true;
        gci.rasterizer_state.depth_bias_constant_factor = 4.0f;
        gci.rasterizer_state.depth_bias_slope_factor = 2.0f;
        gci.rasterizer_state.enable_depth_clip = true;
        gci.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        gci.target_info.num_color_targets = 0;
        gci.target_info.has_depth_stencil_target = true;
        gci.target_info.depth_stencil_format = m_depth_format;
        gci.depth_stencil_state.enable_depth_test = true;
        gci.depth_stencil_state.enable_depth_write = true;
        gci.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        gci.depth_stencil_state.front_stencil_state = stencil_keep;
        gci.depth_stencil_state.back_stencil_state = stencil_keep;
        gci.depth_stencil_state.compare_mask = 0xFF;
        gci.depth_stencil_state.write_mask = 0xFF;

        m_shadow_pipeline = SDL_CreateGPUGraphicsPipeline(m_gpu_device, &gci);

        SDL_ReleaseGPUShader(m_gpu_device, vs);
        SDL_ReleaseGPUShader(m_gpu_device, fs);

        if (!m_shadow_pipeline) {
            log::renderer.error("SDL_CreateGPUGraphicsPipeline (shadow) failed: {}", SDL_GetError());
            return false;
        }

        return true;
    }

    bool Renderer::init_prepass_pipeline() {
        const std::filesystem::path shader_dir = PathUtils::get_engine_assets_root() / BUILTIN_SHADERS_DIR;
        SDL_GPUShader* vs = load_shader(shader_dir / "prepass.vert.hlsl", SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
        if (!vs) {
            return false;
        }

        SDL_GPUShader* fs = load_shader(shader_dir / "prepass.frag.hlsl", SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);
        if (!fs) {
            SDL_ReleaseGPUShader(m_gpu_device, vs);
            return false;
        }

        SDL_GPUVertexBufferDescription vbd{};
        vbd.slot = 0;
        vbd.pitch = sizeof(MeshVertex);
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attrs[2]{};
        attrs[0].location = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset = 0;
        attrs[1].location = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset = 3 * sizeof(float);

        SDL_GPUColorTargetDescription ctd{};
        ctd.format = PREPASS_NORMAL_FORMAT;
        apply_blend_state(ctd.blend_state, BlendMode::Opaque);

        const SDL_GPUStencilOpState stencil_keep{
            SDL_GPU_STENCILOP_KEEP, SDL_GPU_STENCILOP_KEEP, SDL_GPU_STENCILOP_KEEP, SDL_GPU_COMPAREOP_ALWAYS};

        SDL_GPUGraphicsPipelineCreateInfo gci{};
        gci.vertex_shader = vs;
        gci.fragment_shader = fs;
        gci.vertex_input_state.vertex_buffer_descriptions = &vbd;
        gci.vertex_input_state.num_vertex_buffers = 1;
        gci.vertex_input_state.vertex_attributes = attrs;
        gci.vertex_input_state.num_vertex_attributes = 2;
        gci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        gci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        gci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        gci.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        gci.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        gci.target_info.color_target_descriptions = &ctd;
        gci.target_info.num_color_targets = 1;
        gci.target_info.has_depth_stencil_target = true;
        gci.target_info.depth_stencil_format = m_depth_format;
        gci.depth_stencil_state.enable_depth_test = true;
        gci.depth_stencil_state.enable_depth_write = true;
        gci.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        gci.depth_stencil_state.front_stencil_state = stencil_keep;
        gci.depth_stencil_state.back_stencil_state = stencil_keep;
        gci.depth_stencil_state.compare_mask = 0xFF;
        gci.depth_stencil_state.write_mask = 0xFF;

        m_prepass_pipeline = SDL_CreateGPUGraphicsPipeline(m_gpu_device, &gci);

        SDL_ReleaseGPUShader(m_gpu_device, vs);
        SDL_ReleaseGPUShader(m_gpu_device, fs);

        if (!m_prepass_pipeline) {
            log::renderer.error("SDL_CreateGPUGraphicsPipeline (prepass) failed: {}", SDL_GetError());
            return false;
        }

        return true;
    }

    bool Renderer::init_fog_pipelines() {
        m_fog_pipeline = create_fullscreen_pipeline("fullscreen.vert.hlsl", "volumetric_fog.frag.hlsl", m_hdr_format);
        m_fog_upsample_pipeline =
            create_fullscreen_pipeline("fullscreen.vert.hlsl", "fog_upsample.frag.hlsl", m_hdr_format);
        return m_fog_pipeline != nullptr && m_fog_upsample_pipeline != nullptr;
    }

    bool Renderer::init_bloom_pipelines() {
        m_bloom_prefilter_pipeline =
            create_fullscreen_pipeline("fullscreen.vert.hlsl", "bloom_prefilter.frag.hlsl", m_hdr_format);
        m_bloom_downsample_pipeline =
            create_fullscreen_pipeline("fullscreen.vert.hlsl", "bloom_downsample.frag.hlsl", m_hdr_format);
        m_bloom_upsample_pipeline = create_fullscreen_pipeline(
            "fullscreen.vert.hlsl", "bloom_upsample.frag.hlsl", m_hdr_format, BlendMode::Additive);
        return m_bloom_prefilter_pipeline != nullptr && m_bloom_downsample_pipeline != nullptr &&
               m_bloom_upsample_pipeline != nullptr;
    }

    bool Renderer::init_ssao_pipelines() {
        m_ssao_pipeline = create_fullscreen_pipeline("fullscreen.vert.hlsl", "ssao.frag.hlsl", SSAO_FORMAT);
        m_ssao_blur_pipeline = create_fullscreen_pipeline("fullscreen.vert.hlsl", "ssao_blur.frag.hlsl", SSAO_FORMAT);
        return m_ssao_pipeline != nullptr && m_ssao_blur_pipeline != nullptr;
    }

    bool Renderer::init_tonemap_pipeline() {
        m_tonemap_pipeline =
            create_fullscreen_pipeline("fullscreen.vert.hlsl", "tonemap.frag.hlsl", m_offscreen_format);
        return m_tonemap_pipeline != nullptr;
    }

    bool Renderer::init_debug_line_pipeline() {
        const std::filesystem::path shader_dir = PathUtils::get_engine_assets_root() / BUILTIN_SHADERS_DIR;

        SDL_GPUShader* vs = load_shader(shader_dir / "line.vert.hlsl", SDL_SHADERCROSS_SHADERSTAGE_VERTEX);

        if (!vs) {
            return false;
        }

        SDL_GPUShader* fs = load_shader(shader_dir / "line.frag.hlsl", SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

        if (!fs) {
            SDL_ReleaseGPUShader(m_gpu_device, vs);
            return false;
        }

        // DebugLineVertex layout: float2 pos (offset 0), float4 color (offset 8).
        SDL_GPUVertexBufferDescription vbd{};
        vbd.slot = 0;
        vbd.pitch = sizeof(DebugLineVertex);
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attrs[2]{};
        attrs[0].location = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[0].offset = 0;
        attrs[1].location = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[1].offset = sizeof(Vector2);

        SDL_GPUColorTargetDescription ctd{};
        ctd.format = m_swapchain_format;
        ctd.blend_state.enable_blend = true;
        ctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        ctd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        ctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        ctd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo gci{};
        gci.vertex_shader = vs;
        gci.fragment_shader = fs;
        gci.vertex_input_state.vertex_buffer_descriptions = &vbd;
        gci.vertex_input_state.num_vertex_buffers = 1;
        gci.vertex_input_state.vertex_attributes = attrs;
        gci.vertex_input_state.num_vertex_attributes = 2;
        gci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        gci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        gci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        gci.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        gci.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        gci.target_info.color_target_descriptions = &ctd;
        gci.target_info.num_color_targets = 1;
        gci.target_info.has_depth_stencil_target = false;

        m_debug_line_pipeline = SDL_CreateGPUGraphicsPipeline(m_gpu_device, &gci);

        SDL_ReleaseGPUShader(m_gpu_device, vs);
        SDL_ReleaseGPUShader(m_gpu_device, fs);

        if (!m_debug_line_pipeline) {
            log::renderer.error("SDL_CreateGPUGraphicsPipeline (debug_line) failed: {}", SDL_GetError());
            return false;
        }

        const uint32_t buffer_bytes = MAX_DEBUG_LINE_VERTICES * sizeof(DebugLineVertex);

        SDL_GPUBufferCreateInfo bci{};
        bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bci.size = buffer_bytes;
        m_debug_line_vbo = SDL_CreateGPUBuffer(m_gpu_device, &bci);
        if (!m_debug_line_vbo) {
            log::renderer.error("SDL_CreateGPUBuffer (debug_line) failed: {}", SDL_GetError());
            return false;
        }

        SDL_GPUTransferBufferCreateInfo tbi{};
        tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbi.size = buffer_bytes;
        m_debug_line_transfer_buffer = SDL_CreateGPUTransferBuffer(m_gpu_device, &tbi);
        if (!m_debug_line_transfer_buffer) {
            log::renderer.error("SDL_CreateGPUTransferBuffer (debug_line) failed: {}", SDL_GetError());
            return false;
        }

        return true;
    }

    bool Renderer::init_debug_text_pipeline() {
        const std::filesystem::path shader_dir = PathUtils::get_engine_assets_root() / BUILTIN_SHADERS_DIR;

        SDL_GPUShader* vs = load_shader(shader_dir / "debug_text.vert.hlsl", SDL_SHADERCROSS_SHADERSTAGE_VERTEX);

        if (!vs) {
            return false;
        }

        SDL_GPUShader* fs = load_shader(shader_dir / "debug_text.frag.hlsl", SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

        if (!fs) {
            SDL_ReleaseGPUShader(m_gpu_device, vs);
            return false;
        }

        // DebugTextVertex layout: float2 pos (0), float2 uv (8), float4 color (16).
        SDL_GPUVertexBufferDescription vbd{};
        vbd.slot = 0;
        vbd.pitch = sizeof(DebugTextVertex);
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attrs[3]{};
        attrs[0].location = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[0].offset = 0;
        attrs[1].location = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[1].offset = sizeof(Vector2);
        attrs[2].location = 2;
        attrs[2].buffer_slot = 0;
        attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[2].offset = sizeof(Vector2) * 2;

        SDL_GPUColorTargetDescription ctd{};
        ctd.format = m_swapchain_format;
        ctd.blend_state.enable_blend = true;
        ctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        ctd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        ctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        ctd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        ctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo gci{};
        gci.vertex_shader = vs;
        gci.fragment_shader = fs;
        gci.vertex_input_state.vertex_buffer_descriptions = &vbd;
        gci.vertex_input_state.num_vertex_buffers = 1;
        gci.vertex_input_state.vertex_attributes = attrs;
        gci.vertex_input_state.num_vertex_attributes = 3;
        gci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        gci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        gci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        gci.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        gci.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        gci.target_info.color_target_descriptions = &ctd;
        gci.target_info.num_color_targets = 1;
        gci.target_info.has_depth_stencil_target = false;

        m_debug_text_pipeline = SDL_CreateGPUGraphicsPipeline(m_gpu_device, &gci);

        SDL_ReleaseGPUShader(m_gpu_device, vs);
        SDL_ReleaseGPUShader(m_gpu_device, fs);

        if (!m_debug_text_pipeline) {
            log::renderer.error("SDL_CreateGPUGraphicsPipeline (debug_text) failed: {}", SDL_GetError());
            return false;
        }

        const uint32_t vbo_bytes = MAX_DEBUG_TEXT_VERTICES * sizeof(DebugTextVertex);
        const uint32_t ibo_bytes = MAX_DEBUG_TEXT_INDICES * sizeof(uint16_t);

        SDL_GPUBufferCreateInfo vbo_info{};
        vbo_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        vbo_info.size = vbo_bytes;
        m_debug_text_vbo = SDL_CreateGPUBuffer(m_gpu_device, &vbo_info);
        if (!m_debug_text_vbo) {
            log::renderer.error("SDL_CreateGPUBuffer (debug_text vbo) failed: {}", SDL_GetError());
            return false;
        }

        SDL_GPUBufferCreateInfo ibo_info{};
        ibo_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        ibo_info.size = ibo_bytes;
        m_debug_text_ibo = SDL_CreateGPUBuffer(m_gpu_device, &ibo_info);
        if (!m_debug_text_ibo) {
            log::renderer.error("SDL_CreateGPUBuffer (debug_text ibo) failed: {}", SDL_GetError());
            return false;
        }

        SDL_GPUTransferBufferCreateInfo vbo_tbi{};
        vbo_tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        vbo_tbi.size = vbo_bytes;
        m_debug_text_vbo_transfer = SDL_CreateGPUTransferBuffer(m_gpu_device, &vbo_tbi);
        if (!m_debug_text_vbo_transfer) {
            log::renderer.error("SDL_CreateGPUTransferBuffer (debug_text vbo) failed: {}", SDL_GetError());
            return false;
        }

        SDL_GPUTransferBufferCreateInfo ibo_tbi{};
        ibo_tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        ibo_tbi.size = ibo_bytes;
        m_debug_text_ibo_transfer = SDL_CreateGPUTransferBuffer(m_gpu_device, &ibo_tbi);
        if (!m_debug_text_ibo_transfer) {
            log::renderer.error("SDL_CreateGPUTransferBuffer (debug_text ibo) failed: {}", SDL_GetError());
            return false;
        }

        SDL_GPUSamplerCreateInfo sci{};
        sci.min_filter = SDL_GPU_FILTER_LINEAR;
        sci.mag_filter = SDL_GPU_FILTER_LINEAR;
        sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        m_debug_text_sampler = SDL_CreateGPUSampler(m_gpu_device, &sci);
        if (!m_debug_text_sampler) {
            log::renderer.error("SDL_CreateGPUSampler (debug_text) failed: {}", SDL_GetError());
            return false;
        }

        return true;
    }

    bool Renderer::init_debug_font() {
        const std::filesystem::path font_path = PathUtils::get_engine_assets_root() / DEBUG_FONT_PATH;

        // Bake the atlas scaled by pixel_density for crispness on HiDPI displays.
        const float pixel_density = m_game_window->get_pixel_density();
        const float font_size_px = DEBUG_FONT_SIZE_PX * pixel_density;
        if (!m_debug_font.init(*this, font_path, font_size_px)) {
            log::renderer.error("Failed to init debug font from {}", font_path.string());
            return false;
        }

        // Cache the inverse of the density we actually baked at, so draw-time down-scaling always
        // matches this atlas even if the window's live pixel density later changes.
        m_debug_font_baked_inverse_pixel_density = (pixel_density > 0.0f) ? (1.0f / pixel_density) : 1.0f;
        return true;
    }

    ShaderRef Renderer::build_shader(const std::string& relative_path,
                                     SDL_GPUTextureFormat target_format,
                                     BlendMode blend,
                                     CullMode cull) {
        const std::filesystem::path vert_path = PathUtils::resolve_asset_path(relative_path + ".vert.hlsl");
        const std::filesystem::path frag_path = PathUtils::resolve_asset_path(relative_path + ".frag.hlsl");

        ShaderReflection vs_reflection;
        SDL_GPUShader* vs = load_shader(vert_path, SDL_SHADERCROSS_SHADERSTAGE_VERTEX, &vs_reflection);
        if (!vs) {
            return nullptr;
        }

        ShaderReflection fs_reflection;
        SDL_GPUShader* fs = load_shader(frag_path, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT, &fs_reflection);
        if (!fs) {
            SDL_ReleaseGPUShader(m_gpu_device, vs);
            return nullptr;
        }

        std::vector<SDL_GPUVertexAttribute> attrs;
        VertexLayout vertex_layout = VertexLayout::Sprite;
        uint32_t vertex_stride = SPRITE_VERTEX_STRIDE;
        if (!infer_vertex_layout(relative_path, vs_reflection.vertex_inputs, attrs, vertex_layout, vertex_stride)) {
            SDL_ReleaseGPUShader(m_gpu_device, vs);
            SDL_ReleaseGPUShader(m_gpu_device, fs);
            return nullptr;
        }

        const bool is_mesh = vertex_layout == VertexLayout::Mesh;

        SDL_GPUVertexBufferDescription vbd{};
        vbd.slot = 0;
        vbd.pitch = vertex_stride;
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vbd.instance_step_rate = 0;

        SDL_GPUColorTargetDescription ctd{};
        ctd.format = is_mesh ? m_hdr_format : target_format;
        apply_blend_state(ctd.blend_state, blend);

        SDL_GPUGraphicsPipelineCreateInfo gci{};
        gci.vertex_shader = vs;
        gci.fragment_shader = fs;
        gci.vertex_input_state.vertex_buffer_descriptions = &vbd;
        gci.vertex_input_state.num_vertex_buffers = 1;
        gci.vertex_input_state.vertex_attributes = attrs.data();
        gci.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attrs.size());
        gci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        gci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        gci.rasterizer_state.cull_mode = to_sdl_cull(cull);
        gci.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        gci.multisample_state.sample_count = is_mesh ? m_msaa_sample_count : SDL_GPU_SAMPLECOUNT_1;
        gci.target_info.color_target_descriptions = &ctd;
        gci.target_info.num_color_targets = 1;
        gci.target_info.has_depth_stencil_target = is_mesh;
        if (is_mesh) {
            gci.target_info.depth_stencil_format = m_depth_format;
            gci.depth_stencil_state.enable_depth_test = true;
            gci.depth_stencil_state.enable_depth_write = true;
            gci.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

            const SDL_GPUStencilOpState stencil_keep{
                SDL_GPU_STENCILOP_KEEP, SDL_GPU_STENCILOP_KEEP, SDL_GPU_STENCILOP_KEEP, SDL_GPU_COMPAREOP_ALWAYS};
            gci.depth_stencil_state.front_stencil_state = stencil_keep;
            gci.depth_stencil_state.back_stencil_state = stencil_keep;
            gci.depth_stencil_state.compare_mask = 0xFF;
            gci.depth_stencil_state.write_mask = 0xFF;
        }

        SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(m_gpu_device, &gci);

        SDL_ReleaseGPUShader(m_gpu_device, vs);
        SDL_ReleaseGPUShader(m_gpu_device, fs);

        if (!pipeline) {
            log::renderer.error("SDL_CreateGPUGraphicsPipeline ({} '{}') failed: {}",
                                vertex_layout_to_string(vertex_layout),
                                relative_path,
                                SDL_GetError());
            return nullptr;
        }

        auto shader = std::make_shared<Shader>(m_gpu_device, pipeline, relative_path, blend, cull, vertex_layout);

        for (const ShaderUniformBlock& block : fs_reflection.uniform_blocks) {
            if (block_is_named(block, "Engine")) {
                resolve_engine_layout(relative_path, block, *shader);
            }
            else if (block_is_named(block, "Material")) {
                resolve_material_layout(block, *shader);
            }
        }

        std::vector<ShaderTexture> textures;
        for (const ShaderTextureBinding& tex : fs_reflection.textures) {
            if (!is_mesh && tex.binding == SPRITE_TEXTURE_SLOT) {
                continue;
            }
            if (is_mesh && tex.name == SHADOW_MAP_TEXTURE_NAME) {
                shader->set_shadow_map_slot(tex.binding);
                continue;
            }
            if (is_mesh && tex.name == SSAO_TEXTURE_NAME) {
                shader->set_ssao_slot(tex.binding);
                continue;
            }
            if (tex.binding >= MAX_MATERIAL_TEXTURE_SLOTS) {
                log::renderer.error("Shader '{}' texture '{}' binds slot {} beyond the max of {}",
                                    relative_path,
                                    tex.name,
                                    tex.binding,
                                    MAX_MATERIAL_TEXTURE_SLOTS);
                continue;
            }
            textures.emplace_back(tex.name, tex.binding);
        }
        std::sort(textures.begin(), textures.end(), [](const ShaderTexture& a, const ShaderTexture& b) {
            return a.slot < b.slot;
        });
        shader->set_textures(std::move(textures));

        shader->set_default_params(std::vector<uint8_t>(shader->get_material_size(), 0));
        return shader;
    }
} // namespace hob
