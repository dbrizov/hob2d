#pragma once

#include <memory>
#include <string>

#include "component.h"

namespace hob {
    struct LuaScriptComponentImpl;

    class LuaScriptComponent : public Component {
        std::string m_class_name;
        std::unique_ptr<LuaScriptComponentImpl> m_impl;
        int m_priority;

    public:
        LuaScriptComponent(Entity& entity, std::string class_name);
        ~LuaScriptComponent() override;

        const std::string& get_class_name() const;

        LuaScriptComponentImpl& impl();
        const LuaScriptComponentImpl& impl() const;

        void refresh_class_cache();

        int get_priority() const override;

        void init() override;
        void enter_play() override;
        void exit_play() override;
        void tick(float delta_time) override;
        void physics_tick(float fixed_delta_time) override;
        void late_tick(float delta_time) override;
        void debug_draw_tick(float delta_time) override;
        void on_collision_enter(const ColliderComponent* other_collider) override;
        void on_collision_exit(const ColliderComponent* other_collider) override;
        void on_trigger_enter(const ColliderComponent* other_collider) override;
        void on_trigger_exit(const ColliderComponent* other_collider) override;

        std::string to_string() const override;
    };
} // namespace hob
