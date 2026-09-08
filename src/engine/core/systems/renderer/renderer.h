#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <SDL3/SDL_gpu.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include "engine/core/aspect_mode.h"
#include "engine/core/string_hash.h"
#include "engine/math/color.h"
#include "engine/math/matrix4x4.h"
#include "engine/math/vector2.h"
#include "engine/math/vector3.h"
#include "font.h"
#include "material.h"
#include "mesh.h"
#include "mesh_draw_data.h"
#include "render_targets_3d.h"
#include "sampler.h"
#include "shader.h"
#include "shader_reflection.h"
#include "sprite_draw_data.h"
#include "texture.h"

namespace hob {
    struct GraphicsConfig;
    class SdlContext;
    class Console;
    class Window;

    struct SceneLight {
        Vector3 direction = Vector3(0.3f, -1.0f, 0.5f).normalized();
        Vector3 color = Vector3::one();
        Vector3 ambient = Vector3(0.25f, 0.25f, 0.25f);
        Vector3 sky = Vector3(0.35f, 0.4f, 0.5f);
        Vector3 ground = Vector3(0.15f, 0.13f, 0.12f);
        float fog_density = 0.0f;
        float fog_height = 0.0f;
        float fog_falloff = 8.0f;
        float fog_anisotropy = 0.5f;
        Vector3 fog_color = Vector3(0.3f, 0.35f, 0.45f);
    };

    class Renderer {
        friend class Font;
        friend class Texture;

        static constexpr Color CLEAR_COLOR = Color(0.180f, 0.192f, 0.212f, 1.0f);
        static constexpr Color HDR_CLEAR_COLOR = Color(0.027f, 0.031f, 0.038f, 1.0f);
        static constexpr uint32_t SHADOW_MAP_SIZE = 2048;
        static constexpr float SHADOW_EXTENT_METERS = 60.0f;
        static constexpr float SHADOW_DEPTH_METERS = 120.0f;
        static constexpr std::string_view SHADOW_MAP_TEXTURE_NAME = "shadow_map";
        static constexpr std::string_view SSAO_TEXTURE_NAME = "ssao_tex";
        static constexpr SDL_GPUTextureFormat PREPASS_NORMAL_FORMAT = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        static constexpr SDL_GPUTextureFormat SSAO_FORMAT = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        static constexpr float FOG_MAX_DISTANCE_METERS = 80.0f;

        // 6 verts per line segment (two triangles): 65536 verts = ~10,922 lines/frame.
        static constexpr uint32_t MAX_DEBUG_LINE_VERTICES = 65536;

        // 4 verts and 6 indices per glyph quad.
        static constexpr uint32_t MAX_DEBUG_TEXT_GLYPHS = 4096;
        static constexpr uint32_t MAX_DEBUG_TEXT_VERTICES = MAX_DEBUG_TEXT_GLYPHS * 4;
        static constexpr uint32_t MAX_DEBUG_TEXT_INDICES = MAX_DEBUG_TEXT_GLYPHS * 6;
        static constexpr Color DEBUG_TEXT_SHADOW_COLOR = Color::black();
        static constexpr Vector2 DEBUG_TEXT_SHADOW_OFFSET = Vector2(1.0f, 1.0f);

        static constexpr float DEBUG_FONT_SIZE_PX = 13.0f;

        // Relative to the engine assets root.
        static constexpr std::string_view BUILTIN_SHADERS_DIR = "shaders";
        static constexpr std::string_view DEFAULT_SPRITE_SHADER = "shaders/sprite";
        static constexpr std::string_view DEFAULT_MESH_SHADER = "shaders/mesh_lit";
        static constexpr std::string_view DEBUG_FONT_PATH = "fonts/jetbrains_mono_bold.ttf";

        struct DebugLineVertex {
            Vector2 screen_pos;
            Color color;
        };

        struct DebugTextVertex {
            Vector2 screen_pos;
            Vector2 uv;
            Color color;
        };

        SDL_GPUDevice* m_gpu_device;
        Vector2 m_logical_size;
        Vector2 m_reference_size;
        AspectMode m_aspect_mode;
        float m_render_scale;
        float m_pixel_density;

        // Plain game launch: m_main_window == m_game_window.
        // Hosted launch: m_main_window belongs to the host, m_game_window is null until one is opened.
        const Window* m_main_window = nullptr;
        const Window* m_game_window = nullptr;

        bool m_shadercross_initialized = false;
        bool m_initialized = false;

        float m_game_time = 0.0f;
        float m_real_time = 0.0f;

        // -- Registries --
        std::vector<SpriteDrawData> m_sprite_draws;
        std::vector<SpriteDrawIndex> m_sprite_draw_id_to_index;
        std::vector<SpriteDrawId> m_sprite_draw_index_to_id;
        std::vector<SpriteDrawId> m_sprite_draw_free_ids;
        std::vector<uint32_t> m_sprite_draw_order;
        bool m_sprite_draw_order_dirty = true;

        std::vector<MeshDrawData> m_mesh_draws;
        std::vector<MeshDrawIndex> m_mesh_draw_id_to_index;
        std::vector<MeshDrawId> m_mesh_draw_index_to_id;
        std::vector<MeshDrawId> m_mesh_draw_free_ids;
        std::vector<uint32_t> m_mesh_draw_order;
        bool m_mesh_draw_order_dirty = true;

        SceneLight m_scene_light;
        Vector3 m_camera_position;
        std::unordered_set<const Shader*> m_layout_warned_shaders;

        std::vector<DebugLineVertex> m_pending_debug_line_vertices;
        std::vector<DebugTextVertex> m_pending_debug_text_vertices;
        std::vector<uint16_t> m_pending_debug_text_indices;

        // -- Projections --
        Matrix4x4 m_offscreen_projection; // clip-space ortho mapping (0,0)..(w,h) -> (-1,-1)..(+1,+1) with y-down
        Matrix4x4 m_swapchain_projection; // same mapping, y-flipped for the swapchain's opposite NDC y convention

        // -- Command buffer --
        SDL_GPUCommandBuffer* m_command_buffer = nullptr;

        // -- Upload staging --
        // Shared staging buffer for upload_buffer().
        SDL_GPUTransferBuffer* m_upload_transfer_buffer = nullptr;
        uint32_t m_upload_transfer_capacity = 0;

        // -- Texture targets --
        SDL_GPUTexture* m_offscreen_color_target = nullptr;
        RenderTargets3D m_offscreen_targets_3d;
        SDL_GPUTextureFormat m_offscreen_format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        SDL_GPUTextureFormat m_depth_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
        SDL_GPUTextureFormat m_hdr_format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        SDL_GPUSampleCount m_msaa_sample_count = SDL_GPU_SAMPLECOUNT_1;

        SDL_GPUTexture* m_main_swap_texture = nullptr;
        SDL_GPUTexture* m_game_swap_texture = nullptr;
        SDL_GPUTextureFormat m_swapchain_format = SDL_GPU_TEXTUREFORMAT_INVALID;

        // -- Sprite pipelines --
        std::unordered_map<std::string, TextureWeakRef, StringHash, std::equal_to<>> m_textures;
        TextureRef m_fallback_texture; // 1x1 magenta, bound when a material leaves a texture slot unset
        TextureRef m_fallback_white_texture;
        TextureRef m_fallback_flat_normal_texture;

        ShaderRef m_default_shader;
        std::unordered_map<std::string, ShaderRef> m_shaders;

        MaterialRef m_default_material;
        std::vector<MaterialWeakRef> m_materials; // weak registry for the material-ref debug view

        // -- Mesh pipelines --
        ShaderRef m_default_mesh_shader;
        MaterialRef m_default_mesh_material;
        std::unordered_map<std::string, MeshRef> m_meshes;

        SamplerDesc m_default_sampler_desc;
        SDL_GPUSampler* m_default_sampler = nullptr;
        std::unordered_map<uint32_t, SDL_GPUSampler*> m_samplers;

        SDL_GPUBuffer* m_quad_vbo = nullptr;

        // -- Blit pipeline --
        SDL_GPUGraphicsPipeline* m_blit_pipeline = nullptr;
        SDL_GPUSampler* m_blit_sampler = nullptr;

        // -- 3D post pipelines --
        SDL_GPUGraphicsPipeline* m_tonemap_pipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_sky_pipeline = nullptr;

        // -- Volumetric fog --
        SDL_GPUGraphicsPipeline* m_fog_pipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_fog_upsample_pipeline = nullptr;

        // -- Bloom --
        SDL_GPUGraphicsPipeline* m_bloom_prefilter_pipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_bloom_downsample_pipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_bloom_upsample_pipeline = nullptr;

        // -- Depth/normal prepass + SSAO --
        SDL_GPUGraphicsPipeline* m_prepass_pipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_ssao_pipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_ssao_blur_pipeline = nullptr;
        SDL_GPUTexture* m_current_ssao_texture = nullptr;
        Vector2 m_current_target_size;

        // -- Sun shadow map --
        SDL_GPUTexture* m_shadow_map = nullptr;
        SDL_GPUSampler* m_shadow_sampler = nullptr;
        SDL_GPUGraphicsPipeline* m_shadow_pipeline = nullptr;
        Matrix4x4 m_light_view_proj;

        // -- Debug line pipeline --
        SDL_GPUGraphicsPipeline* m_debug_line_pipeline = nullptr;
        SDL_GPUBuffer* m_debug_line_vbo = nullptr;
        SDL_GPUTransferBuffer* m_debug_line_transfer_buffer = nullptr;

        // -- Debug text pipeline --
        SDL_GPUGraphicsPipeline* m_debug_text_pipeline = nullptr;
        SDL_GPUBuffer* m_debug_text_vbo = nullptr;
        SDL_GPUBuffer* m_debug_text_ibo = nullptr;
        SDL_GPUTransferBuffer* m_debug_text_vbo_transfer = nullptr;
        SDL_GPUTransferBuffer* m_debug_text_ibo_transfer = nullptr;
        SDL_GPUSampler* m_debug_text_sampler = nullptr;
        Font m_debug_font;
        float m_debug_font_baked_inverse_pixel_density = 1.0f;

        // -- CVars --
        bool m_cvar_log_textures = false;
        bool m_cvar_show_textures = false;

        bool m_cvar_log_materials = false;
        bool m_cvar_show_materials = false;

        bool m_cvar_log_shaders = false;
        bool m_cvar_show_shaders = false;

        bool m_cvar_log_shader_reflection = false;

        bool m_cvar_log_sprite_queue = false;
        bool m_cvar_show_sprite_queue = false;

        bool m_cvar_log_mesh_queue = false;
        bool m_cvar_show_mesh_queue = false;

        int32_t m_cvar_msaa = 4;
        float m_cvar_exposure = 1.0f;
        bool m_cvar_sky = true;
        bool m_cvar_shadows = true;
        float m_cvar_shadow_bias = 0.0005f;
        bool m_cvar_show_shadow_map = false;
        bool m_cvar_ssao = true;
        float m_cvar_ssao_radius = 0.5f;
        float m_cvar_ssao_intensity = 1.0f;
        bool m_cvar_show_ssao = false;
        bool m_cvar_show_frame_stats = false;
        uint32_t m_stats_sprite_draws = 0;
        uint32_t m_stats_mesh_draws = 0;
        uint32_t m_stats_mesh_triangles = 0;
        float m_stats_frame_seconds = 0.0f;
        bool m_cvar_fog = true;
        int32_t m_cvar_fog_steps = 32;
        bool m_cvar_show_fog = false;
        bool m_cvar_bloom = true;
        float m_cvar_bloom_threshold = 1.0f;
        float m_cvar_bloom_intensity = 0.2f;

    public:
        Renderer(const GraphicsConfig& graphics_config, SDL_GPUDevice* gpu_device, const Window& main_window);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        void register_cvars(Console& console);

        void set_time(float game_time, float real_time);

        SDL_GPUDevice* get_gpu_device() const;

        Vector2 get_logical_size() const;
        Vector2 get_reference_size() const;

        const Window* get_main_window() const;
        const Window* get_game_window() const;
        void set_game_window(const Window* window);

        void on_window_resized(int32_t window_width, int32_t window_height);

        static Matrix4x4 ortho_top_left(float w, float h);
        static Matrix4x4 ortho_top_left_y_flipped(float w, float h);
        static Matrix4x4 perspective_offscreen(float fov_y_rad, float aspect, float near_plane, float far_plane);

        bool acquire_command_buffer();
        void submit_command_buffer();
        void cancel_command_buffer();
        SDL_GPUCommandBuffer* get_command_buffer() const;

        SDL_GPUTexture* get_main_swap_texture() const;
        SDL_GPUTexture* get_game_swap_texture() const;
        SDL_GPUTextureFormat get_swapchain_format() const;
        SDL_GPUTextureFormat get_offscreen_format() const;
        SDL_GPUTextureFormat get_depth_format() const;
        SDL_GPUTextureFormat get_hdr_format() const;
        SDL_GPUSampleCount get_msaa_sample_count() const;

        SDL_GPUTexture* create_color_target(uint32_t width, uint32_t height) const;
        SDL_GPUTexture* create_depth_target(uint32_t width,
                                            uint32_t height,
                                            SDL_GPUSampleCount sample_count = SDL_GPU_SAMPLECOUNT_1,
                                            bool sampled = false) const;
        SDL_GPUTexture* create_hdr_target(uint32_t width, uint32_t height, SDL_GPUSampleCount sample_count) const;
        bool create_targets_3d(RenderTargets3D& targets, uint32_t width, uint32_t height) const;
        void release_targets_3d(RenderTargets3D& targets) const;

        SpriteDrawId register_sprite_draw();
        void unregister_sprite_draw(SpriteDrawId draw_id);
        void update_sprite_draw(SpriteDrawId draw_id, SpriteDrawData draw_data);
        const SpriteDrawData* get_sprite_draw(SpriteDrawId draw_id) const;

        MeshDrawId register_mesh_draw();
        void unregister_mesh_draw(MeshDrawId draw_id);
        void update_mesh_draw(MeshDrawId draw_id, MeshDrawData draw_data);
        const MeshDrawData* get_mesh_draw(MeshDrawId draw_id) const;

        const SceneLight& get_scene_light() const;
        void set_scene_light(const SceneLight& light);

        void draw_debug_line(const Vector2& screen_start,
                             const Vector2& screen_end,
                             const Color& color,
                             float thickness_pixels);

        void draw_debug_text(const Vector2& screen_pos, std::string_view text, const Color& color, float scale);

        int32_t get_debug_font_line_height() const;

        void render_world_pass(const Matrix4x4& view_proj);
        void render_world_pass_to(SDL_GPUTexture* target, const Matrix4x4& view_proj);
        void render_world_pass_3d(const Matrix4x4& view_proj, const Vector3& camera_position);
        void render_world_pass_3d_to(RenderTargets3D& targets,
                                     SDL_GPUTexture* ldr_target,
                                     const Matrix4x4& view_proj,
                                     const Vector3& camera_position);
        void render_blit_pass();
        void render_debug_lines_pass();
        void render_debug_text_pass();

        void discard_pending_debug_draws();

        TextureRef get_cached_texture(std::string_view key) const;
        TextureRef get_or_load_texture(std::string_view relative_path);
        TextureRef create_texture_from_rgba(const void* pixels, uint32_t width, uint32_t height);

        ShaderRef get_or_build_shader(const std::string& relative_path, BlendMode blend, CullMode cull);
        ShaderRef get_default_shader() const;
        SDL_GPUShader* load_shader(const std::filesystem::path& hlsl_path,
                                   SDL_ShaderCross_ShaderStage stage,
                                   ShaderReflection* out_reflection = nullptr);

        MaterialRef create_material(ShaderRef shader);
        MaterialRef clone_material(const Material& source);
        MaterialRef get_default_material() const;

        ShaderRef get_default_mesh_shader() const;
        MaterialRef get_default_mesh_material() const;
        MeshRef create_mesh(const MeshData& data, std::string source);
        MeshRef get_or_create_primitive_mesh(std::string_view name);
        MeshRef get_or_load_mesh(std::string_view relative_path);

        SDL_GPUSampler* get_or_create_sampler(const SamplerDesc& desc);
        const SamplerDesc& get_default_sampler_desc() const;

        bool upload_texture_rgba(SDL_GPUTexture* dst_texture, const void* pixels, uint32_t width, uint32_t height);
        bool upload_buffer(SDL_GPUBuffer* dst_buffer, const void* data, uint32_t size);

    private:
        bool ensure_upload_transfer_buffer(uint32_t size);

        void release_texture(Texture& texture);
        void release_textures();
        void release_shaders();
        void release_materials();
        void release_meshes();
        void track_material(const MaterialRef& material);

        SDL_GPUTextureFormat probe_depth_format() const;
        SDL_GPUTextureFormat probe_hdr_format() const;
        SDL_GPUSampleCount probe_msaa_sample_count(int32_t requested_samples) const;
        bool init_offscreen_targets();
        void rebuild_mesh_shader_pipelines();
        SDL_GPUGraphicsPipeline* create_fullscreen_pipeline(std::string_view vertex_file,
                                                            std::string_view fragment_file,
                                                            SDL_GPUTextureFormat target_format,
                                                            BlendMode blend = BlendMode::Opaque);
        void render_tonemap_pass(SDL_GPUTexture* hdr_source, SDL_GPUTexture* bloom_source, SDL_GPUTexture* ldr_target);
        const Texture& get_mesh_fallback_texture(std::string_view texture_name) const;
        bool init_samplers();
        bool init_quad_vbo();
        bool init_default_sprite_pipeline();
        bool init_default_mesh_pipeline();
        bool init_blit_pipeline();
        bool init_tonemap_pipeline();
        bool init_sky_pipeline();
        bool init_shadow_resources();
        bool init_prepass_pipeline();
        bool init_bloom_pipelines();
        bool init_fog_pipelines();
        void render_fog_passes(const RenderTargets3D& targets, const Matrix4x4& view_proj, SDL_GPUTexture* hdr_source);
        void debug_fog();
        void debug_frame_stats();
        void render_bloom_passes(const RenderTargets3D& targets, SDL_GPUTexture* hdr_source);
        void render_fullscreen_pass(SDL_GPUGraphicsPipeline* pipeline,
                                    SDL_GPUTexture* target,
                                    SDL_GPULoadOp load_op,
                                    SDL_GPUTexture* source,
                                    const void* uniforms,
                                    uint32_t uniforms_size);
        bool init_ssao_pipelines();
        SDL_GPUTexture* create_render_texture(SDL_GPUTextureFormat format,
                                              uint32_t width,
                                              uint32_t height,
                                              const Color& clear_color) const;
        void render_prepass(const RenderTargets3D& targets, const Matrix4x4& view_proj);
        void render_ssao_passes(const RenderTargets3D& targets, const Matrix4x4& view_proj);
        void debug_ssao();
        Matrix4x4 build_light_view_proj(const Vector3& camera_position) const;
        void render_shadow_pass();
        void debug_shadow_map();
        void record_sky(SDL_GPURenderPass* pass, const Matrix4x4& view_proj);
        bool init_debug_line_pipeline();
        bool init_debug_text_pipeline();
        bool init_debug_font();

        ShaderRef build_shader(const std::string& relative_path,
                               SDL_GPUTextureFormat target_format,
                               BlendMode blend,
                               CullMode cull);

        void record_sprite_draw(SDL_GPURenderPass* pass,
                                const SpriteDrawData& draw,
                                const Matrix4x4& view_proj,
                                const Shader*& bound_shader);
        void push_sprite_fragment_uniforms(const Texture& texture, const Material& material);

        void record_mesh_draw(SDL_GPURenderPass* pass,
                              const MeshDrawData& draw,
                              const Matrix4x4& view_proj,
                              const Shader*& bound_shader,
                              const Mesh*& bound_mesh);
        void push_mesh_fragment_uniforms(const Material& material);
        void fill_engine_cbuffer(const Shader& shader,
                                 const Texture* texture,
                                 std::array<uint8_t, ENGINE_CBUFFER_MAX_BYTES>& out) const;
        bool warn_vertex_layout_mismatch(const Shader& shader, VertexLayout expected);

        void debug_textures();
        void debug_shaders();
        void debug_materials();
        void debug_sprite_queue();
        void debug_mesh_queue();
    };
} // namespace hob
