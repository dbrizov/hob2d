#include "component.h"

#include "engine/entity/entity.h"

namespace hob {
    Component::Component(Entity& entity)
        : m_entity(entity) {}

    Engine& Component::get_engine() const {
        return m_entity.get_engine();
    }

    Entity& Component::get_entity() const {
        return m_entity;
    }

    int Component::get_priority() const {
        return component_priority::CP_DEFAULT;
    }

    // clang-format off
    void Component::init() {}
    void Component::enter_play() {}
    void Component::exit_play() {}
    void Component::tick(float delta_time) {}
    void Component::physics_tick(float fixed_delta_time) {}
    void Component::late_tick(float delta_time) {}
    void Component::debug_draw_tick(float delta_time) {}
    void Component::on_collision_enter(const ColliderComponent* other_collider) {}
    void Component::on_collision_exit(const ColliderComponent* other_collider) {}
    void Component::on_trigger_enter(const ColliderComponent* other_collider) {}
    void Component::on_trigger_exit(const ColliderComponent* other_collider) {}
    // clang-format on

    std::string Component::to_string() const {
        return "Component";
    }
} // namespace hob
