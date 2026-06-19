#pragma once

#include <filesystem>

#include "systems/console.h"
#include "systems/cursor.h"
#include "systems/entity_spawner.h"
#include "systems/imgui_system.h"
#include "systems/input.h"
#include "systems/physics.h"
#include "systems/renderer/renderer.h"
#include "systems/scripting/lua_script_system.h"
#include "systems/sdl_context.h"
#include "systems/timer.h"

namespace hob {
    struct EngineConfig;
    class CameraComponent;

    class Engine {
        // Order matters
        SdlContext m_sdl_context;
        ImGuiSystem m_imgui_system;
        Console m_console;
        Renderer m_renderer;
        Timer m_timer;
        Input m_input;
        Physics m_physics;
        Cursor m_cursor;
        EntitySpawner m_entity_spawner;
        LuaScriptSystem m_lua_script_system;

        CameraComponent* m_active_camera = nullptr;

        bool m_is_os_cursor_visible_before_console_opened = false;

#ifndef NDEBUG
        std::filesystem::file_time_type m_last_script_write_time{};
        bool m_has_script_write_baseline = false;
        float m_script_watch_accumulator = 0.0f;
#endif

    public:
        explicit Engine(const EngineConfig& config);
        ~Engine();

        void run();

        bool is_initialized() const;

        SdlContext& get_sdl_context();
        Console& get_console();
        Renderer& get_renderer();
        Timer& get_timer();
        Input& get_input();
        Physics& get_physics();
        Cursor& get_cursor();
        EntitySpawner& get_entity_spawner();
        LuaScriptSystem& get_lua_script_system();

        CameraComponent* get_active_camera() const;
        void set_active_camera(CameraComponent* camera);
        void clear_active_camera(CameraComponent* camera);

    private:
        void draw_entities();
        void flush_debug_draws_to_renderer(float delta_time);

        static bool has_moving_physics_body(const Entity& entity);

#ifndef NDEBUG
        void poll_script_hot_reload(float delta_time);
#endif
    };
} // namespace hob
