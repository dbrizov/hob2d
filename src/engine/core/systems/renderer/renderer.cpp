#include "renderer.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include "engine/core/assert.h"
#include "engine/core/engine_config.h"
#include "engine/core/logging.h"
#include "engine/core/systems/console.h"
#include "engine/core/systems/window.h"

namespace hob {
    Renderer::Renderer(const GraphicsConfig& graphics_config, SDL_GPUDevice* gpu_device, const Window& main_window)
        : m_gpu_device(gpu_device)
        , m_reference_size(static_cast<float>(graphics_config.reference_width),
                           static_cast<float>(graphics_config.reference_height))
        , m_aspect_mode(graphics_config.aspect_mode)
        , m_render_scale(graphics_config.render_scale > 0.0f ? graphics_config.render_scale : 1.0f)
        , m_pixel_density(1.0f)
        , m_main_window(&main_window)
        , m_game_window(&main_window)
        , m_default_sampler_desc{graphics_config.default_texture_filter, graphics_config.default_texture_wrap} {
        // clang-format off
        HOB_CHECK(m_gpu_device, "Renderer init failed: GPU device is null");

        m_swapchain_format = SDL_GetGPUSwapchainTextureFormat(m_gpu_device, m_main_window->get_window());

        const uint32_t magenta = 0xFFFF00FFu;
        m_fallback_texture = create_texture_from_rgba(&magenta, 1, 1);
        HOB_CHECK(m_fallback_texture, "Renderer: failed to create fallback texture");

        const uint32_t white = 0xFFFFFFFFu;
        m_fallback_white_texture = create_texture_from_rgba(&white, 1, 1);
        HOB_CHECK(m_fallback_white_texture, "Renderer: failed to create white fallback texture");

        const uint32_t flat_normal = 0xFFFF8080u;
        m_fallback_flat_normal_texture = create_texture_from_rgba(&flat_normal, 1, 1);
        HOB_CHECK(m_fallback_flat_normal_texture, "Renderer: failed to create flat normal fallback texture");

        const bool shadercross_initialized = SDL_ShaderCross_Init();
        HOB_CHECK(shadercross_initialized, "SDL_ShaderCross_Init failed: {}", SDL_GetError());
        m_shadercross_initialized = true;

        m_depth_format = probe_depth_format();
        m_hdr_format = probe_hdr_format();
        m_msaa_sample_count = probe_msaa_sample_count(m_cvar_msaa);

        const bool samplers_initialized = init_samplers();
        HOB_CHECK(samplers_initialized, "Renderer::init_samplers failed: {}", SDL_GetError());

        const bool quad_vbo_initialized = init_quad_vbo();
        HOB_CHECK(quad_vbo_initialized, "Renderer::init_quad_vbo failed: {}", SDL_GetError());

        const bool default_sprite_pipeline_initialized = init_default_sprite_pipeline();
        HOB_CHECK(default_sprite_pipeline_initialized, "Renderer::init_default_sprite_pipeline failed: {}", SDL_GetError());

        const bool default_mesh_pipeline_initialized = init_default_mesh_pipeline();
        HOB_CHECK(default_mesh_pipeline_initialized, "Renderer::init_default_mesh_pipeline failed: {}", SDL_GetError());

        const bool blit_pipeline_initialized = init_blit_pipeline();
        HOB_CHECK(blit_pipeline_initialized, "Renderer::init_blit_pipeline failed: {}", SDL_GetError());

        const bool tonemap_pipeline_initialized = init_tonemap_pipeline();
        HOB_CHECK(tonemap_pipeline_initialized, "Renderer::init_tonemap_pipeline failed: {}", SDL_GetError());

        const bool sky_pipeline_initialized = init_sky_pipeline();
        HOB_CHECK(sky_pipeline_initialized, "Renderer::init_sky_pipeline failed: {}", SDL_GetError());

        const bool shadow_resources_initialized = init_shadow_resources();
        HOB_CHECK(shadow_resources_initialized, "Renderer::init_shadow_resources failed: {}", SDL_GetError());

        const bool prepass_pipeline_initialized = init_prepass_pipeline();
        HOB_CHECK(prepass_pipeline_initialized, "Renderer::init_prepass_pipeline failed: {}", SDL_GetError());

        const bool ssao_pipelines_initialized = init_ssao_pipelines();
        HOB_CHECK(ssao_pipelines_initialized, "Renderer::init_ssao_pipelines failed: {}", SDL_GetError());

        const bool bloom_pipelines_initialized = init_bloom_pipelines();
        HOB_CHECK(bloom_pipelines_initialized, "Renderer::init_bloom_pipelines failed: {}", SDL_GetError());

        const bool fog_pipelines_initialized = init_fog_pipelines();
        HOB_CHECK(fog_pipelines_initialized, "Renderer::init_fog_pipelines failed: {}", SDL_GetError());

        const bool debug_line_pipeline_initialized = init_debug_line_pipeline();
        HOB_CHECK(debug_line_pipeline_initialized, "Renderer::init_debug_line_pipeline failed: {}", SDL_GetError());

        const bool debug_text_pipeline_initialized = init_debug_text_pipeline();
        HOB_CHECK(debug_text_pipeline_initialized, "Renderer::init_debug_text_pipeline failed: {}", SDL_GetError());

        m_initialized = true;

        log::renderer.info("Renderer::Initialise (render scale {})", m_render_scale);
        // clang-format on
    }

    Renderer::~Renderer() {
        // Debug font owns its atlas texture; release before the GPU device goes away.
        m_debug_font.shutdown();

        if (m_debug_text_sampler)
            SDL_ReleaseGPUSampler(m_gpu_device, m_debug_text_sampler);
        if (m_debug_text_ibo_transfer)
            SDL_ReleaseGPUTransferBuffer(m_gpu_device, m_debug_text_ibo_transfer);
        if (m_debug_text_vbo_transfer)
            SDL_ReleaseGPUTransferBuffer(m_gpu_device, m_debug_text_vbo_transfer);
        if (m_debug_text_ibo)
            SDL_ReleaseGPUBuffer(m_gpu_device, m_debug_text_ibo);
        if (m_debug_text_vbo)
            SDL_ReleaseGPUBuffer(m_gpu_device, m_debug_text_vbo);
        if (m_debug_text_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_debug_text_pipeline);

        if (m_debug_line_transfer_buffer)
            SDL_ReleaseGPUTransferBuffer(m_gpu_device, m_debug_line_transfer_buffer);
        if (m_debug_line_vbo)
            SDL_ReleaseGPUBuffer(m_gpu_device, m_debug_line_vbo);
        if (m_debug_line_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_debug_line_pipeline);
        if (m_blit_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_blit_pipeline);

        // Materials first: they hold shader/texture refs the leak checks below would otherwise miscount.
        release_materials();
        release_shaders();
        release_textures();
        release_meshes();

        if (m_upload_transfer_buffer)
            SDL_ReleaseGPUTransferBuffer(m_gpu_device, m_upload_transfer_buffer);
        if (m_quad_vbo)
            SDL_ReleaseGPUBuffer(m_gpu_device, m_quad_vbo);
        if (m_blit_sampler)
            SDL_ReleaseGPUSampler(m_gpu_device, m_blit_sampler);
        for (auto& [key, sampler] : m_samplers) {
            SDL_ReleaseGPUSampler(m_gpu_device, sampler);
        }
        release_targets_3d(m_offscreen_targets_3d);
        if (m_tonemap_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_tonemap_pipeline);
        if (m_sky_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_sky_pipeline);
        if (m_shadow_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_shadow_pipeline);
        if (m_prepass_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_prepass_pipeline);
        if (m_ssao_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_ssao_pipeline);
        if (m_ssao_blur_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_ssao_blur_pipeline);
        if (m_bloom_prefilter_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_bloom_prefilter_pipeline);
        if (m_bloom_downsample_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_bloom_downsample_pipeline);
        if (m_bloom_upsample_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_bloom_upsample_pipeline);
        if (m_fog_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_fog_pipeline);
        if (m_fog_upsample_pipeline)
            SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, m_fog_upsample_pipeline);
        if (m_shadow_sampler)
            SDL_ReleaseGPUSampler(m_gpu_device, m_shadow_sampler);
        if (m_shadow_map)
            SDL_ReleaseGPUTexture(m_gpu_device, m_shadow_map);
        if (m_offscreen_color_target)
            SDL_ReleaseGPUTexture(m_gpu_device, m_offscreen_color_target);

        if (m_shadercross_initialized) {
            SDL_ShaderCross_Quit();
        }

        log::renderer.info("Renderer::Shutdown");
    }

    void Renderer::set_time(float game_time, float real_time) {
        m_stats_frame_seconds = real_time - m_real_time;
        m_stats_sprite_draws = 0;
        m_stats_mesh_draws = 0;
        m_stats_mesh_triangles = 0;
        m_game_time = game_time;
        m_real_time = real_time;
    }

    SDL_GPUDevice* Renderer::get_gpu_device() const {
        return m_gpu_device;
    }

    Vector2 Renderer::get_logical_size() const {
        return m_logical_size;
    }

    Vector2 Renderer::get_reference_size() const {
        return m_reference_size;
    }

    const Window* Renderer::get_main_window() const {
        return m_main_window;
    }

    const Window* Renderer::get_game_window() const {
        return m_game_window;
    }

    void Renderer::set_game_window(const Window* window) {
        m_game_window = window;
    }

    void Renderer::on_window_resized(int32_t window_width, int32_t window_height) {
        if (m_game_window == nullptr) {
            return;
        }

        const Vector2 logical = compute_logical_size(window_width, window_height, m_reference_size, m_aspect_mode);
        const float density = m_game_window->get_pixel_density();

        const bool size_changed = logical != m_logical_size;
        const bool density_changed = density != m_pixel_density;

        if (!size_changed && !density_changed && m_offscreen_color_target != nullptr && m_debug_font.is_initialized()) {
            return;
        }

        m_logical_size = logical;
        m_pixel_density = density;
        m_offscreen_projection = ortho_top_left(logical.x, logical.y);
        m_swapchain_projection = ortho_top_left_y_flipped(logical.x, logical.y);

        if (!init_offscreen_targets()) {
            log::renderer.error("Renderer::on_window_resized: failed to recreate offscreen targets");
        }

        if (density_changed || !m_debug_font.is_initialized()) {
            m_debug_font.shutdown();
            if (!init_debug_font()) {
                log::renderer.error("Renderer::on_window_resized: failed to bake debug font");
            }
        }
    }

    Matrix4x4 Renderer::ortho_top_left(float w, float h) {
        Matrix4x4 out;
        out.m[0] = 2.0f / w;
        out.m[5] = 2.0f / h;
        out.m[10] = 1.0f;
        out.m[12] = -1.0f;
        out.m[13] = -1.0f;
        out.m[15] = 1.0f;
        return out;
    }

    Matrix4x4 Renderer::perspective_offscreen(float fov_y_rad, float aspect, float near_plane, float far_plane) {
        Matrix4x4 out = Matrix4x4::perspective_lh(fov_y_rad, aspect, near_plane, far_plane);
        out.m[5] = -out.m[5];
        return out;
    }

    Matrix4x4 Renderer::ortho_top_left_y_flipped(float w, float h) {
        Matrix4x4 out;
        out.m[0] = 2.0f / w;
        out.m[5] = -2.0f / h;
        out.m[10] = 1.0f;
        out.m[12] = -1.0f;
        out.m[13] = 1.0f;
        out.m[15] = 1.0f;
        return out;
    }

    bool Renderer::acquire_command_buffer() {
        m_command_buffer = SDL_AcquireGPUCommandBuffer(m_gpu_device);
        m_main_swap_texture = nullptr;
        m_game_swap_texture = nullptr;

        if (m_command_buffer == nullptr) {
            log::renderer.error("SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError());
            return false;
        }

        if (!SDL_WaitAndAcquireGPUSwapchainTexture(
                m_command_buffer, m_main_window->get_window(), &m_main_swap_texture, nullptr, nullptr)) {
            log::renderer.error("SDL_WaitAndAcquireGPUSwapchainTexture (main window) failed: {}", SDL_GetError());
            return false;
        }

        // Swapchain == nullptr means the window is minimized or occluded
        if (m_main_swap_texture == nullptr) {
            return false;
        }

        if (m_game_window == m_main_window) {
            m_game_swap_texture = m_main_swap_texture;
        }
        else if (m_game_window != nullptr &&
                 !SDL_WaitAndAcquireGPUSwapchainTexture(
                     m_command_buffer, m_game_window->get_window(), &m_game_swap_texture, nullptr, nullptr)) {
            log::renderer.error("SDL_WaitAndAcquireGPUSwapchainTexture (game window) failed: {}", SDL_GetError());
        }

        return true;
    }

    void Renderer::submit_command_buffer() {
        SDL_SubmitGPUCommandBuffer(m_command_buffer);
        m_command_buffer = nullptr;
        m_main_swap_texture = nullptr;
        m_game_swap_texture = nullptr;
    }

    void Renderer::cancel_command_buffer() {
        if (m_command_buffer != nullptr) {
            SDL_CancelGPUCommandBuffer(m_command_buffer);
        }

        m_command_buffer = nullptr;
        m_main_swap_texture = nullptr;
        m_game_swap_texture = nullptr;
    }

    SDL_GPUCommandBuffer* Renderer::get_command_buffer() const {
        return m_command_buffer;
    }

    SDL_GPUTexture* Renderer::get_main_swap_texture() const {
        return m_main_swap_texture;
    }

    SDL_GPUTexture* Renderer::get_game_swap_texture() const {
        return m_game_swap_texture;
    }

    SDL_GPUTextureFormat Renderer::get_swapchain_format() const {
        return m_swapchain_format;
    }

    SDL_GPUTextureFormat Renderer::get_offscreen_format() const {
        return m_offscreen_format;
    }

    SDL_GPUTextureFormat Renderer::get_depth_format() const {
        return m_depth_format;
    }

    SDL_GPUTextureFormat Renderer::get_hdr_format() const {
        return m_hdr_format;
    }

    SDL_GPUSampleCount Renderer::get_msaa_sample_count() const {
        return m_msaa_sample_count;
    }

    SpriteDrawId Renderer::register_sprite_draw() {
        const SpriteDrawIndex index = static_cast<SpriteDrawIndex>(m_sprite_draws.size());
        m_sprite_draws.emplace_back();

        SpriteDrawId draw_id;
        if (!m_sprite_draw_free_ids.empty()) {
            draw_id = m_sprite_draw_free_ids.back();
            m_sprite_draw_free_ids.pop_back();
            m_sprite_draw_id_to_index[draw_id] = index;
        }
        else {
            draw_id = static_cast<SpriteDrawId>(m_sprite_draw_id_to_index.size());
            m_sprite_draw_id_to_index.push_back(index);
        }

        m_sprite_draw_index_to_id.push_back(draw_id);
        m_sprite_draw_order_dirty = true;
        return draw_id;
    }

    void Renderer::unregister_sprite_draw(SpriteDrawId draw_id) {
        if (draw_id < 0 || draw_id >= static_cast<SpriteDrawId>(m_sprite_draw_id_to_index.size())) {
            return;
        }

        const SpriteDrawIndex index = m_sprite_draw_id_to_index[draw_id];
        if (index == INVALID_SPRITE_DRAW_INDEX) {
            return;
        }

        // Swap-pop the packed draw, then fix up the moved element's id->index mapping.
        const SpriteDrawIndex last_index = static_cast<SpriteDrawIndex>(m_sprite_draws.size()) - 1;
        if (index != last_index) {
            m_sprite_draws[index] = std::move(m_sprite_draws[last_index]);
            const SpriteDrawId moved_id = m_sprite_draw_index_to_id[last_index];
            m_sprite_draw_index_to_id[index] = moved_id;
            m_sprite_draw_id_to_index[moved_id] = index;
        }

        m_sprite_draws.pop_back();
        m_sprite_draw_index_to_id.pop_back();
        m_sprite_draw_id_to_index[draw_id] = INVALID_SPRITE_DRAW_INDEX;
        m_sprite_draw_free_ids.push_back(draw_id);
        m_sprite_draw_order_dirty = true;
    }

    void Renderer::update_sprite_draw(SpriteDrawId draw_id, SpriteDrawData draw_data) {
        if (draw_id < 0 || draw_id >= static_cast<SpriteDrawId>(m_sprite_draw_id_to_index.size())) {
            return;
        }

        const SpriteDrawIndex index = m_sprite_draw_id_to_index[draw_id];
        if (index == INVALID_SPRITE_DRAW_INDEX) {
            return;
        }

        SpriteDrawData& slot = m_sprite_draws[index];

        // Only z_index and shader feed the draw order; a sprite that merely moved keeps its slot.
        if (slot.z_index != draw_data.z_index || slot.get_shader() != draw_data.get_shader()) {
            m_sprite_draw_order_dirty = true;
        }

        slot = std::move(draw_data);
    }

    const SpriteDrawData* Renderer::get_sprite_draw(SpriteDrawId draw_id) const {
        if (draw_id < 0 || draw_id >= static_cast<SpriteDrawId>(m_sprite_draw_id_to_index.size())) {
            return nullptr;
        }

        const SpriteDrawIndex index = m_sprite_draw_id_to_index[draw_id];
        if (index == INVALID_SPRITE_DRAW_INDEX) {
            return nullptr;
        }

        return &m_sprite_draws[index];
    }

    MeshDrawId Renderer::register_mesh_draw() {
        MeshDrawId draw_id;
        if (!m_mesh_draw_free_ids.empty()) {
            draw_id = m_mesh_draw_free_ids.back();
            m_mesh_draw_free_ids.pop_back();
        }
        else {
            draw_id = static_cast<MeshDrawId>(m_mesh_draw_id_to_index.size());
            m_mesh_draw_id_to_index.push_back(INVALID_MESH_DRAW_INDEX);
        }

        m_mesh_draw_id_to_index[draw_id] = static_cast<MeshDrawIndex>(m_mesh_draws.size());
        m_mesh_draws.emplace_back();
        m_mesh_draw_index_to_id.push_back(draw_id);
        m_mesh_draw_order_dirty = true;
        return draw_id;
    }

    void Renderer::unregister_mesh_draw(MeshDrawId draw_id) {
        if (draw_id < 0 || draw_id >= static_cast<MeshDrawId>(m_mesh_draw_id_to_index.size())) {
            return;
        }

        const MeshDrawIndex index = m_mesh_draw_id_to_index[draw_id];
        if (index == INVALID_MESH_DRAW_INDEX) {
            return;
        }

        const MeshDrawIndex last_index = static_cast<MeshDrawIndex>(m_mesh_draws.size()) - 1;
        if (index != last_index) {
            m_mesh_draws[index] = std::move(m_mesh_draws[last_index]);
            const MeshDrawId moved_id = m_mesh_draw_index_to_id[last_index];
            m_mesh_draw_index_to_id[index] = moved_id;
            m_mesh_draw_id_to_index[moved_id] = index;
        }

        m_mesh_draws.pop_back();
        m_mesh_draw_index_to_id.pop_back();
        m_mesh_draw_id_to_index[draw_id] = INVALID_MESH_DRAW_INDEX;
        m_mesh_draw_free_ids.push_back(draw_id);
        m_mesh_draw_order_dirty = true;
    }

    void Renderer::update_mesh_draw(MeshDrawId draw_id, MeshDrawData draw_data) {
        if (draw_id < 0 || draw_id >= static_cast<MeshDrawId>(m_mesh_draw_id_to_index.size())) {
            return;
        }

        const MeshDrawIndex index = m_mesh_draw_id_to_index[draw_id];
        if (index == INVALID_MESH_DRAW_INDEX) {
            return;
        }

        MeshDrawData& slot = m_mesh_draws[index];
        if (slot.get_shader() != draw_data.get_shader() || slot.mesh != draw_data.mesh) {
            m_mesh_draw_order_dirty = true;
        }

        slot = std::move(draw_data);
    }

    const MeshDrawData* Renderer::get_mesh_draw(MeshDrawId draw_id) const {
        if (draw_id < 0 || draw_id >= static_cast<MeshDrawId>(m_mesh_draw_id_to_index.size())) {
            return nullptr;
        }

        const MeshDrawIndex index = m_mesh_draw_id_to_index[draw_id];
        if (index == INVALID_MESH_DRAW_INDEX) {
            return nullptr;
        }

        return &m_mesh_draws[index];
    }

    const SceneLight& Renderer::get_scene_light() const {
        return m_scene_light;
    }

    void Renderer::set_scene_light(const SceneLight& light) {
        m_scene_light = light;
        m_scene_light.direction = light.direction.normalized();
    }

    void Renderer::draw_debug_line(const Vector2& screen_start,
                                   const Vector2& screen_end,
                                   const Color& color,
                                   float thickness_pixels) {
        const Vector2 delta = screen_end - screen_start;
        const float len = delta.length();
        if (len <= 0.0f) {
            return;
        }

        const float half = thickness_pixels * 0.5f;
        const Vector2 perp = Vector2(-delta.y, delta.x) / len;
        const Vector2 offset = perp * half;

        const Vector2 p0 = screen_start + offset;
        const Vector2 p1 = screen_start - offset;
        const Vector2 p2 = screen_end + offset;
        const Vector2 p3 = screen_end - offset;

        m_pending_debug_line_vertices.emplace_back(p0, color);
        m_pending_debug_line_vertices.emplace_back(p1, color);
        m_pending_debug_line_vertices.emplace_back(p2, color);
        m_pending_debug_line_vertices.emplace_back(p2, color);
        m_pending_debug_line_vertices.emplace_back(p1, color);
        m_pending_debug_line_vertices.emplace_back(p3, color);
    }

    void Renderer::draw_debug_text(const Vector2& screen_pos, std::string_view text, const Color& color, float scale) {
        if (!m_debug_font.is_initialized() || text.empty()) {
            return;
        }

        // The atlas is baked scaled by pixel_density for crispness; down-scale by the baked inverse
        // density so glyphs render at the nominal DEBUG_FONT_SIZE_PX before the caller's logical scale.
        const float glyph_scale = scale * m_debug_font_baked_inverse_pixel_density;

        auto emit_pass = [&](const Vector2& origin, const Color& glyph_color) {
            float pen_x = origin.x;
            const float pen_y = origin.y;

            for (const char c : text) {
                const uint32_t cp = static_cast<unsigned char>(c);
                const Glyph* g = m_debug_font.get_glyph(cp);
                if (!g) {
                    continue;
                }

                // Don't tessellate quads for whitespace glyphs (no ink). Still advances the pen.
                if (g->width > 0 && g->height > 0) {
                    if (m_pending_debug_text_vertices.size() + 4 > MAX_DEBUG_TEXT_VERTICES) {
                        break;
                    }

                    const float x0 = pen_x + static_cast<float>(g->offset_x) * glyph_scale;
                    const float y0 = pen_y + static_cast<float>(g->offset_y) * glyph_scale;
                    const float x1 = x0 + static_cast<float>(g->width) * glyph_scale;
                    const float y1 = y0 + static_cast<float>(g->height) * glyph_scale;

                    const uint16_t base = static_cast<uint16_t>(m_pending_debug_text_vertices.size());

                    m_pending_debug_text_vertices.emplace_back(Vector2(x0, y0), Vector2(g->u0, g->v0), glyph_color);
                    m_pending_debug_text_vertices.emplace_back(Vector2(x1, y0), Vector2(g->u1, g->v0), glyph_color);
                    m_pending_debug_text_vertices.emplace_back(Vector2(x0, y1), Vector2(g->u0, g->v1), glyph_color);
                    m_pending_debug_text_vertices.emplace_back(Vector2(x1, y1), Vector2(g->u1, g->v1), glyph_color);

                    m_pending_debug_text_indices.push_back(base + 0);
                    m_pending_debug_text_indices.push_back(base + 2);
                    m_pending_debug_text_indices.push_back(base + 1);
                    m_pending_debug_text_indices.push_back(base + 1);
                    m_pending_debug_text_indices.push_back(base + 2);
                    m_pending_debug_text_indices.push_back(base + 3);
                }

                pen_x += static_cast<float>(g->advance) * glyph_scale;
            }
        };

        // Shadow first (all glyphs). The shadow offset is in nominal/logical pixels,
        // scaled by the caller's logical scale to stay proportional to the glyphs.
        if (DEBUG_TEXT_SHADOW_OFFSET.x != 0.0f || DEBUG_TEXT_SHADOW_OFFSET.y != 0.0f) {
            Color shadow = DEBUG_TEXT_SHADOW_COLOR;
            shadow.a *= color.a;
            const Vector2 shadow_offset = DEBUG_TEXT_SHADOW_OFFSET * scale;
            const Vector2 shadow_origin = screen_pos + shadow_offset;
            emit_pass(shadow_origin, shadow);
        }

        // Then text on top (all glyphs).
        emit_pass(screen_pos, color);
    }

    int32_t Renderer::get_debug_font_line_height() const {
        // The atlas is baked scaled by pixel_density for crispness; report the nominal (DEBUG_FONT_SIZE_PX) height.
        return static_cast<int32_t>(
            std::round(static_cast<float>(m_debug_font.get_line_height()) * m_debug_font_baked_inverse_pixel_density));
    }
} // namespace hob
