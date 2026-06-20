#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL_gpu.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include "engine/math/color.h"
#include "engine/math/matrix4x4.h"
#include "engine/math/vector2.h"
#include "font.h"
#include "material.h"
#include "sprite_draw_data.h"
#include "texture.h"

namespace hob {
    struct GraphicsConfig;
    class SdlContext;
    class Console;

    class Renderer {
        static constexpr Color CLEAR_COLOR = Color(0.17f, 0.18f, 0.47f, 1.0f);

        // 6 verts per line segment (two triangles): 65536 verts = ~10,922 lines/frame.
        static constexpr uint32_t MAX_DEBUG_LINE_VERTICES = 65536;

        // 4 verts and 6 indices per glyph quad.
        static constexpr uint32_t MAX_DEBUG_TEXT_GLYPHS = 4096;
        static constexpr uint32_t MAX_DEBUG_TEXT_VERTICES = MAX_DEBUG_TEXT_GLYPHS * 4;
        static constexpr uint32_t MAX_DEBUG_TEXT_INDICES = MAX_DEBUG_TEXT_GLYPHS * 6;
        static constexpr Color DEBUG_TEXT_SHADOW_COLOR = Color::black();
        static constexpr Vector2 DEBUG_TEXT_SHADOW_OFFSET = Vector2(1.0f, 1.0f);

        static constexpr float DEBUG_FONT_SIZE_PX = 13.0f;

        static constexpr std::string_view BUILTIN_SHADERS_DIR = "builtin/shaders";
        static constexpr std::string_view DEFAULT_SPRITE_SHADER = "builtin/shaders/sprite";
        static constexpr std::string_view DEBUG_FONT_PATH = "builtin/fonts/jetbrains_mono_bold.ttf";

        struct DebugLineVertex {
            Vector2 screen_pos;
            Color color;
        };

        struct DebugTextVertex {
            Vector2 screen_pos;
            Vector2 uv;
            Color color;
        };

        const SdlContext& m_sdl_context;
        SDL_GPUDevice* m_gpu_device = nullptr;
        uint32_t m_logical_width;
        uint32_t m_logical_height;

        bool m_shadercross_initialized = false;
        bool m_is_initialized = false;

        float m_play_time = 0.0f;

        std::unordered_map<std::string, std::weak_ptr<Texture>> m_textures;

        // Projection used when rendering into the offscreen color target. SDL_GPU clip-space
        // ortho mapping (0,0)..(w,h) -> (-1,-1)..(+1,+1) with y-down. Input is logical pixels.
        Matrix4x4 m_offscreen_projection;
        // Projection used when rendering directly into the swapchain. Same logical-pixel input
        // range as m_offscreen_projection, but y-flipped because the swap target has the opposite NDC y convention.
        Matrix4x4 m_swapchain_projection;
        Matrix4x4 m_sprite_view_projection;
        bool m_has_sprite_view_projection = false;

        SDL_GPUCommandBuffer* m_command_buffer = nullptr;
        SDL_GPUTexture* m_swap_texture = nullptr;

        std::vector<SpriteDrawData> m_sprite_draws;
        std::vector<SpriteDrawIndex> m_sprite_draw_id_to_index;
        std::vector<SpriteDrawId> m_sprite_draw_index_to_id;
        std::vector<SpriteDrawId> m_sprite_draw_free_ids;
        std::vector<uint32_t> m_sprite_draw_order;

        std::vector<DebugLineVertex> m_pending_debug_line_vertices;
        std::vector<DebugTextVertex> m_pending_debug_text_vertices;
        std::vector<uint16_t> m_pending_debug_text_indices;

        SDL_GPUTexture* m_offscreen_color = nullptr;
        SDL_GPUTextureFormat m_offscreen_format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        SDL_GPUTextureFormat m_swapchain_format = SDL_GPU_TEXTUREFORMAT_INVALID;

        std::vector<SDL_GPUGraphicsPipeline*> m_sprite_pipelines;
        std::unordered_map<std::string, ShaderId> m_shader_path_to_id;
        SDL_GPUBuffer* m_quad_vbo = nullptr;
        SDL_GPUSampler* m_sprite_sampler = nullptr;

        SDL_GPUGraphicsPipeline* m_blit_pipeline = nullptr;
        SDL_GPUSampler* m_blit_sampler = nullptr;

        SDL_GPUGraphicsPipeline* m_debug_line_pipeline = nullptr;
        SDL_GPUBuffer* m_debug_line_vbo = nullptr;
        SDL_GPUTransferBuffer* m_debug_line_transfer_buffer = nullptr;

        SDL_GPUGraphicsPipeline* m_debug_text_pipeline = nullptr;
        SDL_GPUBuffer* m_debug_text_vbo = nullptr;
        SDL_GPUBuffer* m_debug_text_ibo = nullptr;
        SDL_GPUTransferBuffer* m_debug_text_vbo_transfer = nullptr;
        SDL_GPUTransferBuffer* m_debug_text_ibo_transfer = nullptr;
        SDL_GPUSampler* m_debug_text_sampler = nullptr;
        Font m_debug_font;
        float m_debug_font_baked_inverse_pixel_density = 1.0f;

        bool m_cvar_log_texture_ref = false;
        bool m_cvar_show_texture_refs = false;

        bool m_cvar_log_sprite_queue = false;
        bool m_cvar_show_sprite_queue = false;

    public:
        Renderer(const GraphicsConfig& graphics_config, const SdlContext& sdl_context, Console& console);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        bool is_initialized() const;

        void set_play_time(float time);

        Vector2 get_logical_size() const;

        static Matrix4x4 ortho_top_left(float w, float h);
        static Matrix4x4 ortho_top_left_y_flipped(float w, float h);

        bool acquire_command_buffer();
        void submit_command_buffer();
        void cancel_command_buffer();

        SDL_GPUCommandBuffer* get_command_buffer() const;
        SDL_GPUTexture* get_swap_texture() const;

        void set_sprite_view_projection(const Matrix4x4& view_projection);

        SpriteDrawId register_sprite_draw();
        void unregister_sprite_draw(SpriteDrawId draw_id);
        void update_sprite_draw(SpriteDrawId draw_id, const SpriteDrawData& draw_data);

        void draw_debug_line(const Vector2& screen_start,
                             const Vector2& screen_end,
                             const Color& color,
                             float thickness_pixels);

        void draw_debug_text(const Vector2& screen_pos, std::string_view text, const Color& color, float scale);

        int get_debug_font_line_height() const;

        void render_world_pass();
        void render_blit_pass();
        void render_debug_lines_pass();
        void render_debug_text_pass();

        TextureRef get_or_load_texture(const std::string& path);
        TextureRef create_texture_from_rgba(const void* pixels, uint32_t width, uint32_t height);
        ShaderId get_or_build_sprite_shader(const std::string& path);

        SDL_GPUShader* load_shader(const std::filesystem::path& hlsl_path, SDL_ShaderCross_ShaderStage stage);
        bool upload_buffer(SDL_GPUBuffer* dst_buffer, const void* data, uint32_t size);
        bool upload_texture_rgba(SDL_GPUTexture* dst_texture, const void* pixels, uint32_t width, uint32_t height);

    private:
        friend class Texture;
        friend class Font;

        void release_texture(const Texture& texture);

        bool init_offscreen_target();
        bool init_samplers();
        bool init_quad_vbo();
        bool init_default_sprite_pipeline();
        bool init_blit_pipeline();
        bool init_debug_line_pipeline();
        bool init_debug_text_pipeline();
        bool init_debug_font();

        SDL_GPUGraphicsPipeline* build_sprite_pipeline(const std::string& path, SDL_GPUTextureFormat target_format);

        void record_sprite_draw(SDL_GPURenderPass* pass, const SpriteDrawData& draw, ShaderId& bound_shader);
        void push_sprite_fragment_uniforms(const Texture& texture, const Material& material);

        void register_cvars(Console& console);
        void debug_texture_refs();
        void debug_sprite_queue();
    };
} // namespace hob
