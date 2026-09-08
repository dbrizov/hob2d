#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include <SDL3/SDL.h>

#include "engine/core/logging.h"
#include "renderer.h"

namespace hob {
    namespace {
        SDL_FColor to_sdl_color(const Color& c) {
            return {c.r, c.g, c.b, c.a};
        }

        // HLSL rounds the cbuffer size up to a 16-byte boundary.
        struct SpriteVSUniforms {
            float proj[16]; // 0..64
            float basis_x[2]; // 64..72
            float basis_y[2]; // 72..80
            float origin[2]; // 80..88
            float pivot[2]; // 88..96
        };

        static_assert(sizeof(SpriteVSUniforms) == 96);

        struct MeshVSUniforms {
            float view_proj[16];
            float model[16];
            float normal_matrix[16];
        };

        static_assert(sizeof(MeshVSUniforms) == 192);

        void write_float3(std::array<uint8_t, ENGINE_CBUFFER_MAX_BYTES>& out, int32_t offset, const Vector3& v) {
            if (offset >= 0) {
                const float values[3] = {v.x, v.y, v.z};
                std::memcpy(out.data() + offset, values, sizeof(values));
            }
        }
    } // namespace

    void Renderer::render_world_pass(const Matrix4x4& view_proj) {
        render_world_pass_to(m_offscreen_color_target, view_proj);

        debug_textures();
        debug_shaders();
        debug_materials();
        debug_sprite_queue();
        debug_frame_stats();
    }

    void Renderer::render_world_pass_to(SDL_GPUTexture* target, const Matrix4x4& view_proj) {
        if (m_sprite_draw_order_dirty) {
            m_sprite_draw_order.resize(m_sprite_draws.size());
            for (uint32_t i = 0; i < m_sprite_draw_order.size(); ++i) {
                m_sprite_draw_order[i] = i;
            }

            std::sort(m_sprite_draw_order.begin(), m_sprite_draw_order.end(), [this](uint32_t a, uint32_t b) {
                const SpriteDrawData& da = m_sprite_draws[a];
                const SpriteDrawData& db = m_sprite_draws[b];
                if (da.z_index != db.z_index) {
                    return da.z_index < db.z_index;
                }

                if (da.get_shader() != db.get_shader()) {
                    return da.get_shader() < db.get_shader();
                }

                return a < b;
            });

            m_sprite_draw_order_dirty = false;
        }

        SDL_GPUColorTargetInfo ct{};
        ct.texture = target;
        ct.clear_color = to_sdl_color(CLEAR_COLOR);
        ct.load_op = SDL_GPU_LOADOP_CLEAR;
        ct.store_op = SDL_GPU_STOREOP_STORE;

        // Render pass
        {
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(m_command_buffer, &ct, 1, nullptr);
            if (!pass) {
                return;
            }

            if (!m_sprite_draw_order.empty()) {
                SDL_GPUBufferBinding vb{};
                vb.buffer = m_quad_vbo;
                vb.offset = 0;
                SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

                const Shader* bound_shader = nullptr;

                for (const uint32_t index : m_sprite_draw_order) {
                    record_sprite_draw(pass, m_sprite_draws[index], view_proj, bound_shader);
                }
            }

            SDL_EndGPURenderPass(pass);
        }
    }

    void Renderer::render_world_pass_3d(const Matrix4x4& view_proj, const Vector3& camera_position) {
        render_world_pass_3d_to(m_offscreen_targets_3d, m_offscreen_color_target, view_proj, camera_position);

        debug_textures();
        debug_shaders();
        debug_materials();
        debug_sprite_queue();
        debug_mesh_queue();
        debug_shadow_map();
        debug_ssao();
        debug_fog();
        debug_frame_stats();
    }

    void Renderer::render_world_pass_3d_to(RenderTargets3D& targets,
                                           SDL_GPUTexture* ldr_target,
                                           const Matrix4x4& view_proj,
                                           const Vector3& camera_position) {
        if (!targets.is_valid()) {
            return;
        }

        m_camera_position = camera_position;

        if (m_mesh_draw_order_dirty) {
            m_mesh_draw_order.resize(m_mesh_draws.size());
            for (uint32_t i = 0; i < m_mesh_draw_order.size(); ++i) {
                m_mesh_draw_order[i] = i;
            }

            std::sort(m_mesh_draw_order.begin(), m_mesh_draw_order.end(), [this](uint32_t a, uint32_t b) {
                const MeshDrawData& da = m_mesh_draws[a];
                const MeshDrawData& db = m_mesh_draws[b];
                if (da.get_shader() != db.get_shader()) {
                    return da.get_shader() < db.get_shader();
                }

                if (da.mesh != db.mesh) {
                    return da.mesh < db.mesh;
                }

                return a < b;
            });

            m_mesh_draw_order_dirty = false;
        }

        m_light_view_proj = build_light_view_proj(camera_position);
        if (m_cvar_shadows) {
            render_shadow_pass();
        }

        m_current_target_size = Vector2(static_cast<float>(targets.width), static_cast<float>(targets.height));
        m_current_ssao_texture = nullptr;
        const bool fog_enabled = m_cvar_fog && m_scene_light.fog_density > 0.0f;
        if (m_cvar_ssao || fog_enabled) {
            render_prepass(targets, view_proj);
        }
        if (m_cvar_ssao) {
            render_ssao_passes(targets, view_proj);
            m_current_ssao_texture = targets.ssao_blurred;
        }

        SDL_GPUColorTargetInfo ct{};
        ct.texture = targets.hdr_color;
        ct.clear_color = to_sdl_color(HDR_CLEAR_COLOR);
        ct.load_op = SDL_GPU_LOADOP_CLEAR;
        if (targets.is_multisampled()) {
            ct.store_op = SDL_GPU_STOREOP_RESOLVE;
            ct.resolve_texture = targets.hdr_resolved;
        }
        else {
            ct.store_op = SDL_GPU_STOREOP_STORE;
        }

        SDL_GPUDepthStencilTargetInfo dt{};
        dt.texture = targets.depth;
        dt.clear_depth = 1.0f;
        dt.load_op = SDL_GPU_LOADOP_CLEAR;
        dt.store_op = SDL_GPU_STOREOP_DONT_CARE;
        dt.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(m_command_buffer, &ct, 1, &dt);
        if (!pass) {
            return;
        }

        const Shader* bound_shader = nullptr;
        const Mesh* bound_mesh = nullptr;
        for (const uint32_t index : m_mesh_draw_order) {
            record_mesh_draw(pass, m_mesh_draws[index], view_proj, bound_shader, bound_mesh);
        }

        if (m_cvar_sky) {
            record_sky(pass, view_proj);
        }

        SDL_EndGPURenderPass(pass);

        SDL_GPUTexture* post_source = targets.get_resolved_color();
        if (fog_enabled) {
            render_fog_passes(targets, view_proj, post_source);
            post_source = targets.hdr_fogged;
        }

        if (m_cvar_bloom) {
            render_bloom_passes(targets, post_source);
        }

        render_tonemap_pass(post_source, targets.bloom_mips[0], ldr_target);
    }

    void Renderer::render_fog_passes(const RenderTargets3D& targets,
                                     const Matrix4x4& view_proj,
                                     SDL_GPUTexture* hdr_source) {
        const Matrix4x4 inverse_view_proj = view_proj.inverse();

        {
            SDL_GPUColorTargetInfo ct{};
            ct.texture = targets.fog_half;
            ct.load_op = SDL_GPU_LOADOP_DONT_CARE;
            ct.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(m_command_buffer, &ct, 1, nullptr);
            if (!pass) {
                return;
            }

            SDL_BindGPUGraphicsPipeline(pass, m_fog_pipeline);

            SDL_GPUTextureSamplerBinding bindings[2]{};
            bindings[0].texture = targets.prepass_depth;
            bindings[0].sampler = m_blit_sampler;
            bindings[1].texture = m_shadow_map;
            bindings[1].sampler = m_shadow_sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, bindings, 2);

            struct FogUniforms {
                float inverse_view_proj[16];
                float light_view_proj[16];
                float camera_position[3];
                float max_distance;
                float sun_direction[3];
                float step_count;
                float sun_color[3];
                float density;
                float fog_color[3];
                float height;
                float falloff;
                float anisotropy;
                float shadow_strength;
                float shadow_bias;
            } uniforms{};
            std::memcpy(uniforms.inverse_view_proj, inverse_view_proj.data(), sizeof(uniforms.inverse_view_proj));
            std::memcpy(uniforms.light_view_proj, m_light_view_proj.data(), sizeof(uniforms.light_view_proj));
            uniforms.camera_position[0] = m_camera_position.x;
            uniforms.camera_position[1] = m_camera_position.y;
            uniforms.camera_position[2] = m_camera_position.z;
            uniforms.max_distance = FOG_MAX_DISTANCE_METERS;
            uniforms.sun_direction[0] = m_scene_light.direction.x;
            uniforms.sun_direction[1] = m_scene_light.direction.y;
            uniforms.sun_direction[2] = m_scene_light.direction.z;
            uniforms.step_count = static_cast<float>(m_cvar_fog_steps);
            uniforms.sun_color[0] = m_scene_light.color.x;
            uniforms.sun_color[1] = m_scene_light.color.y;
            uniforms.sun_color[2] = m_scene_light.color.z;
            uniforms.density = m_scene_light.fog_density;
            uniforms.fog_color[0] = m_scene_light.fog_color.x;
            uniforms.fog_color[1] = m_scene_light.fog_color.y;
            uniforms.fog_color[2] = m_scene_light.fog_color.z;
            uniforms.height = m_scene_light.fog_height;
            uniforms.falloff = m_scene_light.fog_falloff;
            uniforms.anisotropy = m_scene_light.fog_anisotropy;
            uniforms.shadow_strength = m_cvar_shadows ? 1.0f : 0.0f;
            uniforms.shadow_bias = m_cvar_shadow_bias;
            SDL_PushGPUFragmentUniformData(m_command_buffer, 0, &uniforms, sizeof(uniforms));

            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            SDL_EndGPURenderPass(pass);
        }

        {
            SDL_GPUColorTargetInfo ct{};
            ct.texture = targets.hdr_fogged;
            ct.load_op = SDL_GPU_LOADOP_DONT_CARE;
            ct.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(m_command_buffer, &ct, 1, nullptr);
            if (!pass) {
                return;
            }

            SDL_BindGPUGraphicsPipeline(pass, m_fog_upsample_pipeline);

            SDL_GPUTextureSamplerBinding bindings[3]{};
            bindings[0].texture = hdr_source;
            bindings[0].sampler = m_blit_sampler;
            bindings[1].texture = targets.fog_half;
            bindings[1].sampler = m_blit_sampler;
            bindings[2].texture = targets.prepass_depth;
            bindings[2].sampler = m_blit_sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, bindings, 3);

            struct UpsampleUniforms {
                float inverse_view_proj[16];
                float camera_position[3];
                float pad0;
                float fog_texel_size[2];
                float pad1[2];
            } uniforms{};
            std::memcpy(uniforms.inverse_view_proj, inverse_view_proj.data(), sizeof(uniforms.inverse_view_proj));
            uniforms.camera_position[0] = m_camera_position.x;
            uniforms.camera_position[1] = m_camera_position.y;
            uniforms.camera_position[2] = m_camera_position.z;
            uniforms.fog_texel_size[0] = 2.0f / static_cast<float>(targets.width);
            uniforms.fog_texel_size[1] = 2.0f / static_cast<float>(targets.height);
            SDL_PushGPUFragmentUniformData(m_command_buffer, 0, &uniforms, sizeof(uniforms));

            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            SDL_EndGPURenderPass(pass);
        }
    }

    Matrix4x4 Renderer::build_light_view_proj(const Vector3& camera_position) const {
        const Vector3 direction = m_scene_light.direction.normalized();
        const Vector3 up = std::fabs(direction.y) > 0.99f ? Vector3::forward() : Vector3::up();
        const float texel = SHADOW_EXTENT_METERS / static_cast<float>(SHADOW_MAP_SIZE);

        const Matrix4x4 light_rotation = Matrix4x4::look_at_lh(Vector3::zero(), direction, up);
        Vector3 focus_light_space = light_rotation.transform_point(camera_position);
        focus_light_space.x = std::floor(focus_light_space.x / texel) * texel;
        focus_light_space.y = std::floor(focus_light_space.y / texel) * texel;
        const Vector3 focus = light_rotation.inverse_affine().transform_point(focus_light_space);

        const Vector3 eye = focus - direction * (SHADOW_DEPTH_METERS * 0.5f);
        const Matrix4x4 view = Matrix4x4::look_at_lh(eye, focus, up);
        const Matrix4x4 projection =
            Matrix4x4::orthographic_lh(SHADOW_EXTENT_METERS, SHADOW_EXTENT_METERS, 0.0f, SHADOW_DEPTH_METERS);
        return Matrix4x4::multiply(projection, view);
    }

    void Renderer::render_shadow_pass() {
        SDL_GPUDepthStencilTargetInfo dt{};
        dt.texture = m_shadow_map;
        dt.clear_depth = 1.0f;
        dt.load_op = SDL_GPU_LOADOP_CLEAR;
        dt.store_op = SDL_GPU_STOREOP_STORE;
        dt.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(m_command_buffer, nullptr, 0, &dt);
        if (!pass) {
            return;
        }

        SDL_BindGPUGraphicsPipeline(pass, m_shadow_pipeline);

        const Mesh* bound_mesh = nullptr;
        for (const uint32_t index : m_mesh_draw_order) {
            const MeshDrawData& draw = m_mesh_draws[index];
            if (draw.mesh == nullptr || draw.mesh->get_index_count() == 0) {
                continue;
            }

            if (draw.mesh != bound_mesh) {
                SDL_GPUBufferBinding vb{};
                vb.buffer = draw.mesh->get_vertex_buffer();
                SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

                SDL_GPUBufferBinding ib{};
                ib.buffer = draw.mesh->get_index_buffer();
                SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                bound_mesh = draw.mesh;
            }

            MeshVSUniforms vsu{};
            std::memcpy(vsu.view_proj, m_light_view_proj.data(), sizeof(vsu.view_proj));
            std::memcpy(vsu.model, draw.world_matrix.data(), sizeof(vsu.model));
            SDL_PushGPUVertexUniformData(m_command_buffer, 0, &vsu, sizeof(vsu));

            SDL_DrawGPUIndexedPrimitives(pass, draw.mesh->get_index_count(), 1, 0, 0, 0);
        }

        SDL_EndGPURenderPass(pass);
    }

    void Renderer::render_prepass(const RenderTargets3D& targets, const Matrix4x4& view_proj) {
        SDL_GPUColorTargetInfo ct{};
        ct.texture = targets.prepass_normal;
        ct.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 0.0f};
        ct.load_op = SDL_GPU_LOADOP_CLEAR;
        ct.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPUDepthStencilTargetInfo dt{};
        dt.texture = targets.prepass_depth;
        dt.clear_depth = 1.0f;
        dt.load_op = SDL_GPU_LOADOP_CLEAR;
        dt.store_op = SDL_GPU_STOREOP_STORE;
        dt.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(m_command_buffer, &ct, 1, &dt);
        if (!pass) {
            return;
        }

        SDL_BindGPUGraphicsPipeline(pass, m_prepass_pipeline);

        const Mesh* bound_mesh = nullptr;
        for (const uint32_t index : m_mesh_draw_order) {
            const MeshDrawData& draw = m_mesh_draws[index];
            if (draw.mesh == nullptr || draw.mesh->get_index_count() == 0) {
                continue;
            }

            if (draw.mesh != bound_mesh) {
                SDL_GPUBufferBinding vb{};
                vb.buffer = draw.mesh->get_vertex_buffer();
                SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

                SDL_GPUBufferBinding ib{};
                ib.buffer = draw.mesh->get_index_buffer();
                SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                bound_mesh = draw.mesh;
            }

            MeshVSUniforms vsu{};
            std::memcpy(vsu.view_proj, view_proj.data(), sizeof(vsu.view_proj));
            std::memcpy(vsu.model, draw.world_matrix.data(), sizeof(vsu.model));
            const Matrix4x4 normal_matrix = draw.world_matrix.inverse_affine().transpose();
            std::memcpy(vsu.normal_matrix, normal_matrix.data(), sizeof(vsu.normal_matrix));
            SDL_PushGPUVertexUniformData(m_command_buffer, 0, &vsu, sizeof(vsu));

            SDL_DrawGPUIndexedPrimitives(pass, draw.mesh->get_index_count(), 1, 0, 0, 0);
        }

        SDL_EndGPURenderPass(pass);
    }

    void Renderer::render_ssao_passes(const RenderTargets3D& targets, const Matrix4x4& view_proj) {
        const Matrix4x4 inverse_view_proj = view_proj.inverse();
        const float texel_size[2] = {1.0f / static_cast<float>(targets.width),
                                     1.0f / static_cast<float>(targets.height)};

        {
            SDL_GPUColorTargetInfo ct{};
            ct.texture = targets.ssao_raw;
            ct.load_op = SDL_GPU_LOADOP_DONT_CARE;
            ct.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(m_command_buffer, &ct, 1, nullptr);
            if (!pass) {
                return;
            }

            SDL_BindGPUGraphicsPipeline(pass, m_ssao_pipeline);

            SDL_GPUTextureSamplerBinding bindings[2]{};
            bindings[0].texture = targets.prepass_depth;
            bindings[0].sampler = m_blit_sampler;
            bindings[1].texture = targets.prepass_normal;
            bindings[1].sampler = m_blit_sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, bindings, 2);

            struct SsaoUniforms {
                float view_proj[16];
                float inverse_view_proj[16];
                float camera_position[3];
                float radius;
                float texel_size[2];
                float bias;
                float intensity;
            } uniforms{};
            std::memcpy(uniforms.view_proj, view_proj.data(), sizeof(uniforms.view_proj));
            std::memcpy(uniforms.inverse_view_proj, inverse_view_proj.data(), sizeof(uniforms.inverse_view_proj));
            uniforms.camera_position[0] = m_camera_position.x;
            uniforms.camera_position[1] = m_camera_position.y;
            uniforms.camera_position[2] = m_camera_position.z;
            uniforms.radius = m_cvar_ssao_radius;
            uniforms.texel_size[0] = texel_size[0];
            uniforms.texel_size[1] = texel_size[1];
            uniforms.bias = 0.02f;
            uniforms.intensity = m_cvar_ssao_intensity;
            SDL_PushGPUFragmentUniformData(m_command_buffer, 0, &uniforms, sizeof(uniforms));

            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            SDL_EndGPURenderPass(pass);
        }

        {
            SDL_GPUColorTargetInfo ct{};
            ct.texture = targets.ssao_blurred;
            ct.load_op = SDL_GPU_LOADOP_DONT_CARE;
            ct.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(m_command_buffer, &ct, 1, nullptr);
            if (!pass) {
                return;
            }

            SDL_BindGPUGraphicsPipeline(pass, m_ssao_blur_pipeline);

            SDL_GPUTextureSamplerBinding bindings[2]{};
            bindings[0].texture = targets.ssao_raw;
            bindings[0].sampler = m_blit_sampler;
            bindings[1].texture = targets.prepass_depth;
            bindings[1].sampler = m_blit_sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, bindings, 2);

            struct BlurUniforms {
                float inverse_view_proj[16];
                float camera_position[3];
                float pad0;
                float texel_size[2];
                float pad1[2];
            } uniforms{};
            std::memcpy(uniforms.inverse_view_proj, inverse_view_proj.data(), sizeof(uniforms.inverse_view_proj));
            uniforms.camera_position[0] = m_camera_position.x;
            uniforms.camera_position[1] = m_camera_position.y;
            uniforms.camera_position[2] = m_camera_position.z;
            uniforms.texel_size[0] = texel_size[0];
            uniforms.texel_size[1] = texel_size[1];
            SDL_PushGPUFragmentUniformData(m_command_buffer, 0, &uniforms, sizeof(uniforms));

            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            SDL_EndGPURenderPass(pass);
        }
    }

    void Renderer::record_sky(SDL_GPURenderPass* pass, const Matrix4x4& view_proj) {
        SDL_BindGPUGraphicsPipeline(pass, m_sky_pipeline);

        struct SkyVSUniforms {
            float inverse_view_proj[16];
            float camera_position[3];
            float pad;
        } vsu{};
        const Matrix4x4 inverse_view_proj = view_proj.inverse();
        std::memcpy(vsu.inverse_view_proj, inverse_view_proj.data(), sizeof(vsu.inverse_view_proj));
        vsu.camera_position[0] = m_camera_position.x;
        vsu.camera_position[1] = m_camera_position.y;
        vsu.camera_position[2] = m_camera_position.z;
        SDL_PushGPUVertexUniformData(m_command_buffer, 0, &vsu, sizeof(vsu));

        const float fsu[16] = {m_scene_light.sky.x,
                               m_scene_light.sky.y,
                               m_scene_light.sky.z,
                               0.0f,
                               m_scene_light.ground.x,
                               m_scene_light.ground.y,
                               m_scene_light.ground.z,
                               0.0f,
                               m_scene_light.direction.x,
                               m_scene_light.direction.y,
                               m_scene_light.direction.z,
                               0.0f,
                               m_scene_light.color.x,
                               m_scene_light.color.y,
                               m_scene_light.color.z,
                               0.0f};
        SDL_PushGPUFragmentUniformData(m_command_buffer, 0, fsu, sizeof(fsu));

        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    }

    void Renderer::render_fullscreen_pass(SDL_GPUGraphicsPipeline* pipeline,
                                          SDL_GPUTexture* target,
                                          SDL_GPULoadOp load_op,
                                          SDL_GPUTexture* source,
                                          const void* uniforms,
                                          uint32_t uniforms_size) {
        SDL_GPUColorTargetInfo ct{};
        ct.texture = target;
        ct.load_op = load_op;
        ct.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(m_command_buffer, &ct, 1, nullptr);
        if (!pass) {
            return;
        }

        SDL_BindGPUGraphicsPipeline(pass, pipeline);

        SDL_GPUTextureSamplerBinding binding{};
        binding.texture = source;
        binding.sampler = m_blit_sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);

        SDL_PushGPUFragmentUniformData(m_command_buffer, 0, uniforms, uniforms_size);
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(pass);
    }

    void Renderer::render_bloom_passes(const RenderTargets3D& targets, SDL_GPUTexture* hdr_source) {
        struct BloomUniforms {
            float texel_size[2];
            float param0;
            float param1;
        };

        BloomUniforms prefilter{};
        prefilter.texel_size[0] = 1.0f / static_cast<float>(targets.width);
        prefilter.texel_size[1] = 1.0f / static_cast<float>(targets.height);
        prefilter.param0 = m_cvar_bloom_threshold;
        prefilter.param1 = m_cvar_bloom_threshold * 0.5f;
        render_fullscreen_pass(m_bloom_prefilter_pipeline,
                               targets.bloom_mips[0],
                               SDL_GPU_LOADOP_DONT_CARE,
                               hdr_source,
                               &prefilter,
                               sizeof(prefilter));

        for (uint32_t mip = 1; mip < BLOOM_MIP_COUNT; ++mip) {
            BloomUniforms downsample{};
            downsample.texel_size[0] = 1.0f / static_cast<float>(targets.bloom_widths[mip - 1]);
            downsample.texel_size[1] = 1.0f / static_cast<float>(targets.bloom_heights[mip - 1]);
            render_fullscreen_pass(m_bloom_downsample_pipeline,
                                   targets.bloom_mips[mip],
                                   SDL_GPU_LOADOP_DONT_CARE,
                                   targets.bloom_mips[mip - 1],
                                   &downsample,
                                   sizeof(downsample));
        }

        for (uint32_t mip = BLOOM_MIP_COUNT - 1; mip > 0; --mip) {
            BloomUniforms upsample{};
            upsample.texel_size[0] = 1.0f / static_cast<float>(targets.bloom_widths[mip]);
            upsample.texel_size[1] = 1.0f / static_cast<float>(targets.bloom_heights[mip]);
            upsample.param0 = 1.0f;
            render_fullscreen_pass(m_bloom_upsample_pipeline,
                                   targets.bloom_mips[mip - 1],
                                   SDL_GPU_LOADOP_LOAD,
                                   targets.bloom_mips[mip],
                                   &upsample,
                                   sizeof(upsample));
        }
    }

    void Renderer::render_tonemap_pass(SDL_GPUTexture* hdr_source,
                                       SDL_GPUTexture* bloom_source,
                                       SDL_GPUTexture* ldr_target) {
        SDL_GPUColorTargetInfo ct{};
        ct.texture = ldr_target;
        ct.load_op = SDL_GPU_LOADOP_DONT_CARE;
        ct.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(m_command_buffer, &ct, 1, nullptr);
        if (!pass) {
            return;
        }

        SDL_BindGPUGraphicsPipeline(pass, m_tonemap_pipeline);

        SDL_GPUTextureSamplerBinding bindings[2]{};
        bindings[0].texture = hdr_source;
        bindings[0].sampler = m_blit_sampler;
        bindings[1].texture = bloom_source;
        bindings[1].sampler = m_blit_sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, bindings, 2);

        const float uniforms[4] = {m_cvar_exposure, m_cvar_bloom ? m_cvar_bloom_intensity : 0.0f, 0.0f, 0.0f};
        SDL_PushGPUFragmentUniformData(m_command_buffer, 0, uniforms, sizeof(uniforms));

        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);

        SDL_EndGPURenderPass(pass);
    }

    void Renderer::render_blit_pass() {
        SDL_GPUColorTargetInfo ct{};
        ct.texture = m_game_swap_texture;
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
            ts.texture = m_offscreen_color_target;
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
        ct.texture = m_game_swap_texture;
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
        ct.texture = m_game_swap_texture;
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

    void Renderer::discard_pending_debug_draws() {
        m_pending_debug_line_vertices.clear();
        m_pending_debug_text_vertices.clear();
        m_pending_debug_text_indices.clear();
    }

    void Renderer::record_sprite_draw(SDL_GPURenderPass* pass,
                                      const SpriteDrawData& draw,
                                      const Matrix4x4& view_proj,
                                      const Shader*& bound_shader) {
        if (draw.texture == nullptr || !draw.texture->m_gpu_texture) {
            return;
        }

        const Material& material = draw.material != nullptr ? *draw.material : *m_default_material;
        const Shader* shader = material.get_shader();
        if (!shader || warn_vertex_layout_mismatch(*shader, VertexLayout::Sprite)) {
            return;
        }

        if (shader != bound_shader) {
            SDL_BindGPUGraphicsPipeline(pass, shader->get_pipeline());
            bound_shader = shader;
        }

        const Vector2 basis_x = draw.world_matrix.basis_x * draw.local_size.x;
        const Vector2 basis_y = draw.world_matrix.basis_y * draw.local_size.y;

        SpriteVSUniforms vsu{};
        std::memcpy(vsu.proj, view_proj.data(), sizeof(vsu.proj));
        vsu.basis_x[0] = basis_x.x;
        vsu.basis_x[1] = basis_x.y;
        vsu.basis_y[0] = basis_y.x;
        vsu.basis_y[1] = basis_y.y;
        vsu.origin[0] = draw.world_matrix.origin.x;
        vsu.origin[1] = draw.world_matrix.origin.y;
        vsu.pivot[0] = draw.pivot.x;
        vsu.pivot[1] = draw.pivot.y;
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

        m_stats_sprite_draws += 1;
        SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
    }

    void Renderer::fill_engine_cbuffer(const Shader& shader,
                                       const Texture* texture,
                                       std::array<uint8_t, ENGINE_CBUFFER_MAX_BYTES>& out) const {
        const int32_t texel_off = shader.get_engine_offset(EngineBuiltin::TexelSize);
        if (texel_off >= 0 && texture != nullptr) {
            const uint32_t tex_w = texture->get_width();
            const uint32_t tex_h = texture->get_height();
            const float texel_size[2] = {tex_w > 0 ? 1.0f / static_cast<float>(tex_w) : 0.0f,
                                         tex_h > 0 ? 1.0f / static_cast<float>(tex_h) : 0.0f};
            std::memcpy(out.data() + texel_off, texel_size, sizeof(texel_size));
        }

        const int32_t game_off = shader.get_engine_offset(EngineBuiltin::GameTime);
        if (game_off >= 0) {
            std::memcpy(out.data() + game_off, &m_game_time, sizeof(float));
        }

        const int32_t real_off = shader.get_engine_offset(EngineBuiltin::RealTime);
        if (real_off >= 0) {
            std::memcpy(out.data() + real_off, &m_real_time, sizeof(float));
        }

        write_float3(out, shader.get_engine_offset(EngineBuiltin::LightDirection), m_scene_light.direction);
        write_float3(out, shader.get_engine_offset(EngineBuiltin::LightColor), m_scene_light.color);
        write_float3(out, shader.get_engine_offset(EngineBuiltin::AmbientColor), m_scene_light.ambient);
        write_float3(out, shader.get_engine_offset(EngineBuiltin::CameraPosition), m_camera_position);
        write_float3(out, shader.get_engine_offset(EngineBuiltin::SkyColor), m_scene_light.sky);
        write_float3(out, shader.get_engine_offset(EngineBuiltin::GroundColor), m_scene_light.ground);

        const int32_t light_vp_off = shader.get_engine_offset(EngineBuiltin::LightViewProj);
        if (light_vp_off >= 0) {
            std::memcpy(out.data() + light_vp_off, m_light_view_proj.data(), 16 * sizeof(float));
        }

        const int32_t screen_off = shader.get_engine_offset(EngineBuiltin::ScreenParams);
        if (screen_off >= 0) {
            const float screen_params[4] = {m_current_target_size.x > 0.0f ? 1.0f / m_current_target_size.x : 0.0f,
                                            m_current_target_size.y > 0.0f ? 1.0f / m_current_target_size.y : 0.0f,
                                            m_current_target_size.x,
                                            m_current_target_size.y};
            std::memcpy(out.data() + screen_off, screen_params, sizeof(screen_params));
        }

        const int32_t shadow_off = shader.get_engine_offset(EngineBuiltin::ShadowParams);
        if (shadow_off >= 0) {
            const float shadow_params[4] = {SHADOW_EXTENT_METERS / static_cast<float>(SHADOW_MAP_SIZE),
                                            m_cvar_shadow_bias,
                                            m_cvar_shadows ? 1.0f : 0.0f,
                                            1.0f / static_cast<float>(SHADOW_MAP_SIZE)};
            std::memcpy(out.data() + shadow_off, shadow_params, sizeof(shadow_params));
        }
    }

    bool Renderer::warn_vertex_layout_mismatch(const Shader& shader, VertexLayout expected) {
        if (shader.get_vertex_layout() == expected) {
            return false;
        }

        if (m_layout_warned_shaders.insert(&shader).second) {
            log::renderer.error("Shader '{}' is a {} shader and cannot draw {} geometry",
                                shader.get_path(),
                                vertex_layout_to_string(shader.get_vertex_layout()),
                                vertex_layout_to_string(expected));
        }

        return true;
    }

    void Renderer::push_sprite_fragment_uniforms(const Texture& texture, const Material& material) {
        const Shader* shader = material.get_shader();
        if (!shader) {
            return;
        }

        if (shader->get_engine_slot() != INVALID_SHADER_SLOT) {
            std::array<uint8_t, ENGINE_CBUFFER_MAX_BYTES> engine_cbuffer{};
            fill_engine_cbuffer(*shader, &texture, engine_cbuffer);
            SDL_PushGPUFragmentUniformData(
                m_command_buffer, shader->get_engine_slot(), engine_cbuffer.data(), shader->get_engine_size());
        }

        if (shader->get_material_slot() != INVALID_SHADER_SLOT && material.get_params_size() > 0) {
            SDL_PushGPUFragmentUniformData(
                m_command_buffer, shader->get_material_slot(), material.get_params_data(), material.get_params_size());
        }
    }

    void Renderer::record_mesh_draw(SDL_GPURenderPass* pass,
                                    const MeshDrawData& draw,
                                    const Matrix4x4& view_proj,
                                    const Shader*& bound_shader,
                                    const Mesh*& bound_mesh) {
        if (draw.mesh == nullptr || draw.mesh->get_index_count() == 0) {
            return;
        }

        const Material& material = draw.material != nullptr ? *draw.material : *m_default_mesh_material;
        const Shader* shader = material.get_shader();
        if (!shader || warn_vertex_layout_mismatch(*shader, VertexLayout::Mesh)) {
            return;
        }

        if (shader != bound_shader) {
            SDL_BindGPUGraphicsPipeline(pass, shader->get_pipeline());
            bound_shader = shader;
        }

        if (draw.mesh != bound_mesh) {
            SDL_GPUBufferBinding vb{};
            vb.buffer = draw.mesh->get_vertex_buffer();
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

            SDL_GPUBufferBinding ib{};
            ib.buffer = draw.mesh->get_index_buffer();
            SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            bound_mesh = draw.mesh;
        }

        MeshVSUniforms vsu{};
        std::memcpy(vsu.view_proj, view_proj.data(), sizeof(vsu.view_proj));
        std::memcpy(vsu.model, draw.world_matrix.data(), sizeof(vsu.model));
        const Matrix4x4 normal_matrix = draw.world_matrix.inverse_affine().transpose();
        std::memcpy(vsu.normal_matrix, normal_matrix.data(), sizeof(vsu.normal_matrix));
        SDL_PushGPUVertexUniformData(m_command_buffer, 0, &vsu, sizeof(vsu));

        push_mesh_fragment_uniforms(material);

        for (const ShaderTexture& st : shader->get_textures()) {
            const TextureRef& tex = material.get_texture(st.slot);
            const bool has_texture = tex && tex->m_gpu_texture;

            SDL_GPUTextureSamplerBinding binding{};
            binding.texture = has_texture ? tex->m_gpu_texture : get_mesh_fallback_texture(st.name).m_gpu_texture;
            binding.sampler = (has_texture && tex->m_sampler) ? tex->m_sampler : m_default_sampler;
            SDL_BindGPUFragmentSamplers(pass, st.slot, &binding, 1);
        }

        if (shader->get_shadow_map_slot() != INVALID_SHADER_SLOT) {
            SDL_GPUTextureSamplerBinding binding{};
            binding.texture = m_shadow_map;
            binding.sampler = m_shadow_sampler;
            SDL_BindGPUFragmentSamplers(pass, shader->get_shadow_map_slot(), &binding, 1);
        }

        m_stats_mesh_draws += 1;
        m_stats_mesh_triangles += draw.mesh->get_index_count() / 3;

        if (shader->get_ssao_slot() != INVALID_SHADER_SLOT) {
            SDL_GPUTextureSamplerBinding binding{};
            binding.texture =
                m_current_ssao_texture != nullptr ? m_current_ssao_texture : m_fallback_white_texture->m_gpu_texture;
            binding.sampler = m_blit_sampler;
            SDL_BindGPUFragmentSamplers(pass, shader->get_ssao_slot(), &binding, 1);
        }

        SDL_DrawGPUIndexedPrimitives(pass, draw.mesh->get_index_count(), 1, 0, 0, 0);
    }

    const Texture& Renderer::get_mesh_fallback_texture(std::string_view texture_name) const {
        if (texture_name.find("normal") != std::string_view::npos) {
            return *m_fallback_flat_normal_texture;
        }

        return *m_fallback_white_texture;
    }

    void Renderer::push_mesh_fragment_uniforms(const Material& material) {
        const Shader* shader = material.get_shader();
        if (!shader) {
            return;
        }

        if (shader->get_engine_slot() != INVALID_SHADER_SLOT) {
            std::array<uint8_t, ENGINE_CBUFFER_MAX_BYTES> engine_cbuffer{};
            fill_engine_cbuffer(*shader, nullptr, engine_cbuffer);
            SDL_PushGPUFragmentUniformData(
                m_command_buffer, shader->get_engine_slot(), engine_cbuffer.data(), shader->get_engine_size());
        }

        if (shader->get_material_slot() != INVALID_SHADER_SLOT && material.get_params_size() > 0) {
            SDL_PushGPUFragmentUniformData(
                m_command_buffer, shader->get_material_slot(), material.get_params_data(), material.get_params_size());
        }
    }
} // namespace hob
