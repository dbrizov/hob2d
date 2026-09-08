#pragma once

#include <concepts>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

#include "engine/components/component.h"
#include "engine/core/space.h"

namespace hob {
    class Engine;
    class ColliderComponent;
    class ColliderComponent3D;
    class LuaScriptComponent;
    class RigidbodyComponent;
    class RigidbodyComponent3D;
    class TransformComponent;
    class TransformComponent3D;

    using EntityId = int64_t;
    constexpr EntityId INVALID_ENTITY_ID = -1;

    using TickIndex = uint32_t;
    constexpr TickIndex INVALID_TICK_INDEX = std::numeric_limits<TickIndex>::max();

    template<typename T>
    concept ComponentType = std::derived_from<T, Component>;

    template<typename T>
    concept NonLuaComponentType = ComponentType<T> && !std::same_as<T, LuaScriptComponent>;

    template<typename Func, typename T>
    concept ComponentInvocable = std::invocable<Func, T*>;

    template<typename Pred, typename T>
    concept ComponentPredicate = std::predicate<Pred, T*>;

    class Entity final {
        friend class EntitySpawner;

        Engine& m_engine;
        EntityId m_id = 0;
        Space m_space = Space::Space2D;
        std::string m_name;
        std::string m_prefab_name;
        mutable std::string m_fallback_display_name;
        bool m_is_in_world = false;
        bool m_is_in_play = false;

        TickIndex m_tick_index = INVALID_TICK_INDEX; // Slot in EntitySpawner's ticking registry.
        bool m_is_ticking_request = false;

        std::vector<std::unique_ptr<Component>> m_components;
        mutable TransformComponent* m_transform = nullptr;
        mutable TransformComponent3D* m_transform_3d = nullptr;
        mutable RigidbodyComponent* m_rigidbody = nullptr;
        mutable bool m_rigidbody_resolved = false;
        mutable RigidbodyComponent3D* m_rigidbody_3d = nullptr;
        mutable bool m_rigidbody_3d_resolved = false;

        explicit Entity(Engine& engine);

    public:
        Entity(const Entity&) = delete;
        Entity& operator=(const Entity&) = delete;

        Entity(Entity&&) = delete;
        Entity& operator=(Entity&&) = delete;

        void enter_world();
        void exit_world();
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
        void on_collision_enter_3d(const ColliderComponent3D* other_collider);
        void on_collision_exit_3d(const ColliderComponent3D* other_collider);
        void on_trigger_enter_3d(const ColliderComponent3D* other_collider);
        void on_trigger_exit_3d(const ColliderComponent3D* other_collider);

        std::string to_string() const;

        Engine& get_engine() const;

        EntityId get_id() const;
        void set_id(EntityId id);

        const std::string& get_name() const;
        void set_name(std::string name);

        const std::string& get_prefab_name() const;
        void set_prefab_name(std::string name);

        const std::string& get_display_name() const;

        bool is_in_world() const;
        bool is_in_play() const;

        bool is_ticking() const;
        void set_ticking(bool is_ticking);

        Space get_space() const;

        TransformComponent* get_transform() const;
        TransformComponent3D* get_transform_3d() const;
        Entity* get_parent_entity() const;
        void get_child_entities(std::vector<Entity*>& out_children) const;

        RigidbodyComponent* get_rigidbody() const;
        RigidbodyComponent3D* get_rigidbody_3d() const;

        template<NonLuaComponentType T, typename... Args>
        T* add_component(Args&&... args);

        LuaScriptComponent* add_lua_component(std::string class_name);
        LuaScriptComponent* get_lua_component(std::string_view class_name) const;

        template<ComponentType T>
        T* get_component() const;

        template<ComponentType T, ComponentPredicate<T> Pred>
        T* get_component(Pred&& pred) const;

        template<ComponentType T>
        std::vector<T*> get_components() const;

        template<ComponentType T, ComponentInvocable<T> Func>
        void for_each_component(Func&& func) const;

        template<ComponentType T, ComponentInvocable<T> Func, ComponentPredicate<T> Until>
        void for_each_component(Func&& func, Until&& until) const;

        void sort_components();

    private:
        template<ComponentType T, typename... Args>
        T* emplace_component(Args&&... args);

        void log_duplicate_component(const Component& comp) const;
    };

    template<NonLuaComponentType T, typename... Args>
    T* Entity::add_component(Args&&... args) {
        for (const auto& c : m_components) {
            const Component& comp = *c;
            if (typeid(comp) == typeid(T)) {
                T* existing = static_cast<T*>(c.get());
                log_duplicate_component(*existing);

                return existing;
            }
        }

        return emplace_component<T>(std::forward<Args>(args)...);
    }

    template<ComponentType T, typename... Args>
    T* Entity::emplace_component(Args&&... args) {
        std::unique_ptr<T> component = std::make_unique<T>(*this, std::forward<Args>(args)...);

        component->init();

        if (is_in_world()) {
            component->enter_world();
        }

        if (is_in_play()) {
            component->enter_play();
        }

        T* component_ptr = component.get();

        m_components.push_back(std::move(component));
        sort_components();
        m_rigidbody_resolved = false;
        m_rigidbody_3d_resolved = false;

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

    template<ComponentType T, ComponentPredicate<T> Pred>
    T* Entity::get_component(Pred&& pred) const {
        for (auto& c : m_components) {
            T* casted = dynamic_cast<T*>(c.get());
            if (casted != nullptr && pred(casted)) {
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

    template<ComponentType T, ComponentInvocable<T> Func>
    void Entity::for_each_component(Func&& func) const {
        for_each_component<T>(std::forward<Func>(func), [](T*) {
            return false;
        });
    }

    template<ComponentType T, ComponentInvocable<T> Func, ComponentPredicate<T> Until>
    void Entity::for_each_component(Func&& func, Until&& until) const {
        for (auto& c : m_components) {
            if (T* casted = dynamic_cast<T*>(c.get())) {
                func(casted);
                if (until(casted)) {
                    return;
                }
            }
        }
    }
} // namespace hob
