// clang-format off
#include "engine.h"
#include "engine_config.h"
// clang-format on

#include <utility>

#include "debug.h"
#include "engine/components/audio_component.h"
#include "engine/components/camera_component.h"
#include "engine/components/physics/rigidbody_component.h"
#include "engine/components/sprite_component.h"
#include "engine/components/transform_component.h"
#include "engine/core/assert.h"
#include "engine/core/engine_hooks.h"
#include "engine/core/systems/window.h"
#include "engine/math/matrix2x3.h"
#include "logging.h"

namespace hob {
    namespace {
        WindowConfig make_window_config(const GraphicsConfig& graphics_config) {
            WindowConfig config;
            config.title = graphics_config.window_title;
            config.icon = graphics_config.window_icon;
            config.width = static_cast<int32_t>(graphics_config.window_width);
            config.height = static_cast<int32_t>(graphics_config.window_height);
            config.vsync = graphics_config.vsync_enabled;
            return config;
        }

        WindowConfig make_main_window_config(const EngineConfig& config) {
            return config.host_config.main_window_override.value_or(make_window_config(config.graphics_config));
        }
    } // namespace

    Engine::Engine(const EngineConfig& config)
        : m_sdl_context()
        , m_main_window(m_sdl_context.get_gpu_device(), make_main_window_config(config))
        , m_renderer(config.graphics_config, m_sdl_context.get_gpu_device(), m_main_window)
        , m_timer(config.graphics_config)
        , m_input(m_renderer)
        , m_ui_system(config.ui_system_config, m_renderer, m_timer)
        , m_imgui_system(m_renderer)
        , m_console()
        , m_physics(config.physics_config)
        , m_audio(config.audio_config)
        , m_entity_spawner(*this)
        , m_lua_script_system(*this, config.host_config.run_project_main_on_boot)
        , m_game_window_config(make_window_config(config.graphics_config)) {

        m_renderer.register_cvars(m_console);
        m_physics.register_cvars(m_console);
        m_lua_script_system.register_cvars(m_console);

        if (config.host_config.main_window_hosts_game) {
            m_renderer.set_game_window(&m_main_window);

            int32_t width_px = 0;
            int32_t height_px = 0;
            m_main_window.get_size_px(width_px, height_px);
            m_renderer.on_window_resized(width_px, height_px);
            m_ui_system.on_window_resized(width_px, height_px);
        }
        else {
            m_renderer.set_game_window(nullptr);
        }
    }

    Engine::~Engine() {
        // Tear down entities (and their components) while every subsystem is still alive.
        // Avoids dangling references during member destruction.
        // In particular - LuaScriptComponent's sol::table must release its Lua registry slot before
        // LuaScriptSystem destroys the lua_State.
        m_entity_spawner.clear();

        // UI event listeners (and data-model event callbacks) hold Lua callbacks; release them
        // here for the same reason - UiSystem outlives LuaScriptSystem in member-destruction order.
        m_ui_system.clear_event_listeners();
        m_ui_system.clear_data_models();
    }

    EngineHooks* Engine::get_hooks() const {
        return m_hooks;
    }

    void Engine::set_hooks(EngineHooks* hooks) {
        m_hooks = hooks;
    }

    void Engine::run() {
        if (m_hooks != nullptr) {
            m_hooks->init();
        }

        bool is_running = true;

        while (is_running) {
            m_timer.begin_frame();

            bool game_window_resized = false;

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                m_imgui_system.process_event(event);
                m_ui_system.process_event(event);
                m_input.process_event(event);

                if (event.type == SDL_EVENT_QUIT) {
                    if (m_hooks == nullptr || !m_hooks->on_quit_requested()) {
                        is_running = false;
                    }
                }
                else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                    if (m_hooks == nullptr || !m_hooks->on_window_close_requested(event.window.windowID)) {
                        is_running = false;
                    }
                }
                else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                    if (m_game_window && event.window.windowID == m_game_window->get_id()) {
                        game_window_resized = true;
                    }
                }
                else if (event.type == SDL_EVENT_KEY_DOWN) {
                    if (event.key.key == SDLK_GRAVE) {
                        m_console.toggle_open();
                    }
                }
            }

            if (game_window_resized) {
                sync_game_window_size();
            }

            m_imgui_system.new_frame();

            if (m_hooks != nullptr) {
                m_hooks->begin_frame();
            }

            m_entity_spawner.resolve_requests();

            const float delta_time = m_timer.get_delta_time();
            const float scaled_delta_time = delta_time * m_timer.get_time_scale();

#ifndef NDEBUG
            m_lua_script_system.poll_hot_reload(delta_time);
            m_ui_system.poll_hot_reload(delta_time);
#endif

            if (m_hooks != nullptr) {
                m_hooks->tick(delta_time);
            }

            const bool is_game_input_active =
                m_world_state == WorldState::Playing && !m_console.is_open() && get_play_window().has_focus();

            if (is_game_input_active) {
                m_input.tick(scaled_delta_time);
            }

            const bool ticking = is_ticking();
            m_is_tick_step_requested = false;

            if (ticking) {
                for (Entity* entity : m_entity_spawner.get_ticking_entities()) {
                    entity->tick(scaled_delta_time);
                }

                m_physics.tick(scaled_delta_time, m_entity_spawner.get_simulated_rigidbodies());

                for (Entity* entity : m_entity_spawner.get_ticking_entities()) {
                    entity->late_tick(scaled_delta_time);
                }

                for (AudioComponent* audio_source : m_entity_spawner.get_audio_sources()) {
                    audio_source->update_spatialization();
                }

                m_ui_system.tick();
            }

#ifndef NDEBUG
            m_entity_spawner.for_each_entity([scaled_delta_time](Entity* entity) {
                entity->debug_draw_tick(scaled_delta_time);
            });
#endif

            flush_debug_draws_to_renderer(scaled_delta_time);

            if (m_console.is_open()) {
                m_console.draw();
            }

            if (m_hooks != nullptr) {
                m_hooks->draw_gui();
            }

            draw_entities();

            m_renderer.set_time(m_timer.get_game_time(), m_timer.get_real_time());
            if (m_renderer.acquire_command_buffer()) {
                if (m_renderer.get_game_swap_texture() != nullptr) {
                    m_renderer.render_world_pass(get_game_camera_view_projection());
                    m_renderer.render_blit_pass();
                    m_renderer.render_debug_lines_pass();
                    m_ui_system.render_pass();
                    m_renderer.render_debug_text_pass();
                }
                else {
                    m_renderer.discard_pending_debug_draws();
                }

                if (m_hooks != nullptr) {
                    m_hooks->render_passes();
                }

                m_imgui_system.render_pass();

                m_renderer.submit_command_buffer();
            }
            else {
                m_imgui_system.discard_frame();
                m_renderer.cancel_command_buffer();
            }

            if (m_hooks != nullptr) {
                m_hooks->end_frame();
            }

            m_input.end_frame(is_game_input_active);
            m_timer.end_frame();
        }
    }

    SdlContext& Engine::get_sdl_context() {
        return m_sdl_context;
    }

    Renderer& Engine::get_renderer() {
        return m_renderer;
    }

    Timer& Engine::get_timer() {
        return m_timer;
    }

    Input& Engine::get_input() {
        return m_input;
    }

    UiSystem& Engine::get_ui_system() {
        return m_ui_system;
    }

    ImGuiSystem& Engine::get_imgui_system() {
        return m_imgui_system;
    }

    Console& Engine::get_console() {
        return m_console;
    }

    Physics& Engine::get_physics() {
        return m_physics;
    }

    Audio& Engine::get_audio() {
        return m_audio;
    }

    EntitySpawner& Engine::get_entity_spawner() {
        return m_entity_spawner;
    }

    LuaScriptSystem& Engine::get_lua_script_system() {
        return m_lua_script_system;
    }

    Window& Engine::get_main_window() {
        return m_main_window;
    }

    const Window& Engine::get_main_window() const {
        return m_main_window;
    }

    const Window& Engine::get_play_window() const {
        return m_game_window ? *m_game_window : m_main_window;
    }

    const Window* Engine::get_game_window() const {
        return m_game_window.get();
    }

    WindowConfig Engine::get_game_window_config() const {
        if (!m_game_window) {
            return m_game_window_config;
        }

        WindowConfig config = m_game_window_config;
        m_game_window->get_position(config.x, config.y);
        m_game_window->get_size(config.width, config.height);
        config.maximized = m_game_window->is_maximized();
        return config;
    }

    void Engine::set_game_window_config(const WindowConfig& config) {
        m_game_window_config = config;
    }

    void Engine::open_game_window() {
        if (m_game_window) {
            return;
        }

        m_game_window = std::make_unique<Window>(m_sdl_context.get_gpu_device(), m_game_window_config);
        m_renderer.set_game_window(m_game_window.get());

        sync_game_window_size();
    }

    void Engine::close_game_window() {
        if (!m_game_window) {
            return;
        }

        m_game_window_config = get_game_window_config();

        // The last frame presented to this window may still be in flight; wait before releasing it.
        SDL_WaitForGPUIdle(m_renderer.get_gpu_device());

        m_renderer.set_game_window(nullptr);
        m_game_window.reset();
    }

    WorldState Engine::get_world_state() const {
        return m_world_state;
    }

    void Engine::set_world_state(WorldState state) {
        m_world_state = state;
    }

    bool Engine::is_in_play() const {
        return m_world_state != WorldState::Stopped;
    }

    bool Engine::is_ticking() const {
        return m_world_state == WorldState::Playing ||
               (m_world_state == WorldState::Paused && m_is_tick_step_requested);
    }

    void Engine::request_tick_step() {
        m_is_tick_step_requested = true;
    }

    CameraComponent* Engine::get_active_camera() const {
        return m_active_camera;
    }

    void Engine::set_active_camera(CameraComponent* camera) {
        m_active_camera = camera;
        m_warned_no_active_camera = false;
    }

    void Engine::clear_active_camera(CameraComponent* camera) {
        if (m_active_camera == camera) {
            m_active_camera = nullptr;
        }
    }

    Matrix4x4 Engine::get_game_camera_view_projection() const {
        const CameraComponent* camera = get_active_camera();
        if (camera == nullptr) {
            // A host may legitimately run frames with no game camera (a stopped world).
            if (!m_warned_no_active_camera && m_hooks == nullptr) {
                log::engine.error("Engine::draw_entities: no active camera (spawn a Camera entity to render)");
                m_warned_no_active_camera = true;
            }

            return Matrix4x4::identity();
        }

        return camera->build_view_projection();
    }

    void Engine::sync_game_window_size() {
        if (!m_game_window) {
            return;
        }

        int32_t width_px = 0;
        int32_t height_px = 0;
        m_game_window->get_size_px(width_px, height_px);
        m_renderer.on_window_resized(width_px, height_px);
        m_ui_system.on_window_resized(width_px, height_px);
    }

    void Engine::draw_entities() {
        const float interpolation_fraction = m_physics.get_interpolation_fraction();

        for (SpriteComponent* sprite_comp : m_entity_spawner.get_sprites()) {
            TransformComponent* transform_comp = sprite_comp->get_entity().get_transform();

            const bool sprite_dirty = sprite_comp->consume_render_dirty();
            const bool transform_dirty = transform_comp->consume_render_dirty();
            const bool interpolating =
                transform_comp->get_interpolate_physics() && has_moving_physics_body(sprite_comp->get_entity());

            const TextureRef& texture = sprite_comp->get_texture();
            // const overload: reading the material here must not mark the sprite dirty every frame.
            const MaterialRef& material = static_cast<const SpriteComponent*>(sprite_comp)->get_material();

            if (!sprite_dirty && !transform_dirty && !interpolating) {
                const SpriteDrawData* pooled = m_renderer.get_sprite_draw(sprite_comp->get_sprite_draw_id());
                HOB_CHECK(pooled == nullptr || (pooled->texture == texture.get() && pooled->material == material.get()),
                          "Sprite draw {} is stale: its texture/material changed without marking render-dirty",
                          sprite_comp->get_sprite_draw_id());
                continue;
            }

            const Matrix2x3 matrix = transform_comp->get_interpolate_physics()
                                         ? Matrix2x3::lerp(transform_comp->get_prev_world_matrix(),
                                                           transform_comp->get_world_matrix(),
                                                           interpolation_fraction)
                                         : transform_comp->get_world_matrix();

            SpriteDrawData draw_data;
            draw_data.texture = texture.get();
            draw_data.material = material.get();
            draw_data.z_index = sprite_comp->get_z_index();
            draw_data.pivot = sprite_comp->get_pivot();
            draw_data.world_matrix = matrix;
            draw_data.local_size = sprite_comp->get_local_size();

            m_renderer.update_sprite_draw(sprite_comp->get_sprite_draw_id(), std::move(draw_data));
        }
    }

    void Engine::flush_debug_draws_to_renderer(float delta_time) {
        CameraComponent* camera = get_active_camera();
        if (camera == nullptr) {
            return;
        }

        debug::flush_draws_to_renderer(m_renderer, camera, get_play_window().get_size(), delta_time);
    }

    bool Engine::has_moving_physics_body(const Entity& entity) {
        const RigidbodyComponent* rigidbody = entity.get_rigidbody();
        const bool result = rigidbody != nullptr && rigidbody->has_body() &&
                            rigidbody->get_body_type() != BodyType::Static && rigidbody->is_awake();

        return result;
    }

} // namespace hob
