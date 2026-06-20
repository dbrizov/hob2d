#include "ui_render_interface.h"

#include <cstddef>
#include <cstring>
#include <filesystem>

#include <SDL3/SDL.h>

#include "engine/core/debug.h"
#include "engine/core/path_utils.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/core/systems/sdl_context.h"

namespace hob {
    namespace {
        struct UiCompiledGeometry {
            SDL_GPUBuffer* vbo;
            SDL_GPUBuffer* ibo;
            uint32_t index_count;
        };

        // HLSL rounds the cbuffer size up to a 16-byte boundary.
        struct UiVertexUniforms {
            float proj[16]; // 0..64
            float translation[2]; // 64..72
            float _pad[2]; // 72..80
        };

        static_assert(sizeof(UiVertexUniforms) == 80);
    } // namespace

    UiRenderInterface::UiRenderInterface(const SdlContext& sdl_context, Renderer& renderer)
        : m_sdl_context(sdl_context)
        , m_renderer(renderer) {}

    UiRenderInterface::~UiRenderInterface() {
        SDL_GPUDevice* gpu_device = m_sdl_context.get_gpu_device();

        if (m_white_texture) {
            SDL_ReleaseGPUTexture(gpu_device, m_white_texture);
        }

        if (m_sampler) {
            SDL_ReleaseGPUSampler(gpu_device, m_sampler);
        }

        if (m_pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(gpu_device, m_pipeline);
        }
    }

    bool UiRenderInterface::init() {
        SDL_GPUDevice* gpu_device = m_sdl_context.get_gpu_device();
        const std::filesystem::path shader_dir = PathUtils::get_assets_root_path() / "builtin" / "shaders";

        SDL_GPUShader* vs = m_renderer.load_shader(shader_dir / "ui.vert.hlsl", SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
        if (!vs) {
            return false;
        }

        SDL_GPUShader* fs = m_renderer.load_shader(shader_dir / "ui.frag.hlsl", SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);
        if (!fs) {
            SDL_ReleaseGPUShader(gpu_device, vs);
            return false;
        }

        SDL_GPUVertexBufferDescription vbd{};
        vbd.slot = 0;
        vbd.pitch = sizeof(Rml::Vertex);
        vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attrs[3]{};
        attrs[0].location = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[0].offset = offsetof(Rml::Vertex, position);
        attrs[1].location = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
        attrs[1].offset = offsetof(Rml::Vertex, colour);
        attrs[2].location = 2;
        attrs[2].buffer_slot = 0;
        attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset = offsetof(Rml::Vertex, tex_coord);

        // RmlUi emits premultiplied-alpha vertex colours and textures.
        SDL_GPUColorTargetDescription ctd{};
        ctd.format = SDL_GetGPUSwapchainTextureFormat(gpu_device, m_sdl_context.get_window());
        ctd.blend_state.enable_blend = true;
        ctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
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

        m_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &gci);

        SDL_ReleaseGPUShader(gpu_device, vs);
        SDL_ReleaseGPUShader(gpu_device, fs);

        if (!m_pipeline) {
            debug::log_error("SDL_CreateGPUGraphicsPipeline (ui) failed: {}", SDL_GetError());
            return false;
        }

        SDL_GPUSamplerCreateInfo sci{};
        sci.min_filter = SDL_GPU_FILTER_LINEAR;
        sci.mag_filter = SDL_GPU_FILTER_LINEAR;
        sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        m_sampler = SDL_CreateGPUSampler(gpu_device, &sci);
        if (!m_sampler) {
            debug::log_error("SDL_CreateGPUSampler (ui) failed: {}", SDL_GetError());
            return false;
        }

        SDL_GPUTextureCreateInfo tci{};
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tci.width = 1;
        tci.height = 1;
        tci.layer_count_or_depth = 1;
        tci.num_levels = 1;
        tci.sample_count = SDL_GPU_SAMPLECOUNT_1;
        m_white_texture = SDL_CreateGPUTexture(gpu_device, &tci);
        if (!m_white_texture) {
            debug::log_error("SDL_CreateGPUTexture (ui white) failed: {}", SDL_GetError());
            return false;
        }

        const uint32_t white = 0xFFFFFFFFu;
        if (!m_renderer.upload_texture_rgba(m_white_texture, &white, 1, 1)) {
            return false;
        }

        const Vector2 logical_size = m_renderer.get_logical_size();
        m_projection = Renderer::ortho_top_left_y_flipped(logical_size.x, logical_size.y);

        m_is_initialized = true;
        return true;
    }

    bool UiRenderInterface::is_initialized() const {
        return m_is_initialized;
    }

    void UiRenderInterface::begin_frame(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* swap_tex) {
        m_active_cmd = cmd;
        m_sdl_context.get_window_size_px(m_target_width, m_target_height);

        SDL_GPUColorTargetInfo ct{};
        ct.texture = swap_tex;
        ct.load_op = SDL_GPU_LOADOP_LOAD;
        ct.store_op = SDL_GPU_STOREOP_STORE;

        m_active_pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
        if (m_active_pass) {
            SDL_BindGPUGraphicsPipeline(m_active_pass, m_pipeline);
        }
    }

    void UiRenderInterface::end_frame() {
        if (m_active_pass) {
            SDL_EndGPURenderPass(m_active_pass);
            m_active_pass = nullptr;
        }
        m_active_cmd = nullptr;
    }

    Rml::CompiledGeometryHandle UiRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                                   Rml::Span<const int> indices) {
        SDL_GPUDevice* gpu_device = m_sdl_context.get_gpu_device();
        const uint32_t vbytes = static_cast<uint32_t>(vertices.size() * sizeof(Rml::Vertex));
        const uint32_t ibytes = static_cast<uint32_t>(indices.size() * sizeof(int));

        SDL_GPUBufferCreateInfo vbci{};
        vbci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        vbci.size = vbytes;
        SDL_GPUBuffer* vbo = SDL_CreateGPUBuffer(gpu_device, &vbci);

        SDL_GPUBufferCreateInfo ibci{};
        ibci.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        ibci.size = ibytes;
        SDL_GPUBuffer* ibo = SDL_CreateGPUBuffer(gpu_device, &ibci);

        if (!vbo || !ibo) {
            debug::log_error("SDL_CreateGPUBuffer (ui geometry) failed: {}", SDL_GetError());
            if (vbo) {
                SDL_ReleaseGPUBuffer(gpu_device, vbo);
            }

            if (ibo) {
                SDL_ReleaseGPUBuffer(gpu_device, ibo);
            }

            return 0;
        }

        m_renderer.upload_buffer(vbo, vertices.data(), vbytes);
        m_renderer.upload_buffer(ibo, indices.data(), ibytes);

        auto* geometry = new UiCompiledGeometry{vbo, ibo, static_cast<uint32_t>(indices.size())};
        return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry);
    }

    void UiRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry,
                                           Rml::Vector2f translation,
                                           Rml::TextureHandle texture) {
        if (!m_active_pass) {
            return;
        }

        const auto* g = reinterpret_cast<const UiCompiledGeometry*>(geometry);

        if (m_scissor_enabled) {
            SDL_SetGPUScissor(m_active_pass, &m_scissor_rect);
        }
        else {
            const SDL_Rect full{0, 0, m_target_width, m_target_height};
            SDL_SetGPUScissor(m_active_pass, &full);
        }

        SDL_GPUBufferBinding vb{};
        vb.buffer = g->vbo;
        vb.offset = 0;
        SDL_BindGPUVertexBuffers(m_active_pass, 0, &vb, 1);

        SDL_GPUBufferBinding ib{};
        ib.buffer = g->ibo;
        ib.offset = 0;
        SDL_BindGPUIndexBuffer(m_active_pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);

        SDL_GPUTexture* tex = m_white_texture;
        if (texture != INVALID_TEXTURE_HANDLE) {
            const auto it = m_textures.find(texture);
            if (it != m_textures.end()) {
                tex = it->second->get_gpu_texture();
            }
        }

        SDL_GPUTextureSamplerBinding ts{};
        ts.texture = tex;
        ts.sampler = m_sampler;
        SDL_BindGPUFragmentSamplers(m_active_pass, 0, &ts, 1);

        UiVertexUniforms u{};
        std::memcpy(u.proj, m_projection.data(), sizeof(u.proj));
        u.translation[0] = translation.x;
        u.translation[1] = translation.y;
        SDL_PushGPUVertexUniformData(m_active_cmd, 0, &u, sizeof(u));

        SDL_DrawGPUIndexedPrimitives(m_active_pass, g->index_count, 1, 0, 0, 0);
    }

    void UiRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
        auto* g = reinterpret_cast<UiCompiledGeometry*>(geometry);
        if (!g) {
            return;
        }

        SDL_GPUDevice* gpu_device = m_sdl_context.get_gpu_device();
        SDL_ReleaseGPUBuffer(gpu_device, g->vbo);
        SDL_ReleaseGPUBuffer(gpu_device, g->ibo);
        delete g;
    }

    Rml::TextureHandle UiRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
        TextureRef texture = m_renderer.get_or_load_texture(source);
        if (!texture) {
            return INVALID_TEXTURE_HANDLE;
        }

        texture_dimensions.x = static_cast<int>(texture->get_width());
        texture_dimensions.y = static_cast<int>(texture->get_height());

        const Rml::TextureHandle handle = m_next_texture_handle++;
        m_textures.emplace(handle, std::move(texture));
        return handle;
    }

    Rml::TextureHandle UiRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source,
                                                          Rml::Vector2i source_dimensions) {
        TextureRef texture = m_renderer.create_texture_from_rgba(
            source.data(), static_cast<uint32_t>(source_dimensions.x), static_cast<uint32_t>(source_dimensions.y));
        if (!texture) {
            return INVALID_TEXTURE_HANDLE;
        }

        const Rml::TextureHandle handle = m_next_texture_handle++;
        m_textures.emplace(handle, std::move(texture));
        return handle;
    }

    void UiRenderInterface::ReleaseTexture(Rml::TextureHandle texture) {
        m_textures.erase(texture);
    }

    void UiRenderInterface::EnableScissorRegion(bool enable) {
        m_scissor_enabled = enable;
    }

    void UiRenderInterface::SetScissorRegion(Rml::Rectanglei region) {
        const Vector2 logical = m_renderer.get_logical_size();
        const float sx = (logical.x > 0.0f) ? static_cast<float>(m_target_width) / logical.x : 1.0f;
        const float sy = (logical.y > 0.0f) ? static_cast<float>(m_target_height) / logical.y : 1.0f;

        m_scissor_rect.x = static_cast<int>(region.Left() * sx);
        m_scissor_rect.y = static_cast<int>(region.Top() * sy);
        m_scissor_rect.w = static_cast<int>(region.Width() * sx);
        m_scissor_rect.h = static_cast<int>(region.Height() * sy);
    }
} // namespace hob
