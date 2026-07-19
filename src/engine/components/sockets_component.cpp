#include "sockets_component.h"

#include <format>
#include <iterator>

#include "engine/components/transform_component.h"
#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"

namespace hob {
    SocketsComponent::SocketsComponent(Entity& entity)
        : Component(entity) {}

    int SocketsComponent::get_priority() const {
        return component_priority::CP_SOCKETS;
    }

    void SocketsComponent::enter_play() {
        create_socket_entities();
    }

    void SocketsComponent::exit_play() {
        destroy_socket_entities();
    }

    std::string SocketsComponent::to_string() const {
        return std::format(
            "SocketsComponent(entity_id = {}, sockets = {})", get_entity().get_id(), m_socket_ids.size());
    }

    void SocketsComponent::add_socket(const std::string& name, const Vector2& local_position, float local_rotation) {
        m_socket_poses[name] = SocketPose{local_position, local_rotation};

        if (!get_entity().is_in_play()) {
            return; // enter_play() will materialize it.
        }

        if (TransformComponent* transform = get_socket(name)) {
            transform->set_local_position(local_position);
            transform->set_local_rotation(local_rotation);
        }
        else {
            create_socket_entity(name, m_socket_poses[name]);
        }
    }

    void SocketsComponent::retain_sockets(const std::unordered_set<std::string>& names) {
        EntitySpawner& spawner = get_engine().get_entity_spawner();

        for (auto it = m_socket_ids.begin(); it != m_socket_ids.end();) {
            if (names.contains(it->first)) {
                ++it;
            }
            else {
                spawner.destroy_entity(it->second);
                it = m_socket_ids.erase(it);
            }
        }

        for (auto it = m_socket_poses.begin(); it != m_socket_poses.end();) {
            it = names.contains(it->first) ? std::next(it) : m_socket_poses.erase(it);
        }
    }

    TransformComponent* SocketsComponent::get_socket(const std::string& name) const {
        auto it = m_socket_ids.find(name);
        if (it == m_socket_ids.end()) {
            return nullptr;
        }

        const Entity* socket = get_engine().get_entity_spawner().get_entity(it->second);
        return socket != nullptr ? socket->get_transform() : nullptr;
    }

    bool SocketsComponent::has_socket(const std::string& name) const {
        return m_socket_ids.contains(name);
    }

    void SocketsComponent::create_socket_entity(const std::string& name, const SocketPose& pose) {
        Entity& socket = get_engine().get_entity_spawner().spawn_entity();
        TransformComponent* transform = socket.get_transform();
        transform->set_parent(get_entity().get_transform(), false);
        transform->set_local_position(pose.local_position);
        transform->set_local_rotation(pose.local_rotation);
        m_socket_ids[name] = socket.get_id();
    }

    void SocketsComponent::create_socket_entities() {
        for (const auto& [name, pose] : m_socket_poses) {
            if (!m_socket_ids.contains(name)) {
                create_socket_entity(name, pose);
            }
        }
    }

    void SocketsComponent::destroy_socket_entities() {
        EntitySpawner& spawner = get_engine().get_entity_spawner();
        for (const auto& [name, id] : m_socket_ids) {
            spawner.destroy_entity(id);
        }
        m_socket_ids.clear();
    }
} // namespace hob
