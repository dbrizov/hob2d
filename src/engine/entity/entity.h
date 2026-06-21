#pragma once

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "engine/components/component.h"

namespace hob {
    class Engine;
    class ColliderComponent;
    class RigidbodyComponent;
    class TransformComponent;

    using EntityId = int64_t;
    constexpr EntityId INVALID_ENTITY_ID = -1;

    using TickIndex = uint32_t;
    constexpr TickIndex INVALID_TICK_INDEX = std::numeric_limits<TickIndex>::max();

    template<typename T>
    concept ComponentType = std::derived_from<T, Component>;

    class Entity final {
        Engine& m_engine;
        EntityId m_id = 0;
        bool m_is_in_play = false;

        TickIndex m_tick_index = INVALID_TICK_INDEX; // Slot in EntitySpawner's ticking registry.
        bool m_is_ticking_request = false;

        std::vector<std::unique_ptr<Component>> m_components;
        mutable TransformComponent* m_transform = nullptr;
        mutable RigidbodyComponent* m_rigidbody = nullptr;

        // Only the EntitySpawner can create entities.
        friend class EntitySpawner;
        explicit Entity(Engine& engine);

    public:
        Entity(const Entity&) = delete;
        Entity& operator=(const Entity&) = delete;

        Entity(Entity&&) = delete;
        Entity& operator=(Entity&&) = delete;

        void enter_play();
        void exit_play();
        void tick(float delta_time);
        void physics_tick(float fixed_delta_time);
        void late_tick(float delta_time);
        void debug_draw_tick(float delta_time);
        void on_collision_enter(const ColliderComponent* other_collider);
        void on_collision_exit(const ColliderComponent* other_collider);
        void on_trigger_enter(const ColliderComponent* other_collider);
        void on_trigger_exit(const ColliderComponent* other_collider);

        std::string to_string() const;

        Engine& get_engine() const;

        EntityId get_id() const;
        void set_id(EntityId id);

        bool is_in_play() const;

        bool is_ticking() const;
        void set_ticking(bool is_ticking);

        TransformComponent* get_transform() const;
        RigidbodyComponent* get_rigidbody() const;

        template<ComponentType T, typename... Args>
        T* add_component(Args&&... args);

        template<ComponentType T>
        T* get_component() const;

        template<ComponentType T>
        std::vector<T*> get_components() const;

        void sort_components();
    };

    template<ComponentType T, typename... Args>
    T* Entity::add_component(Args&&... args) {
        std::unique_ptr<T> component = std::make_unique<T>(*this, std::forward<Args>(args)...);

        component->init();

        if (is_in_play()) {
            component->enter_play();
        }

        T* component_ptr = component.get();

        m_components.push_back(std::move(component));
        sort_components();

        return component_ptr;
    }

    template<ComponentType T>
    T* Entity::get_component() const {
        for (auto& c : m_components) {
            if (T* casted = dynamic_cast<T*>(c.get())) {
                return casted;
            }
        }

        return nullptr;
    }

    template<ComponentType T>
    std::vector<T*> Entity::get_components() const {
        std::vector<T*> result;
        for (auto& c : m_components) {
            if (T* casted = dynamic_cast<T*>(c.get())) {
                result.push_back(casted);
            }
        }

        return result;
    }
} // namespace hob
