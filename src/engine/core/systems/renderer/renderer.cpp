#include "renderer.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include "engine/core/assert.h"
#include "engine/core/engine_config.h"
#include "engine/core/logging.h"
#include "engine/core/systems/console.h"
#include "engine/core/systems/sdl_context.h"

namespace hob {
    Renderer::Renderer(const GraphicsConfig& graphics_config, const SdlContext& sdl_context, Console& console)
        : m_sdl_context(sdl_context)
        , m_gpu_device(sdl_context.get_gpu_device())
        , m_reference_size(static_cast<float>(graphics_config.reference_width),
                           static_cast<float>(graphics_config.reference_height))
        , m_aspect_mode(graphics_config.aspect_mode)
        , m_render_scale(graphics_config.render_scale > 0.0f ? graphics_config.render_scale : 1.0f)
        , m_pixel_density(sdl_context.get_pixel_density()) {
        // clang-format off
        HOB_CHECK(m_gpu_device, "Renderer init failed: GPU device is null");

        int window_width = 0;
        int window_height = 0;
        m_sdl_context.get_window_size_px(window_width, window_height);
        on_window_resized(window_width, window_height);

        m_swapchain_format = SDL_GetGPUSwapchainTextureFormat(m_gpu_device, m_sdl_context.get_window());

        const bool shadercross_initialized = SDL_ShaderCross_Init();
        HOB_CHECK(shadercross_initialized, "SDL_ShaderCross_Init failed: {}", SDL_GetError());
        m_shadercross_initialized = true;

        const bool offscreen_target_initialized = init_offscreen_target();
        HOB_CHECK(offscreen_target_initialized, "Renderer::init_offscreen_target failed: {}", SDL_GetError());

        const bool samplers_initialized = init_samplers();
        HOB_CHECK(samplers_initialized, "Renderer::init_samplers failed: {}", SDL_GetError());

        const bool quad_vbo_initialized = init_quad_vbo();
        HOB_CHECK(quad_vbo_initialized, "Renderer::init_quad_vbo failed: {}", SDL_GetError());

        const bool default_sprite_pipeline_initialized = init_default_sprite_pipeline();
        HOB_CHECK(default_sprite_pipeline_initialized, "Renderer::init_default_sprite_pipeline failed: {}", SDL_GetError());

        const bool blit_pipeline_initialized = init_blit_pipeline();
        HOB_CHECK(blit_pipeline_initialized, "Renderer::init_blit_pipeline failed: {}", SDL_GetError());

        const bool debug_line_pipeline_initialized = init_debug_line_pipeline();
        HOB_CHECK(debug_line_pipeline_initialized, "Renderer::init_debug_line_pipeline failed: {}", SDL_GetError());

        const bool debug_text_pipeline_initialized = init_debug_text_pipeline();
        HOB_CHECK(debug_text_pipeline_initialized, "Renderer::init_debug_text_pipeline failed: {}", SDL_GetError());

        const bool debug_font_initialized = init_debug_font();
        HOB_CHECK(debug_font_initialized, "Renderer::init_debug_font failed: {}", SDL_GetError());

        register_cvars(console);

        m_initialized = true;

        log::renderer.info("Renderer::Initialise (logical {}x{}, render_scale {})",
                           m_logical_size.x,
                           m_logical_size.y,
                           m_render_scale);
        // clang-format on
    }

    Renderer::~Renderer() {
        // Defensive sweep: with the engine's subsystem destruction order, every TextureRef
        // holder dies before the Renderer, so the weak refs here should already be expired.
        // If anything is still alive, detach it from the renderer and release its GPU handle
        // directly so Texture::~Texture (which would call back into us) becomes a no-op.
        for (auto& [path, weak] : m_textures) {
            if (auto tex = weak.lock()) {
                log::renderer.error(
                    "Renderer::~Renderer: texture '{}' still has {} holder(s) at shutdown — destruction order is wrong",
                    path,
                    tex.use_count() - 1);

                if (tex->m_gpu_texture) {
                    SDL_ReleaseGPUTexture(m_gpu_device, tex->m_gpu_texture);
                    tex->m_gpu_texture = nullptr;
                }

                tex->m_renderer = nullptr;
            }
        }
        m_textures.clear();

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

        // Sprite pipelines: failed builds alias the default-slot pointer, so dedupe by
        // pointer identity before releasing to avoid double-free.
        std::unordered_set<SDL_GPUGraphicsPipeline*> released_pipelines;
        for (SDL_GPUGraphicsPipeline* pipeline : m_sprite_pipelines) {
            if (pipeline && released_pipelines.insert(pipeline).second) {
                SDL_ReleaseGPUGraphicsPipeline(m_gpu_device, pipeline);
            }
        }
        m_sprite_pipelines.clear();
        m_shader_path_to_id.clear();

        if (m_quad_vbo)
            SDL_ReleaseGPUBuffer(m_gpu_device, m_quad_vbo);
        if (m_blit_sampler)
            SDL_ReleaseGPUSampler(m_gpu_device, m_blit_sampler);
        if (m_sprite_sampler)
            SDL_ReleaseGPUSampler(m_gpu_device, m_sprite_sampler);
        if (m_offscreen_color)
            SDL_ReleaseGPUTexture(m_gpu_device, m_offscreen_color);

        if (m_shadercross_initialized) {
            SDL_ShaderCross_Quit();
        }

        log::renderer.info("Renderer::Shutdown");
    }

    void Renderer::set_play_time(float time) {
        m_play_time = time;
    }

    Vector2 Renderer::get_logical_size() const {
        return m_logical_size;
    }

    void Renderer::on_window_resized(int window_width, int window_height) {
        const Vector2 logical = compute_logical_size(window_width, window_height, m_reference_size, m_aspect_mode);
        const float density = m_sdl_context.get_pixel_density();

        if (m_initialized && logical == m_logical_size && density == m_pixel_density) {
            return;
        }

        m_logical_size = logical;
        m_pixel_density = density;
        m_offscreen_projection = ortho_top_left(logical.x, logical.y);
        m_swapchain_projection = ortho_top_left_y_flipped(logical.x, logical.y);

        if (m_initialized && !init_offscreen_target()) {
            log::renderer.error("Renderer::on_window_resized: failed to recreate offscreen target");
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
        m_swap_texture = nullptr;

        const bool ok = SDL_WaitAndAcquireGPUSwapchainTexture(
                            m_command_buffer, m_sdl_context.get_window(), &m_swap_texture, nullptr, nullptr) &&
                        m_swap_texture != nullptr;
        return ok;
    }

    void Renderer::submit_command_buffer() {
        SDL_SubmitGPUCommandBuffer(m_command_buffer);
        m_command_buffer = nullptr;
        m_swap_texture = nullptr;
        m_has_sprite_view_projection = false;
    }

    void Renderer::cancel_command_buffer() {
        SDL_CancelGPUCommandBuffer(m_command_buffer);
        m_command_buffer = nullptr;
        m_swap_texture = nullptr;
        m_has_sprite_view_projection = false;
    }

    SDL_GPUCommandBuffer* Renderer::get_command_buffer() const {
        return m_command_buffer;
    }

    SDL_GPUTexture* Renderer::get_swap_texture() const {
        return m_swap_texture;
    }

    void Renderer::set_sprite_view_projection(const Matrix4x4& view_projection) {
        m_sprite_view_projection = view_projection;
        m_has_sprite_view_projection = true;
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
    }

    void Renderer::update_sprite_draw(SpriteDrawId draw_id, const SpriteDrawData& draw_data) {
        if (draw_id < 0 || draw_id >= static_cast<SpriteDrawId>(m_sprite_draw_id_to_index.size())) {
            return;
        }

        const SpriteDrawIndex index = m_sprite_draw_id_to_index[draw_id];
        if (index == INVALID_SPRITE_DRAW_INDEX) {
            return;
        }

        m_sprite_draws[index] = draw_data;
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

        m_pending_debug_line_vertices.push_back({p0, color});
        m_pending_debug_line_vertices.push_back({p1, color});
        m_pending_debug_line_vertices.push_back({p2, color});
        m_pending_debug_line_vertices.push_back({p2, color});
        m_pending_debug_line_vertices.push_back({p1, color});
        m_pending_debug_line_vertices.push_back({p3, color});
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

                    m_pending_debug_text_vertices.push_back({Vector2(x0, y0), Vector2(g->u0, g->v0), glyph_color});
                    m_pending_debug_text_vertices.push_back({Vector2(x1, y0), Vector2(g->u1, g->v0), glyph_color});
                    m_pending_debug_text_vertices.push_back({Vector2(x0, y1), Vector2(g->u0, g->v1), glyph_color});
                    m_pending_debug_text_vertices.push_back({Vector2(x1, y1), Vector2(g->u1, g->v1), glyph_color});

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

    int Renderer::get_debug_font_line_height() const {
        // The atlas is baked scaled by pixel_density for crispness; report the nominal (DEBUG_FONT_SIZE_PX) height.
        return static_cast<int>(
            std::round(static_cast<float>(m_debug_font.get_line_height()) * m_debug_font_baked_inverse_pixel_density));
    }
} // namespace hob
