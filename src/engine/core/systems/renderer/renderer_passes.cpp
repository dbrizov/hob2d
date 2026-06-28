#include <algorithm>
#include <cstring>

#include <SDL3/SDL.h>

#include "renderer.h"

namespace hob {
    namespace {
        SDL_FColor to_sdl_color(const Color& c) {
            return {c.r, c.g, c.b, c.a};
        }

        // HLSL rounds the cbuffer size up to a 16-byte boundary.
        struct SpriteVSUniforms {
            float proj[16]; // 0..64
            float position[2]; // 64..72
            float size[2]; // 72..80
            float pivot[2]; // 80..88
            float rotation; // 88..92
            float _pad; // 92..96
        };

        static_assert(sizeof(SpriteVSUniforms) == 96);
    } // namespace

    void Renderer::render_world_pass() {
        SDL_GPUCommandBuffer* cmd = m_command_buffer;

        SDL_GPUColorTargetInfo ct{};
        ct.texture = m_offscreen_color;
        ct.clear_color = to_sdl_color(CLEAR_COLOR);
        ct.load_op = SDL_GPU_LOADOP_CLEAR;
        ct.store_op = SDL_GPU_STOREOP_STORE;

        // Render pass
        {
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
            if (!pass) {
                return;
            }

            // World sprites: build the draw order as a sorted index list (sorting the pool itself
            // would scramble the handle->index mapping) by (z, shader). Rebuilt every frame so it
            // stays in sync with the pool — the debug sprite-queue view reads it too.
            m_sprite_draw_order.resize(m_sprite_draws.size());
            for (uint32_t i = 0; i < m_sprite_draw_order.size(); ++i) {
                m_sprite_draw_order[i] = i;
            }

            std::stable_sort(m_sprite_draw_order.begin(), m_sprite_draw_order.end(), [this](uint32_t a, uint32_t b) {
                const SpriteDrawData& da = m_sprite_draws[a];
                const SpriteDrawData& db = m_sprite_draws[b];
                if (da.z_index != db.z_index) {
                    return da.z_index < db.z_index;
                }

                return da.get_shader() < db.get_shader();
            });

            if (m_has_camera_view_projection && !m_sprite_draw_order.empty()) {
                SDL_GPUBufferBinding vb{};
                vb.buffer = m_quad_vbo;
                vb.offset = 0;
                SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

                const Shader* bound_shader = nullptr;

                for (const uint32_t index : m_sprite_draw_order) {
                    record_sprite_draw(pass, m_sprite_draws[index], bound_shader);
                }
            }

            SDL_EndGPURenderPass(pass);
        }

        debug_texture_refs();
        debug_shader_refs();
        debug_material_refs();
        debug_sprite_queue();
    }

    void Renderer::render_blit_pass() {
        SDL_GPUColorTargetInfo ct{};
        ct.texture = m_swap_texture;
        ct.load_op = SDL_GPU_LOADOP_DONT_CARE;
        ct.store_op = SDL_GPU_STOREOP_STORE;

        // Render pass
        {
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(m_command_buffer, &ct, 1, nullptr);
            if (!pass) {
                return;
            }

            SDL_BindGPUGraphicsPipeline(pass, m_blit_pipeline);

            SDL_GPUTextureSamplerBinding ts{};
            ts.texture = m_offscreen_color;
            ts.sampler = m_blit_sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, &ts, 1);

            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);

            SDL_EndGPURenderPass(pass);
        }
    }

    void Renderer::render_debug_lines_pass() {
        if (m_pending_debug_line_vertices.empty()) {
            return;
        }

        SDL_GPUCommandBuffer* cmd = m_command_buffer;

        // Upload pending line vertices into the persistent debug-line VBO before the
        // render pass starts (copy passes can't run inside a graphics render pass).
        const uint32_t line_vertex_count =
            static_cast<uint32_t>(std::min<size_t>(m_pending_debug_line_vertices.size(), MAX_DEBUG_LINE_VERTICES));

        const uint32_t bytes = line_vertex_count * sizeof(DebugLineVertex);
        void* map = SDL_MapGPUTransferBuffer(m_gpu_device, m_debug_line_transfer_buffer, true);
        if (!map) {
            m_pending_debug_line_vertices.clear();
            return;
        }
        std::memcpy(map, m_pending_debug_line_vertices.data(), bytes);
        SDL_UnmapGPUTransferBuffer(m_gpu_device, m_debug_line_transfer_buffer);

        // Copy pass
        {
            SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd);
            SDL_GPUTransferBufferLocation src{};
            src.transfer_buffer = m_debug_line_transfer_buffer;
            src.offset = 0;
            SDL_GPUBufferRegion dst{};
            dst.buffer = m_debug_line_vbo;
            dst.offset = 0;
            dst.size = bytes;
            SDL_UploadToGPUBuffer(copy_pass, &src, &dst, true);
            SDL_EndGPUCopyPass(copy_pass);
        }

        SDL_GPUColorTargetInfo ct{};
        ct.texture = m_swap_texture;
        ct.load_op = SDL_GPU_LOADOP_LOAD;
        ct.store_op = SDL_GPU_STOREOP_STORE;

        // Render pass
        {
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
            if (!pass) {
                m_pending_debug_line_vertices.clear();
                return;
            }

            SDL_BindGPUGraphicsPipeline(pass, m_debug_line_pipeline);

            SDL_GPUBufferBinding vb{};
            vb.buffer = m_debug_line_vbo;
            vb.offset = 0;
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

            SDL_PushGPUVertexUniformData(cmd, 0, m_swapchain_projection.data(), Matrix4x4::byte_size());

            SDL_DrawGPUPrimitives(pass, line_vertex_count, 1, 0, 0);

            SDL_EndGPURenderPass(pass);
        }

        m_pending_debug_line_vertices.clear();
    }

    void Renderer::render_debug_text_pass() {
        if (m_pending_debug_text_indices.empty() || !m_debug_font.is_initialized()) {
            m_pending_debug_text_vertices.clear();
            m_pending_debug_text_indices.clear();
            return;
        }

        SDL_GPUCommandBuffer* cmd = m_command_buffer;

        const uint32_t vertex_count =
            static_cast<uint32_t>(std::min<size_t>(m_pending_debug_text_vertices.size(), MAX_DEBUG_TEXT_VERTICES));
        const uint32_t index_count =
            static_cast<uint32_t>(std::min<size_t>(m_pending_debug_text_indices.size(), MAX_DEBUG_TEXT_INDICES));

        const uint32_t vbo_bytes = vertex_count * sizeof(DebugTextVertex);
        const uint32_t ibo_bytes = index_count * sizeof(uint16_t);

        // Upload vertices.
        {
            void* map = SDL_MapGPUTransferBuffer(m_gpu_device, m_debug_text_vbo_transfer, true);
            if (!map) {
                m_pending_debug_text_vertices.clear();
                m_pending_debug_text_indices.clear();
                return;
            }
            std::memcpy(map, m_pending_debug_text_vertices.data(), vbo_bytes);
            SDL_UnmapGPUTransferBuffer(m_gpu_device, m_debug_text_vbo_transfer);
        }

        // Upload indices.
        {
            void* map = SDL_MapGPUTransferBuffer(m_gpu_device, m_debug_text_ibo_transfer, true);
            if (!map) {
                m_pending_debug_text_vertices.clear();
                m_pending_debug_text_indices.clear();
                return;
            }
            std::memcpy(map, m_pending_debug_text_indices.data(), ibo_bytes);
            SDL_UnmapGPUTransferBuffer(m_gpu_device, m_debug_text_ibo_transfer);
        }

        // Copy pass — both buffers in one pass.
        {
            SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd);

            SDL_GPUTransferBufferLocation vsrc{};
            vsrc.transfer_buffer = m_debug_text_vbo_transfer;
            SDL_GPUBufferRegion vdst{};
            vdst.buffer = m_debug_text_vbo;
            vdst.size = vbo_bytes;
            SDL_UploadToGPUBuffer(copy_pass, &vsrc, &vdst, true);

            SDL_GPUTransferBufferLocation isrc{};
            isrc.transfer_buffer = m_debug_text_ibo_transfer;
            SDL_GPUBufferRegion idst{};
            idst.buffer = m_debug_text_ibo;
            idst.size = ibo_bytes;
            SDL_UploadToGPUBuffer(copy_pass, &isrc, &idst, true);

            SDL_EndGPUCopyPass(copy_pass);
        }

        SDL_GPUColorTargetInfo ct{};
        ct.texture = m_swap_texture;
        ct.load_op = SDL_GPU_LOADOP_LOAD;
        ct.store_op = SDL_GPU_STOREOP_STORE;

        // Render pass
        {
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
            if (!pass) {
                m_pending_debug_text_vertices.clear();
                m_pending_debug_text_indices.clear();
                return;
            }

            SDL_BindGPUGraphicsPipeline(pass, m_debug_text_pipeline);

            SDL_GPUBufferBinding vb{};
            vb.buffer = m_debug_text_vbo;
            vb.offset = 0;
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

            SDL_GPUBufferBinding ib{};
            ib.buffer = m_debug_text_ibo;
            ib.offset = 0;
            SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);

            SDL_GPUTextureSamplerBinding ts{};
            ts.texture = m_debug_font.get_atlas_texture();
            ts.sampler = m_debug_text_sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, &ts, 1);

            SDL_PushGPUVertexUniformData(cmd, 0, m_swapchain_projection.data(), Matrix4x4::byte_size());

            SDL_DrawGPUIndexedPrimitives(pass, index_count, 1, 0, 0, 0);

            SDL_EndGPURenderPass(pass);
        }

        m_pending_debug_text_vertices.clear();
        m_pending_debug_text_indices.clear();
    }

    void Renderer::record_sprite_draw(SDL_GPURenderPass* pass,
                                      const SpriteDrawData& draw,
                                      const Shader*& bound_shader) {
        if (!draw.texture || !draw.texture->m_gpu_texture) {
            return;
        }

        const Material& material = draw.material ? *draw.material : *m_default_material;
        const Shader* shader = material.get_shader();
        if (!shader) {
            return;
        }

        if (shader != bound_shader) {
            SDL_BindGPUGraphicsPipeline(pass, shader->get_pipeline());
            bound_shader = shader;
        }

        SpriteVSUniforms vsu{};
        std::memcpy(vsu.proj, m_camera_view_projection.data(), sizeof(vsu.proj));
        vsu.position[0] = draw.world_pos.x;
        vsu.position[1] = draw.world_pos.y;
        vsu.size[0] = draw.size.x;
        vsu.size[1] = draw.size.y;
        vsu.pivot[0] = draw.pivot.x;
        vsu.pivot[1] = draw.pivot.y;
        vsu.rotation = draw.rotation;
        SDL_PushGPUVertexUniformData(m_command_buffer, 0, &vsu, sizeof(vsu));

        push_sprite_fragment_uniforms(*draw.texture, material);

        SDL_GPUTextureSamplerBinding ts{};
        ts.texture = draw.texture->m_gpu_texture;
        ts.sampler = draw.texture->m_sampler ? draw.texture->m_sampler : m_default_sampler;
        SDL_BindGPUFragmentSamplers(pass, SPRITE_TEXTURE_SLOT, &ts, 1);

        for (const ShaderTexture& st : shader->get_textures()) {
            const TextureRef& tex = material.get_texture(st.slot);
            const bool has_texture = tex && tex->m_gpu_texture;

            SDL_GPUTextureSamplerBinding extra{};
            extra.texture = has_texture ? tex->m_gpu_texture : m_fallback_texture->m_gpu_texture;
            extra.sampler = (has_texture && tex->m_sampler) ? tex->m_sampler : m_default_sampler;
            SDL_BindGPUFragmentSamplers(pass, st.slot, &extra, 1);
        }

        SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
    }

    void Renderer::push_sprite_fragment_uniforms(const Texture& texture, const Material& material) {
        const Shader* shader = material.get_shader();
        if (!shader) {
            return;
        }

        if (shader->get_engine_slot() != INVALID_SHADER_SLOT) {
            std::array<uint8_t, ENGINE_CBUFFER_MAX_BYTES> engine_cbuffer{};

            const int32_t texel_off = shader->get_engine_offset(EngineBuiltin::TexelSize);
            if (texel_off >= 0) {
                const uint32_t tex_w = texture.get_width();
                const uint32_t tex_h = texture.get_height();
                const float texel_size[2] = {tex_w > 0 ? 1.0f / static_cast<float>(tex_w) : 0.0f,
                                             tex_h > 0 ? 1.0f / static_cast<float>(tex_h) : 0.0f};
                std::memcpy(engine_cbuffer.data() + texel_off, texel_size, sizeof(texel_size));
            }

            const int32_t game_off = shader->get_engine_offset(EngineBuiltin::GameTime);
            if (game_off >= 0) {
                std::memcpy(engine_cbuffer.data() + game_off, &m_game_time, sizeof(float));
            }

            const int32_t real_off = shader->get_engine_offset(EngineBuiltin::RealTime);
            if (real_off >= 0) {
                std::memcpy(engine_cbuffer.data() + real_off, &m_real_time, sizeof(float));
            }

            SDL_PushGPUFragmentUniformData(
                m_command_buffer, shader->get_engine_slot(), engine_cbuffer.data(), shader->get_engine_size());
        }

        if (shader->get_material_slot() != INVALID_SHADER_SLOT && material.get_params_size() > 0) {
            SDL_PushGPUFragmentUniformData(
                m_command_buffer, shader->get_material_slot(), material.get_params_data(), material.get_params_size());
        }
    }
} // namespace hob
