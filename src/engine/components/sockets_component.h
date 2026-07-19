#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "component.h"
#include "engine/entity/entity.h"
#include "engine/math/vector2.h"

namespace hob {
    class TransformComponent;
    class Console;

    class SocketsComponent : public Component {
        struct SocketPose {
            Vector2 local_position;
            float local_rotation = 0.0f; // radians
        };

        std::unordered_map<std::string, SocketPose> m_socket_poses;
        std::unordered_map<std::string, EntityId> m_socket_ids;

    public:
        static bool cvar_show_sockets;
        static void register_cvars(Console& console);

        explicit SocketsComponent(Entity& entity);

        int get_priority() const override;

        void enter_play() override;
        void exit_play() override;
        void debug_draw_tick(float delta_time) override;

        std::string to_string() const override;

        void add_socket(const std::string& name, const Vector2& local_position, float local_rotation);
        void retain_sockets(const std::unordered_set<std::string>& names);

        TransformComponent* get_socket(const std::string& name) const;
        bool has_socket(const std::string& name) const;

    private:
        void create_socket_entity(const std::string& name, const SocketPose& pose);
        void create_socket_entities();
        void destroy_socket_entities();
    };
} // namespace hob
