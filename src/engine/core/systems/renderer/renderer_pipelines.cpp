#include <algorithm>
#include <cmath>
#include <cstring>
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
#include "engine/core/systems/sdl_context.h"
#include "renderer.h"
#include "shader_reflection.h"

namespace hob {
    namespace {
        std::string read_text_file(const std::filesystem::path& path) {
            std::ifstream f(path, std::ios::binary | std::ios::ate);
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
        std::string shader_cache_key(const std::string& path, BlendMode blend, CullMode cull) {
            return path + '|' + std::to_string(static_cast<int>(blend)) + '|' + std::to_string(static_cast<int>(cull));
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
        
        struct SpriteVertexSlot {
            ShaderParamType type;
            uint32_t location;
            uint32_t offset;
        };

        constexpr SpriteVertexSlot SPRITE_VERTEX_LAYOUT[] = {
            {ShaderParamType::Float2, 0, 0},
            {ShaderParamType::Float2, 1, 2 * sizeof(float)},
        };
        constexpr uint32_t SPRITE_VERTEX_STRIDE = 4 * sizeof(float);

        bool build_sprite_vertex_attributes(const std::string& path,
                                            const std::vector<ShaderVertexInput>& inputs,
                                            std::vector<SDL_GPUVertexAttribute>& out_attrs) {
            constexpr uint32_t expected_count = std::size(SPRITE_VERTEX_LAYOUT);
            if (inputs.size() != expected_count) {
                log::renderer.error(
                    "Shader '{}' has {} vertex input(s); the engine quad provides {} (pos float2 @loc0, uv float2 @loc1)",
                    path,
                    inputs.size(),
                    expected_count);
                return false;
            }

            for (const SpriteVertexSlot& slot : SPRITE_VERTEX_LAYOUT) {
                const ShaderVertexInput* match = nullptr;
                for (const ShaderVertexInput& in : inputs) {
                    if (in.location == slot.location) {
                        match = &in;
                        break;
                    }
                }
                if (!match) {
                    log::renderer.error("Shader '{}' is missing vertex input at location {} (expected {})",
                                        path,
                                        slot.location,
                                        to_string(slot.type));
                    return false;
                }
                if (match->type != slot.type) {
                    log::renderer.error("Shader '{}' vertex input at location {} is {}; engine quad provides {}",
                                        path,
                                        slot.location,
                                        to_string(match->type),
                                        to_string(slot.type));
                    return false;
                }

                SDL_GPUVertexAttribute attr{};
                attr.location = match->location;
                attr.buffer_slot = 0;
                attr.format = to_sdl_vertex_format(match->type);
                attr.offset = slot.offset;
                out_attrs.push_back(attr);
            }
            return true;
        }
    } // namespace

    ShaderRef Renderer::get_or_build_shader(const std::string& path, BlendMode blend, CullMode cull) {
        if (path.empty()) {
            return m_default_shader;
        }

        const std::string normalized_path = std::filesystem::path(path).lexically_normal().string();
        const std::string key = shader_cache_key(normalized_path, blend, cull);

        auto it = m_shaders.find(key);
        if (it != m_shaders.end()) {
            return it->second;
        }

        ShaderRef shader = build_shader(normalized_path, m_offscreen_format, blend, cull);
        if (!shader) {
            shader = m_default_shader;
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
        return material;
    }

    MaterialRef Renderer::clone_material(const Material& source) {
        MaterialRef material = source.clone();
        material->set_name(source.get_name() + " (clone)");
        track_material(material);
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

    void Renderer::track_material(const MaterialRef& material) {
        std::erase_if(m_materials, [](const MaterialWeakRef& weak) {
            return weak.expired();
        });
        m_materials.push_back(material);
    }

    bool Renderer::init_offscreen_target() {
        if (m_offscreen_color) {
            SDL_ReleaseGPUTexture(m_gpu_device, m_offscreen_color);
            m_offscreen_color = nullptr;
        }

        const uint32_t tex_width =
            std::max(1u, static_cast<uint32_t>(std::round(m_logical_size.x * m_render_scale * m_pixel_density)));
        const uint32_t tex_height =
            std::max(1u, static_cast<uint32_t>(std::round(m_logical_size.y * m_render_scale * m_pixel_density)));

        SDL_GPUTextureCreateInfo tci{};
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = m_offscreen_format;
        tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tci.width = tex_width;
        tci.height = tex_height;
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

        m_offscreen_color = SDL_CreateGPUTexture(m_gpu_device, &tci);

        SDL_DestroyProperties(props);

        if (!m_offscreen_color) {
            log::renderer.error("SDL_CreateGPUTexture (offscreen) failed: {}", SDL_GetError());
            return false;
        }

        return true;
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
        const std::string normalized_path = std::filesystem::path(DEFAULT_SPRITE_SHADER).lexically_normal().string();

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

    bool Renderer::init_blit_pipeline() {
        const std::filesystem::path shader_dir = PathUtils::get_assets_root_path() / BUILTIN_SHADERS_DIR;

        SDL_GPUShader* vs = load_shader(shader_dir / "blit.vert.hlsl", SDL_SHADERCROSS_SHADERSTAGE_VERTEX);

        if (!vs) {
            return false;
        }

        SDL_GPUShader* fs = load_shader(shader_dir / "blit.frag.hlsl", SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

        if (!fs) {
            SDL_ReleaseGPUShader(m_gpu_device, vs);
            return false;
        }

        SDL_GPUColorTargetDescription ctd{};
        ctd.format = m_swapchain_format;
        ctd.blend_state.enable_blend = false;

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

        m_blit_pipeline = SDL_CreateGPUGraphicsPipeline(m_gpu_device, &gci);

        SDL_ReleaseGPUShader(m_gpu_device, vs);
        SDL_ReleaseGPUShader(m_gpu_device, fs);

        if (!m_blit_pipeline) {
            log::renderer.error("SDL_CreateGPUGraphicsPipeline (blit) failed: {}", SDL_GetError());
            return false;
        }

        return true;
    }

    bool Renderer::init_debug_line_pipeline() {
        const std::filesystem::path shader_dir = PathUtils::get_assets_root_path() / BUILTIN_SHADERS_DIR;

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
        const std::filesystem::path shader_dir = PathUtils::get_assets_root_path() / BUILTIN_SHADERS_DIR;

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
        const std::filesystem::path font_path = PathUtils::get_assets_root_path() / DEBUG_FONT_PATH;

        // Bake the atlas scaled by pixel_density for crispness on HiDPI displays.
        const float pixel_density = m_sdl_context.get_pixel_density();
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

    ShaderRef Renderer::build_shader(const std::string& path,
                                     SDL_GPUTextureFormat target_format,
                                     BlendMode blend,
                                     CullMode cull) {
        const std::filesystem::path assets_root = PathUtils::get_assets_root_path();
        const std::filesystem::path vert_path = assets_root / (path + ".vert.hlsl");
        const std::filesystem::path frag_path = assets_root / (path + ".frag.hlsl");

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
        if (!build_sprite_vertex_attributes(path, vs_reflection.vertex_inputs, attrs)) {
            SDL_ReleaseGPUShader(m_gpu_device, vs);
            SDL_ReleaseGPUShader(m_gpu_device, fs);
            return nullptr;
        }

        SDL_GPUVertexBufferDescription vbd{};
        vbd.slot = 0;
        vbd.pitch = SPRITE_VERTEX_STRIDE;
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vbd.instance_step_rate = 0;

        SDL_GPUColorTargetDescription ctd{};
        ctd.format = target_format;
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
        gci.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        gci.target_info.color_target_descriptions = &ctd;
        gci.target_info.num_color_targets = 1;
        gci.target_info.has_depth_stencil_target = false;

        SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(m_gpu_device, &gci);

        SDL_ReleaseGPUShader(m_gpu_device, vs);
        SDL_ReleaseGPUShader(m_gpu_device, fs);

        if (!pipeline) {
            log::renderer.error("SDL_CreateGPUGraphicsPipeline (sprite '{}') failed: {}", path, SDL_GetError());
            return nullptr;
        }

        auto shader = std::make_shared<Shader>(m_gpu_device, pipeline, path, blend, cull);

        for (const ShaderUniformBlock& block : fs_reflection.uniform_blocks) {
            if (block_is_named(block, "Engine")) {
                shader->set_engine_slot(block.binding);
            }
            else if (block_is_named(block, "Material")) {
                std::unordered_map<std::string, ShaderParam> params;
                params.reserve(block.members.size());
                for (const ShaderUniformMember& m : block.members) {
                    params.emplace(m.name, ShaderParam{m.type, m.offset, m.size});
                }
                // Round up to the 16-byte cbuffer footprint; reflection may report the unpadded size.
                const uint32_t material_size = (block.size + 15u) & ~15u;
                shader->set_material_layout(block.binding, material_size, std::move(params));
            }
        }

        std::vector<ShaderTexture> textures;
        for (const ShaderTextureBinding& tex : fs_reflection.textures) {
            if (tex.binding != SPRITE_TEXTURE_SLOT) {
                textures.push_back(ShaderTexture{tex.name, tex.binding});
            }
        }
        std::sort(textures.begin(), textures.end(), [](const ShaderTexture& a, const ShaderTexture& b) {
            return a.slot < b.slot;
        });
        shader->set_textures(std::move(textures));

        shader->set_default_params(std::vector<uint8_t>(shader->get_material_size(), 0));
        return shader;
    }
} // namespace hob
