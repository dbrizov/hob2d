#pragma once

#include <memory>

#include "systems/audio/audio.h"
#include "systems/console.h"
#include "systems/entity_spawner.h"
#include "systems/imgui_system.h"
#include "systems/input/input.h"
#include "systems/physics/physics.h"
#include "systems/physics_3d/physics_3d.h"
#include "systems/renderer/renderer.h"
#include "systems/scripting/lua_script_system.h"
#include "systems/sdl_context.h"
#include "systems/timer.h"
#include "systems/ui/ui_system.h"
#include "systems/window.h"
#include "world_state.h"

namespace hob {
    struct EngineConfig;
    class CameraComponent;
    class CameraComponent3D;
    class DirectionalLightComponent;
    class EngineHooks;

    class Engine {
        // Order matters
        SdlContext m_sdl_context;
        Window m_main_window;
        Renderer m_renderer;
        Timer m_timer;
        Input m_input;
        UiSystem m_ui_system;
        ImGuiSystem m_imgui_system;
        Console m_console;
        Physics m_physics;
        Physics3D m_physics_3d;
        Audio m_audio;
        EntitySpawner m_entity_spawner;
        LuaScriptSystem m_lua_script_system;

        std::unique_ptr<Window> m_game_window;
        WindowConfig m_game_window_config;

        EngineHooks* m_hooks = nullptr;

        WorldState m_world_state = WorldState::Playing;
        bool m_is_tick_step_requested = false;

        CameraComponent* m_active_camera = nullptr;
        CameraComponent3D* m_active_camera_3d = nullptr;
        DirectionalLightComponent* m_active_directional_light = nullptr;
        mutable bool m_warned_no_active_camera = false;

    public:
        explicit Engine(const EngineConfig& config);
        ~Engine();

        EngineHooks* get_hooks() const;
        void set_hooks(EngineHooks* hooks);

        void run();

        SdlContext& get_sdl_context();
        Renderer& get_renderer();
        Timer& get_timer();
        Input& get_input();
        UiSystem& get_ui_system();
        ImGuiSystem& get_imgui_system();
        Console& get_console();
        Physics& get_physics();
        Physics3D& get_physics_3d();
        Audio& get_audio();
        EntitySpawner& get_entity_spawner();
        LuaScriptSystem& get_lua_script_system();

        Window& get_main_window();
        const Window& get_main_window() const;
        const Window& get_play_window() const;
        const Window* get_game_window() const;
        WindowConfig get_game_window_config() const;
        void set_game_window_config(const WindowConfig& config);
        void open_game_window();
        void close_game_window();

        WorldState get_world_state() const;
        void set_world_state(WorldState state);

        bool is_in_play() const;
        bool is_ticking() const;
        void request_tick_step();

        CameraComponent* get_active_camera() const;
        void set_active_camera(CameraComponent* camera);
        void clear_active_camera(CameraComponent* camera);

        CameraComponent3D* get_active_camera_3d() const;
        void set_active_camera_3d(CameraComponent3D* camera);
        void clear_active_camera_3d(CameraComponent3D* camera);

        DirectionalLightComponent* get_active_directional_light() const;
        void set_active_directional_light(DirectionalLightComponent* light);
        void clear_active_directional_light(DirectionalLightComponent* light);

    private:
        Matrix4x4 get_game_camera_view_projection() const;

        void sync_game_window_size();

        void draw_entities();
        void render_game_world_pass();
        void flush_debug_draws_to_renderer(float delta_time);

        static bool has_moving_physics_body(const Entity& entity);
        static bool has_moving_physics_body_3d(const Entity& entity);
    };
} // namespace hob
