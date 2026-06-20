#include <filesystem>
#include <fstream>
#include <string>

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include "engine/core/debug.h"
#include "engine/core/path_utils.h"
#include "engine/core/systems/sdl_context.h"
#include "renderer.h"

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
    } // namespace

    SDL_GPUShader* Renderer::load_shader(const std::filesystem::path& hlsl_path, SDL_ShaderCross_ShaderStage stage) {
        const std::string source = read_text_file(hlsl_path);
        if (source.empty()) {
            debug::log_error("Failed to read shader: {}", hlsl_path.string());
            return nullptr;
        }

        SDL_ShaderCross_HLSL_Info hlsl_info{};
        hlsl_info.source = source.c_str();
        hlsl_info.entrypoint = "main";
        hlsl_info.shader_stage = stage;

        size_t spirv_size = 0;
        void* spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlsl_info, &spirv_size);
        if (!spirv) {
            debug::log_error("CompileSPIRVFromHLSL failed for {}: {}", hlsl_path.string(), SDL_GetError());
            return nullptr;
        }

        SDL_ShaderCross_GraphicsShaderMetadata* meta =
            SDL_ShaderCross_ReflectGraphicsSPIRV(static_cast<Uint8*>(spirv), spirv_size, 0);
        if (!meta) {
            debug::log_error("ReflectGraphicsSPIRV failed for {}: {}", hlsl_path.string(), SDL_GetError());
            SDL_free(spirv);
            return nullptr;
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
            debug::log_error("CompileGraphicsShaderFromSPIRV failed for {}: {}", hlsl_path.string(), SDL_GetError());
            return nullptr;
        }

        return shader;
    }

    ShaderId Renderer::get_or_build_sprite_shader(const std::string& path) {
        if (path.empty()) {
            return DEFAULT_SPRITE_SHADER_ID;
        }

        const std::string key = std::filesystem::path(path).lexically_normal().string();

        auto it = m_shader_path_to_id.find(key);
        if (it != m_shader_path_to_id.end()) {
            return it->second;
        }

        SDL_GPUGraphicsPipeline* pipeline = build_sprite_pipeline(key, m_offscreen_format);
        if (!pipeline) {
            // Alias to default so subsequent lookups are O(1) and silent.
            m_shader_path_to_id.emplace(key, DEFAULT_SPRITE_SHADER_ID);
            return DEFAULT_SPRITE_SHADER_ID;
        }

        const ShaderId id = static_cast<ShaderId>(m_sprite_pipelines.size());
        m_sprite_pipelines.push_back(pipeline);
        m_shader_path_to_id.emplace(key, id);
        return id;
    }

    bool Renderer::init_offscreen_target() {
        SDL_GPUTextureCreateInfo tci{};
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = m_offscreen_format;
        tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tci.width = m_logical_width;
        tci.height = m_logical_height;
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
            debug::log_error("SDL_CreateGPUTexture (offscreen) failed: {}", SDL_GetError());
            return false;
        }

        return true;
    }

    bool Renderer::init_samplers() {
        SDL_GPUSamplerCreateInfo sprite_info{};
        sprite_info.min_filter = SDL_GPU_FILTER_LINEAR;
        sprite_info.mag_filter = SDL_GPU_FILTER_NEAREST;
        sprite_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sprite_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sprite_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sprite_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

        m_sprite_sampler = SDL_CreateGPUSampler(m_gpu_device, &sprite_info);
        if (!m_sprite_sampler) {
            debug::log_error("SDL_CreateGPUSampler (sprite) failed: {}", SDL_GetError());
            return false;
        }

        SDL_GPUSamplerCreateInfo blit_info = sprite_info;
        blit_info.min_filter = SDL_GPU_FILTER_LINEAR;
        blit_info.mag_filter = SDL_GPU_FILTER_LINEAR;

        m_blit_sampler = SDL_CreateGPUSampler(m_gpu_device, &blit_info);
        if (!m_blit_sampler) {
            debug::log_error("SDL_CreateGPUSampler (blit) failed: {}", SDL_GetError());
            return false;
        }

        return true;
    }

    bool Renderer::init_quad_vbo() {
        // 6 vertices for two triangles covering the unit square [0,1] x [0,1].
        // Layout per vertex: float2 pos, float2 uv.
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
            debug::log_error("SDL_CreateGPUBuffer (quad) failed: {}", SDL_GetError());
            return false;
        }

        return upload_buffer(m_quad_vbo, verts, sizeof(verts));
    }

    bool Renderer::init_default_sprite_pipeline() {
        const std::string default_key = std::filesystem::path(DEFAULT_SPRITE_SHADER).lexically_normal().string();

        SDL_GPUGraphicsPipeline* pipeline = build_sprite_pipeline(default_key, m_offscreen_format);
        if (!pipeline) {
            return false;
        }

        // Default lands at slot 0 = DEFAULT_SPRITE_SHADER_ID.
        m_sprite_pipelines.push_back(pipeline);
        m_shader_path_to_id.emplace(default_key, DEFAULT_SPRITE_SHADER_ID);
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
            debug::log_error("SDL_CreateGPUGraphicsPipeline (blit) failed: {}", SDL_GetError());
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
            debug::log_error("SDL_CreateGPUGraphicsPipeline (debug_line) failed: {}", SDL_GetError());
            return false;
        }

        const uint32_t buffer_bytes = MAX_DEBUG_LINE_VERTICES * sizeof(DebugLineVertex);

        SDL_GPUBufferCreateInfo bci{};
        bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bci.size = buffer_bytes;
        m_debug_line_vbo = SDL_CreateGPUBuffer(m_gpu_device, &bci);
        if (!m_debug_line_vbo) {
            debug::log_error("SDL_CreateGPUBuffer (debug_line) failed: {}", SDL_GetError());
            return false;
        }

        SDL_GPUTransferBufferCreateInfo tbi{};
        tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbi.size = buffer_bytes;
        m_debug_line_transfer_buffer = SDL_CreateGPUTransferBuffer(m_gpu_device, &tbi);
        if (!m_debug_line_transfer_buffer) {
            debug::log_error("SDL_CreateGPUTransferBuffer (debug_line) failed: {}", SDL_GetError());
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
            debug::log_error("SDL_CreateGPUGraphicsPipeline (debug_text) failed: {}", SDL_GetError());
            return false;
        }

        const uint32_t vbo_bytes = MAX_DEBUG_TEXT_VERTICES * sizeof(DebugTextVertex);
        const uint32_t ibo_bytes = MAX_DEBUG_TEXT_INDICES * sizeof(uint16_t);

        SDL_GPUBufferCreateInfo vbo_info{};
        vbo_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        vbo_info.size = vbo_bytes;
        m_debug_text_vbo = SDL_CreateGPUBuffer(m_gpu_device, &vbo_info);
        if (!m_debug_text_vbo) {
            debug::log_error("SDL_CreateGPUBuffer (debug_text vbo) failed: {}", SDL_GetError());
            return false;
        }

        SDL_GPUBufferCreateInfo ibo_info{};
        ibo_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        ibo_info.size = ibo_bytes;
        m_debug_text_ibo = SDL_CreateGPUBuffer(m_gpu_device, &ibo_info);
        if (!m_debug_text_ibo) {
            debug::log_error("SDL_CreateGPUBuffer (debug_text ibo) failed: {}", SDL_GetError());
            return false;
        }

        SDL_GPUTransferBufferCreateInfo vbo_tbi{};
        vbo_tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        vbo_tbi.size = vbo_bytes;
        m_debug_text_vbo_transfer = SDL_CreateGPUTransferBuffer(m_gpu_device, &vbo_tbi);
        if (!m_debug_text_vbo_transfer) {
            debug::log_error("SDL_CreateGPUTransferBuffer (debug_text vbo) failed: {}", SDL_GetError());
            return false;
        }

        SDL_GPUTransferBufferCreateInfo ibo_tbi{};
        ibo_tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        ibo_tbi.size = ibo_bytes;
        m_debug_text_ibo_transfer = SDL_CreateGPUTransferBuffer(m_gpu_device, &ibo_tbi);
        if (!m_debug_text_ibo_transfer) {
            debug::log_error("SDL_CreateGPUTransferBuffer (debug_text ibo) failed: {}", SDL_GetError());
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
            debug::log_error("SDL_CreateGPUSampler (debug_text) failed: {}", SDL_GetError());
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
            debug::log_error("Failed to init debug font from {}", font_path.string());
            return false;
        }

        // Cache the inverse of the density we actually baked at, so draw-time down-scaling always
        // matches this atlas even if the window's live pixel density later changes.
        m_debug_font_baked_inverse_pixel_density = (pixel_density > 0.0f) ? (1.0f / pixel_density) : 1.0f;
        return true;
    }

    SDL_GPUGraphicsPipeline* Renderer::build_sprite_pipeline(const std::string& path,
                                                             SDL_GPUTextureFormat target_format) {
        const std::filesystem::path assets_root = PathUtils::get_assets_root_path();
        const std::filesystem::path vert_path = assets_root / (path + ".vert.hlsl");
        const std::filesystem::path frag_path = assets_root / (path + ".frag.hlsl");

        SDL_GPUShader* vs = load_shader(vert_path, SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
        if (!vs) {
            return nullptr;
        }

        SDL_GPUShader* fs = load_shader(frag_path, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);
        if (!fs) {
            SDL_ReleaseGPUShader(m_gpu_device, vs);
            return nullptr;
        }

        SDL_GPUVertexBufferDescription vbd{};
        vbd.slot = 0;
        vbd.pitch = 4 * sizeof(float);
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vbd.instance_step_rate = 0;

        SDL_GPUVertexAttribute attrs[2]{};
        attrs[0].location = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[0].offset = 0;
        attrs[1].location = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[1].offset = 2 * sizeof(float);

        SDL_GPUColorTargetDescription ctd{};
        ctd.format = target_format;
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

        SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(m_gpu_device, &gci);

        SDL_ReleaseGPUShader(m_gpu_device, vs);
        SDL_ReleaseGPUShader(m_gpu_device, fs);

        if (!pipeline) {
            debug::log_error("SDL_CreateGPUGraphicsPipeline (sprite '{}') failed: {}", path, SDL_GetError());
            return nullptr;
        }

        return pipeline;
    }
} // namespace hob
