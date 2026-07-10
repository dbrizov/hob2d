// clang-format off
#include "engine.h"
#include "engine_config.h"
// clang-format on

#include "debug.h"
#include "engine/components/audio_component.h"
#include "engine/components/camera_component.h"
#include "engine/components/physics/rigidbody_component.h"
#include "engine/components/sprite_component.h"
#include "engine/components/transform_component.h"
#include "engine/math/matrix2x3.h"
#include "logging.h"

namespace hob {
    Engine::Engine(const EngineConfig& config)
        : m_sdl_context(config.graphics_config)
        , m_renderer(config.graphics_config, m_sdl_context)
        , m_timer(config.graphics_config)
        , m_input(m_sdl_context, m_renderer)
        , m_ui_system(config.ui_system_config, m_sdl_context, m_renderer, m_timer)
        , m_imgui_system(m_sdl_context)
        , m_console()
        , m_physics(config.physics_config)
        , m_audio(config.audio_config)
        , m_entity_spawner(*this)
        , m_lua_script_system(*this) {
        m_renderer.register_cvars(m_console);
        m_physics.register_cvars(m_console);
        m_audio.register_cvars(m_console);
        m_lua_script_system.register_cvars(m_console);
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

    void Engine::run() {
        bool is_running = true;
        std::vector<Entity*> entities;

        while (is_running) {
            m_timer.frame_start();

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                m_imgui_system.process_event(event);
                m_ui_system.process_event(event);
                m_input.process_event(event);

                if (event.type == SDL_EVENT_QUIT) {
                    is_running = false;
                }
                else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                    m_renderer.on_window_resized(event.window.data1, event.window.data2);
                    m_ui_system.on_window_resized(event.window.data1, event.window.data2);
                }
                else if (event.type == SDL_EVENT_KEY_DOWN) {
                    if (event.key.key == SDLK_GRAVE) {
                        m_console.toggle_open();
                    }
                }
            }

            m_imgui_system.new_frame();

            m_entity_spawner.resolve_requests();
            m_entity_spawner.get_entities(entities);

            const float delta_time = m_timer.get_delta_time();
            const float scaled_delta_time = delta_time * m_timer.get_time_scale();

#ifndef NDEBUG
            m_lua_script_system.poll_hot_reload(delta_time);
            m_ui_system.poll_hot_reload(delta_time);
#endif

            if (!m_console.is_open()) {
                m_input.tick(scaled_delta_time);
            }

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
            m_audio.debug_clips();

            m_ui_system.tick();

#ifndef NDEBUG
            for (Entity* entity : entities) {
                entity->debug_draw_tick(scaled_delta_time);
            }
#endif

            draw_entities();
            flush_debug_draws_to_renderer(scaled_delta_time);
            if (m_console.is_open()) {
                m_console.draw();
            }

            m_renderer.set_time(m_timer.get_game_time(), m_timer.get_real_time());
            if (m_renderer.acquire_command_buffer()) {
                m_renderer.render_world_pass();
                m_renderer.render_blit_pass();
                m_renderer.render_debug_lines_pass();
                m_ui_system.render_pass(m_renderer.get_command_buffer(), m_renderer.get_swap_texture());
                m_renderer.render_debug_text_pass();
                m_imgui_system.render_pass(m_renderer.get_command_buffer(), m_renderer.get_swap_texture());

                m_renderer.submit_command_buffer();
            }
            else {
                m_imgui_system.discard_frame();
                m_renderer.cancel_command_buffer();
            }

            m_input.end_frame();
            m_timer.frame_end();
        }
    }

    SdlContext& Engine::get_sdl_context() {
        return m_sdl_context;
    }

    Console& Engine::get_console() {
        return m_console;
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

    void Engine::draw_entities() {
        const CameraComponent* camera = get_active_camera();
        if (camera == nullptr) {
            if (!m_warned_no_active_camera) {
                log::engine.error("Engine::draw_entities: no active camera (spawn a Camera entity to render)");
                m_warned_no_active_camera = true;
            }
            return;
        }

        m_renderer.set_camera_view_projection(camera->build_view_projection());

        const float interpolation_fraction = m_physics.get_interpolation_fraction();

        for (SpriteComponent* sprite_comp : m_entity_spawner.get_sprites()) {
            TransformComponent* transform_comp = sprite_comp->get_entity().get_transform();

            const bool sprite_dirty = sprite_comp->consume_render_dirty();
            const bool transform_dirty = transform_comp->consume_render_dirty();
            const bool interpolating =
                transform_comp->get_interpolate_physics() && has_moving_physics_body(sprite_comp->get_entity());

            if (!sprite_dirty && !transform_dirty && !interpolating) {
                continue;
            }

            const Matrix2x3 matrix = transform_comp->get_interpolate_physics()
                                         ? Matrix2x3::lerp(transform_comp->get_prev_world_matrix(),
                                                           transform_comp->get_world_matrix(),
                                                           interpolation_fraction)
                                         : transform_comp->get_world_matrix();

            SpriteDrawData draw_data;
            draw_data.texture = sprite_comp->get_texture();
            // const overload: reading the material here must not mark the sprite dirty every frame.
            draw_data.material = static_cast<const SpriteComponent*>(sprite_comp)->get_material();
            draw_data.z_index = sprite_comp->get_z_index();
            draw_data.pivot = sprite_comp->get_pivot();
            draw_data.world_pos = matrix.origin;
            draw_data.rotation = matrix.get_rotation();

            if (draw_data.texture != nullptr) {
                // Read world scale directly rather than decomposing the interpolated matrix:
                // lerping two rotation matrices ~180 deg apart shrinks the basis toward zero at the midpoint,
                // which would momentarily collapse the sprite. World scale is not interpolated.
                const Vector2 tr_scale = transform_comp->get_scale();
                const Vector2 sp_scale = sprite_comp->get_scale();
                const float sprite_ppm = sprite_comp->get_pixels_per_meter_f();
                const float texture_width = static_cast<float>(draw_data.texture->get_width());
                const float texture_height = static_cast<float>(draw_data.texture->get_height());
                // Size is in world meters (camera ppm is applied by the view-projection).
                draw_data.size = Vector2((texture_width / sprite_ppm) * tr_scale.x * sp_scale.x,
                                         (texture_height / sprite_ppm) * tr_scale.y * sp_scale.y);
            }

            m_renderer.update_sprite_draw(sprite_comp->get_sprite_draw_id(), draw_data);
        }
    }

    void Engine::flush_debug_draws_to_renderer(float delta_time) {
        CameraComponent* camera = get_active_camera();
        if (camera == nullptr) {
            return;
        }

        debug::flush_draws_to_renderer(m_renderer, camera, m_sdl_context.get_window_size(), delta_time);
    }

    bool Engine::has_moving_physics_body(const Entity& entity) {
        const RigidbodyComponent* rigidbody = entity.get_rigidbody();
        const bool result = rigidbody != nullptr && rigidbody->has_body() &&
                            rigidbody->get_body_type() != BodyType::Static && rigidbody->is_awake();

        return result;
    }

} // namespace hob
