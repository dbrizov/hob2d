// clang-format off
#include "lua_script_component.h"
#include "lua_script_component_impl.h"
// clang-format on

#include <format>
#include <utility>

#include "engine/components/physics/collider_component.h"
#include "engine/core/debug.h"
#include "engine/core/engine.h"
#include "engine/core/systems/scripting/lua_script_system.h"
#include "engine/entity/entity.h"
#include "engine/entity/entity_ref.h"

namespace hob {
    namespace {
        sol::protected_function resolve_hook(const sol::table& instance, const char* method) {
            if (!instance.valid()) {
                return {};
            }

            sol::object fn = instance[method];
            if (!fn.is<sol::protected_function>()) {
                return {};
            }

            return fn;
        }

        template<typename... Args>
        void invoke_hook(const sol::protected_function& hook,
                         const sol::table& instance,
                         const char* method,
                         Args&&... args) {
            if (!hook.valid()) {
                return;
            }

            sol::protected_function_result result = hook(instance, std::forward<Args>(args)...);
            if (!result.valid()) {
                const sol::error err = result;
                debug::log_error("Lua error in {}: {}", method, err.what());
            }
        }
    } // namespace

    LuaScriptComponent::LuaScriptComponent(Entity& entity, std::string class_name)
        : Component(entity)
        , m_class_name(std::move(class_name))
        , m_impl(std::make_unique<LuaScriptComponentImpl>())
        , m_priority(component_priority::CP_DEFAULT) {}

    LuaScriptComponent::~LuaScriptComponent() = default;

    const std::string& LuaScriptComponent::get_class_name() const {
        return m_class_name;
    }

    LuaScriptComponentImpl& LuaScriptComponent::impl() {
        return *m_impl;
    }

    const LuaScriptComponentImpl& LuaScriptComponent::impl() const {
        return *m_impl;
    }

    void LuaScriptComponent::refresh_hook_cache() {
        m_impl->init = resolve_hook(m_impl->lua_instance, "init");
        m_impl->enter_play = resolve_hook(m_impl->lua_instance, "enter_play");
        m_impl->exit_play = resolve_hook(m_impl->lua_instance, "exit_play");
        m_impl->tick = resolve_hook(m_impl->lua_instance, "tick");
        m_impl->physics_tick = resolve_hook(m_impl->lua_instance, "physics_tick");
        m_impl->late_tick = resolve_hook(m_impl->lua_instance, "late_tick");
        m_impl->debug_draw_tick = resolve_hook(m_impl->lua_instance, "debug_draw_tick");
        m_impl->on_collision_enter = resolve_hook(m_impl->lua_instance, "on_collision_enter");
        m_impl->on_collision_exit = resolve_hook(m_impl->lua_instance, "on_collision_exit");
        m_impl->on_trigger_enter = resolve_hook(m_impl->lua_instance, "on_trigger_enter");
        m_impl->on_trigger_exit = resolve_hook(m_impl->lua_instance, "on_trigger_exit");
    }

    int LuaScriptComponent::get_priority() const {
        return m_priority;
    }

    void LuaScriptComponent::init() {
        if (m_class_name.empty()) {
            debug::log_error("LuaScriptComponent has no class name");
            return;
        }

        sol::state& lua = get_engine().get_lua_script_system().get_lua();

        const sol::object registry_obj = lua["__component_registry"];
        if (!registry_obj.is<sol::table>()) {
            debug::log_error("__component_registry is missing — engine bootstrap did not run");
            return;
        }

        sol::table registry = registry_obj;
        const sol::object class_obj = registry[m_class_name];
        if (!class_obj.is<sol::table>()) {
            debug::log_error("DefineComponent '{}' is not registered", m_class_name);
            return;
        }

        sol::table class_table = class_obj;

        const sol::optional<int> priority = class_table["priority"];
        m_priority = priority.value_or(component_priority::CP_DEFAULT);

        const sol::object new_fn_obj = class_table["new"];
        if (!new_fn_obj.is<sol::protected_function>()) {
            debug::log_error("'{}' does not have a 'new' function", m_class_name);
            return;
        }

        const sol::protected_function new_fn = new_fn_obj;
        sol::protected_function_result inst_result = new_fn();
        if (!inst_result.valid()) {
            const sol::error err = inst_result;
            debug::log_error("Failed to instantiate '{}': {}", m_class_name, err.what());
            return;
        }

        const sol::object inst_obj = inst_result;
        if (!inst_obj.is<sol::table>()) {
            debug::log_error("'{}'.new() did not return a table", m_class_name);
            return;
        }

        m_impl->lua_instance = inst_obj;
        m_impl->lua_instance["entity"] = EntityRef(get_entity().get_id(), get_engine().get_entity_spawner());
        m_impl->lua_instance["class_name"] = m_class_name;

        refresh_hook_cache();

        invoke_hook(m_impl->init, m_impl->lua_instance, "init");
    }

    void LuaScriptComponent::enter_play() {
        invoke_hook(m_impl->enter_play, m_impl->lua_instance, "enter_play");
    }

    void LuaScriptComponent::exit_play() {
        invoke_hook(m_impl->exit_play, m_impl->lua_instance, "exit_play");
    }

    void LuaScriptComponent::tick(float delta_time) {
        invoke_hook(m_impl->tick, m_impl->lua_instance, "tick", delta_time);
    }

    void LuaScriptComponent::physics_tick(float fixed_delta_time) {
        invoke_hook(m_impl->physics_tick, m_impl->lua_instance, "physics_tick", fixed_delta_time);
    }

    void LuaScriptComponent::late_tick(float delta_time) {
        invoke_hook(m_impl->late_tick, m_impl->lua_instance, "late_tick", delta_time);
    }

    void LuaScriptComponent::debug_draw_tick(float delta_time) {
        invoke_hook(m_impl->debug_draw_tick, m_impl->lua_instance, "debug_draw_tick", delta_time);
    }

    void LuaScriptComponent::on_collision_enter(const ColliderComponent* other_collider) {
        invoke_hook(m_impl->on_collision_enter, m_impl->lua_instance, "on_collision_enter", other_collider);
    }

    void LuaScriptComponent::on_collision_exit(const ColliderComponent* other_collider) {
        invoke_hook(m_impl->on_collision_exit, m_impl->lua_instance, "on_collision_exit", other_collider);
    }

    void LuaScriptComponent::on_trigger_enter(const ColliderComponent* other_collider) {
        invoke_hook(m_impl->on_trigger_enter, m_impl->lua_instance, "on_trigger_enter", other_collider);
    }

    void LuaScriptComponent::on_trigger_exit(const ColliderComponent* other_collider) {
        invoke_hook(m_impl->on_trigger_exit, m_impl->lua_instance, "on_trigger_exit", other_collider);
    }

    std::string LuaScriptComponent::to_string() const {
        return std::format("LuaScriptComponent(entity_id = {}, class = {})", get_entity().get_id(), get_class_name());
    }
} // namespace hob
