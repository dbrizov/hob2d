#pragma once

#include <string>

namespace hob {
    class Engine;
    class ColliderComponent;
    class Entity;

    namespace component_priority {
        constexpr int CP_INPUT = -100;
        constexpr int CP_SOCKETS = -50;
        constexpr int CP_DEFAULT = 0;
    } // namespace component_priority

    class Component {
        Entity& m_entity;

    protected:
        explicit Component(Entity& entity);

    public:
        virtual ~Component() = default;

        Engine& get_engine() const;
        Entity& get_entity() const;

        virtual int get_priority() const;

        virtual void init();
        virtual void enter_play();
        virtual void exit_play();
        virtual void tick(float delta_time);
        virtual void physics_tick(float fixed_delta_time);
        virtual void late_tick(float delta_time);
        virtual void debug_draw_tick(float delta_time);
        virtual void on_collision_enter(const ColliderComponent* other_collider);
        virtual void on_collision_exit(const ColliderComponent* other_collider);
        virtual void on_trigger_enter(const ColliderComponent* other_collider);
        virtual void on_trigger_exit(const ColliderComponent* other_collider);

        virtual std::string to_string() const;
    };
} // namespace hob
