#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>

#include "engine/animation/animation_clip.h"
#include "engine/animation/animation_track.h"
#include "engine/components/audio_component.h"
#include "engine/components/camera_component.h"
#include "engine/components/camera_component_3d.h"
#include "engine/components/directional_light_component.h"
#include "engine/components/input_component.h"
#include "engine/components/mesh_renderer_component.h"
#include "engine/components/physics/box_collider_component.h"
#include "engine/components/physics/capsule_collider_component.h"
#include "engine/components/physics/character_body_component.h"
#include "engine/components/physics/circle_collider_component.h"
#include "engine/components/physics/collider_component.h"
#include "engine/components/physics/rigidbody_component.h"
#include "engine/components/physics_3d/box_collider_component_3d.h"
#include "engine/components/physics_3d/capsule_collider_component_3d.h"
#include "engine/components/physics_3d/character_body_component_3d.h"
#include "engine/components/physics_3d/collider_component_3d.h"
#include "engine/components/physics_3d/rigidbody_component_3d.h"
#include "engine/components/physics_3d/sphere_collider_component_3d.h"
#include "engine/components/sockets_component.h"
#include "engine/components/sprite_animator_component.h"
#include "engine/components/sprite_component.h"
#include "engine/components/transform_component.h"
#include "engine/components/transform_component_3d.h"
#include "engine/core/asset.h"
#include "engine/core/engine.h"
#include "engine/core/logging.h"
#include "engine/core/systems/input/input.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/entity/entity.h"
#include "engine/entity/entity_ref.h"
#include "engine/math/constants.h"
#include "lua_bind_helpers.h"
#include "lua_meta.h"
#include "lua_schema_asset_factory.h"
#include "lua_schema_component.h"
#include "lua_schema_keys.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    namespace {
        template<typename T>
        void sort_keys(std::vector<Keyframe<T>>& keys) {
            std::stable_sort(keys.begin(), keys.end(), [](const Keyframe<T>& a, const Keyframe<T>& b) {
                return a.time < b.time;
            });
        }

        AnimationTrackRef build_texture_track(Renderer& renderer, const sol::table& t) {
            auto track = std::make_unique<TextureTrack>();
            const float fps = t.get_or("fps", 12.0f);
            const float step = fps > 0.0f ? 1.0f / fps : 0.0f;

            if (auto textures = t.get<sol::optional<sol::table>>("textures")) {
                for (int32_t i = 1; i <= textures->size(); ++i) {
                    if (TextureRef texture = resolve_texture(renderer, textures->get<sol::object>(i))) {
                        track->keys.emplace_back(static_cast<float>(track->keys.size()) * step, std::move(texture));
                    }
                }
                // Every frame (including the last) is shown for one 1/fps slice, so the loop spans all frames.
                track->length = static_cast<float>(track->keys.size()) * step;
            }

            if (auto keys = t.get<sol::optional<sol::table>>("keys")) {
                for (int32_t i = 1; i <= keys->size(); ++i) {
                    if (auto k = keys->get<sol::optional<sol::table>>(i)) {
                        if (TextureRef texture = resolve_texture(renderer, k->get<sol::object>(2))) {
                            const float time = k->get_or(1, 0.0f);
                            track->keys.emplace_back(time, std::move(texture));
                            track->length = std::max(track->length, time);
                        }
                    }
                }
            }

            sort_keys(track->keys);
            return track;
        }

        void build_track(const sol::table& t, AnimationClip& clip) {
            const std::string type = t.get<sol::optional<std::string>>("type").value_or("");

            if (type == "socket_position") {
                auto track = std::make_unique<SocketPositionTrack>();
                track->socket = t.get<sol::optional<std::string>>("socket").value_or("");
                if (auto keys = t.get<sol::optional<sol::table>>("keys")) {
                    for (int32_t i = 1; i <= keys->size(); ++i) {
                        if (auto k = keys->get<sol::optional<sol::table>>(i)) {
                            const Vector2 value = k->get<sol::optional<Vector2>>(2).value_or(Vector2());
                            track->keys.emplace_back(k->get_or(1, 0.0f), value);
                        }
                    }
                }
                sort_keys(track->keys);
                clip.add_track(std::move(track));
            }
            else if (type == "socket_rotation") {
                auto track = std::make_unique<SocketRotationTrack>();
                track->socket = t.get<sol::optional<std::string>>("socket").value_or("");
                if (auto keys = t.get<sol::optional<sol::table>>("keys")) {
                    for (int32_t i = 1; i <= keys->size(); ++i) {
                        if (auto k = keys->get<sol::optional<sol::table>>(i)) {
                            track->keys.emplace_back(k->get_or(1, 0.0f), k->get_or(2, 0.0f) * DEG_TO_RAD);
                        }
                    }
                }
                sort_keys(track->keys);
                clip.add_track(std::move(track));
            }
            else {
                log::lua.error("Unknown animation track type '{}'", type);
            }
        }
    } // namespace

    void LuaScriptSystem::bind_components() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;
        LuaComponentSchemaRegistry& schemas = m_impl->component_schemas;
        LuaAssetFactorySchemaRegistry& asset_factory_schemas = m_impl->asset_factory_schemas;

        // Component
        bind_usertype<Component>(lua, meta)
            .method("get_entity",
                    [](Component& c) {
                        return EntityRef(c.get_entity().get_id(), c.get_engine().get_entity_spawner());
                    })
            .op_tostring(&Component::to_string)
            .op_concat(&Component::to_string);

        // TransformComponent
        bind_usertype<TransformComponent>(lua, meta, Bases<Component>{})
            .method("get_position", &TransformComponent::get_position)
            .method("set_position", &TransformComponent::set_position, {"position"})
            .method("get_rotation", &TransformComponent::get_rotation)
            .method("set_rotation", &TransformComponent::set_rotation, {"radians"})
            .method("get_scale", &TransformComponent::get_scale)
            .method("set_scale", &TransformComponent::set_scale, {"scale"})
            .method("get_local_position", &TransformComponent::get_local_position)
            .method("set_local_position", &TransformComponent::set_local_position, {"position"})
            .method("get_local_rotation", &TransformComponent::get_local_rotation)
            .method("set_local_rotation", &TransformComponent::set_local_rotation, {"radians"})
            .method("get_local_scale", &TransformComponent::get_local_scale)
            .method("set_local_scale", &TransformComponent::set_local_scale, {"scale"})
            .method("get_parent", &TransformComponent::get_parent)
            .method("set_parent",
                    [](TransformComponent& self, TransformComponent* parent, sol::optional<bool> keep_world_transform) {
                        self.set_parent(parent, keep_world_transform.value_or(true));
                    },
                    {"parent", "keep_world_transform"})
            .method("get_children", &TransformComponent::get_children)
            .method("get_interpolate_physics", &TransformComponent::get_interpolate_physics)
            .method("set_interpolate_physics", &TransformComponent::set_interpolate_physics, {"value"});

        bind_component_schema<TransformComponent>(
            schemas,
            transform_key::SECTION,
            "get_transform",
            {
                {.name = transform_key::POSITION,
                 .get_method = "get_local_position",
                 .set_method = "set_local_position",
                 .reapply_on_hot_reload = false},
                {.name = transform_key::ROTATION,
                 .get_method = "get_local_rotation",
                 .set_method = "set_local_rotation",
                 .type = field_type::ANGLE,
                 .reapply_on_hot_reload = false},
                {.name = transform_key::SCALE,
                 .get_method = "get_local_scale",
                 .set_method = "set_local_scale",
                 .reapply_on_hot_reload = false},
                {"interpolate_physics", "get_interpolate_physics", "set_interpolate_physics"},
            });

        // TransformComponent3D
        bind_usertype<TransformComponent3D>(lua, meta, Bases<Component>{})
            .method("get_position", &TransformComponent3D::get_position)
            .method("set_position", &TransformComponent3D::set_position, {"position"})
            .method("get_rotation", &TransformComponent3D::get_rotation)
            .method("set_rotation", &TransformComponent3D::set_rotation, {"rotation"})
            .method("get_euler_deg", &TransformComponent3D::get_euler_deg)
            .method("set_euler_deg", &TransformComponent3D::set_euler_deg, {"euler_deg"})
            .method("get_lossy_scale", &TransformComponent3D::get_lossy_scale)
            .method("get_local_position", &TransformComponent3D::get_local_position)
            .method("set_local_position", &TransformComponent3D::set_local_position, {"position"})
            .method("get_local_rotation", &TransformComponent3D::get_local_rotation)
            .method("set_local_rotation", &TransformComponent3D::set_local_rotation, {"rotation"})
            .method("get_local_euler_deg", &TransformComponent3D::get_local_euler_deg)
            .method("set_local_euler_deg", &TransformComponent3D::set_local_euler_deg, {"euler_deg"})
            .method("get_local_scale", &TransformComponent3D::get_local_scale)
            .method("set_local_scale", &TransformComponent3D::set_local_scale, {"scale"})
            .method("get_forward", &TransformComponent3D::get_forward)
            .method("get_right", &TransformComponent3D::get_right)
            .method("get_up", &TransformComponent3D::get_up)
            .method("get_parent", &TransformComponent3D::get_parent)
            .method(
                "set_parent",
                [](TransformComponent3D& self, TransformComponent3D* parent, sol::optional<bool> keep_world_transform) {
                    self.set_parent(parent, keep_world_transform.value_or(true));
                },
                {"parent", "keep_world_transform"})
            .method("get_children", &TransformComponent3D::get_children)
            .method("get_interpolate_physics", &TransformComponent3D::get_interpolate_physics)
            .method("set_interpolate_physics", &TransformComponent3D::set_interpolate_physics, {"value"});

        bind_component_schema<TransformComponent3D>(
            schemas,
            transform_3d_key::SECTION,
            "get_transform_3d",
            {
                {.name = transform_key::POSITION,
                 .get_method = "get_local_position",
                 .set_method = "set_local_position",
                 .type = field_type::VECTOR3,
                 .reapply_on_hot_reload = false},
                {.name = transform_key::ROTATION,
                 .get_method = "get_local_euler_deg",
                 .set_method = "set_local_euler_deg",
                 .type = field_type::EULER_DEG,
                 .reapply_on_hot_reload = false},
                {.name = transform_key::SCALE,
                 .get_method = "get_local_scale",
                 .set_method = "set_local_scale",
                 .type = field_type::VECTOR3,
                 .reapply_on_hot_reload = false},
                {"interpolate_physics", "get_interpolate_physics", "set_interpolate_physics"},
            },
            Space::Space3D);

        // RigidbodyComponent3D
        bind_usertype<RigidbodyComponent3D>(lua, meta, Bases<Component>{})
            .method("has_body", &RigidbodyComponent3D::has_body)
            .method("is_awake", &RigidbodyComponent3D::is_awake)
            .method("get_body_type", &RigidbodyComponent3D::get_body_type)
            .method("set_body_type", &RigidbodyComponent3D::set_body_type, {"body_type"})
            .method("get_motion_locks",
                    [](const RigidbodyComponent3D& self) {
                        return static_cast<int64_t>(self.get_motion_locks());
                    })
            .method("set_motion_locks",
                    [](RigidbodyComponent3D& self, int64_t locks) {
                        self.set_motion_locks(static_cast<uint64_t>(locks));
                    },
                    {"locks"})
            .method("get_gravity_scale", &RigidbodyComponent3D::get_gravity_scale)
            .method("set_gravity_scale", &RigidbodyComponent3D::set_gravity_scale, {"value"})
            .method("get_linear_damping", &RigidbodyComponent3D::get_linear_damping)
            .method("set_linear_damping", &RigidbodyComponent3D::set_linear_damping, {"value"})
            .method("get_angular_damping", &RigidbodyComponent3D::get_angular_damping)
            .method("set_angular_damping", &RigidbodyComponent3D::set_angular_damping, {"value"})
            .method("get_velocity", &RigidbodyComponent3D::get_velocity)
            .method("set_velocity", &RigidbodyComponent3D::set_velocity, {"velocity"})
            .method("get_angular_velocity", &RigidbodyComponent3D::get_angular_velocity)
            .method("set_angular_velocity", &RigidbodyComponent3D::set_angular_velocity, {"angular_velocity"})
            .method("get_position", &RigidbodyComponent3D::get_position)
            .method("set_position", &RigidbodyComponent3D::set_position, {"position"})
            .method("get_rotation", &RigidbodyComponent3D::get_rotation)
            .method("set_rotation", &RigidbodyComponent3D::set_rotation, {"rotation"})
            .method("apply_force", &RigidbodyComponent3D::apply_force, {"force"})
            .method("apply_impulse", &RigidbodyComponent3D::apply_impulse, {"impulse"})
            .method("apply_torque", &RigidbodyComponent3D::apply_torque, {"torque"});

        bind_component_schema<RigidbodyComponent3D>(lua,
                                                    meta,
                                                    schemas,
                                                    "rigidbody_3d",
                                                    "add_rigidbody_3d",
                                                    "get_rigidbody_3d",
                                                    {
                                                        {.name = "body_type",
                                                         .get_method = "get_body_type",
                                                         .set_method = "set_body_type",
                                                         .type = field_type::ENUM,
                                                         .enum_name = LuaTypeName<BodyType>::value},
                                                        {.name = "motion_locks",
                                                         .get_method = "get_motion_locks",
                                                         .set_method = "set_motion_locks",
                                                         .type = field_type::BITMASK,
                                                         .enum_name = LuaTypeName<MotionLock>::value},
                                                        {"gravity_scale", "get_gravity_scale", "set_gravity_scale"},
                                                        {.name = "linear_damping",
                                                         .get_method = "get_linear_damping",
                                                         .set_method = "set_linear_damping",
                                                         .min = 0.0f,
                                                         .max = MAX_FLOAT},
                                                        {.name = "angular_damping",
                                                         .get_method = "get_angular_damping",
                                                         .set_method = "set_angular_damping",
                                                         .min = 0.0f,
                                                         .max = MAX_FLOAT},
                                                    },
                                                    Space::Space3D);

        // ColliderComponent3D
        bind_usertype<ColliderComponent3D>(lua, meta, Bases<Component>{})
            .method("get_density", &ColliderComponent3D::get_density)
            .method("set_density", &ColliderComponent3D::set_density, {"density"})
            .method("get_friction", &ColliderComponent3D::get_friction)
            .method("set_friction", &ColliderComponent3D::set_friction, {"friction"})
            .method("get_bounciness", &ColliderComponent3D::get_bounciness)
            .method("set_bounciness", &ColliderComponent3D::set_bounciness, {"bounciness"})
            .method("get_collision_layer",
                    [](const ColliderComponent3D& self) {
                        return static_cast<int64_t>(self.get_collision_layer());
                    })
            .method("set_collision_layer",
                    [](ColliderComponent3D& self, int64_t layer) {
                        self.set_collision_layer(static_cast<uint64_t>(layer));
                    },
                    {"layer"})
            .method("get_collision_mask",
                    [](const ColliderComponent3D& self) {
                        return static_cast<int64_t>(self.get_collision_mask());
                    })
            .method("set_collision_mask",
                    [](ColliderComponent3D& self, int64_t mask) {
                        self.set_collision_mask(static_cast<uint64_t>(mask));
                    },
                    {"mask"})
            .method("is_trigger", &ColliderComponent3D::is_trigger)
            .method("set_trigger", &ColliderComponent3D::set_trigger, {"trigger"});

        const LuaComponentSchemaField collider_density{
            .name = "density", .get_method = "get_density", .set_method = "set_density", .min = 0.0f, .max = MAX_FLOAT};
        const LuaComponentSchemaField collider_friction{
            .name = "friction", .get_method = "get_friction", .set_method = "set_friction", .min = 0.0f, .max = 1.0f};
        const LuaComponentSchemaField collider_bounciness{.name = "bounciness",
                                                          .get_method = "get_bounciness",
                                                          .set_method = "set_bounciness",
                                                          .min = 0.0f,
                                                          .max = 1.0f};
        const LuaComponentSchemaField collider_layer{.name = "collision_layer",
                                                     .get_method = "get_collision_layer",
                                                     .set_method = "set_collision_layer",
                                                     .type = field_type::BITMASK,
                                                     .enum_name = LuaTypeName<CollisionLayer>::value};
        const LuaComponentSchemaField collider_mask{.name = "collision_mask",
                                                    .get_method = "get_collision_mask",
                                                    .set_method = "set_collision_mask",
                                                    .type = field_type::BITMASK,
                                                    .enum_name = LuaTypeName<CollisionLayer>::value};
        const LuaComponentSchemaField collider_trigger{"trigger", "is_trigger", "set_trigger"};

        // BoxColliderComponent3D
        bind_usertype<BoxColliderComponent3D>(lua, meta, Bases<ColliderComponent3D, Component>{})
            .method("get_box", &BoxColliderComponent3D::get_box)
            .method("set_box", &BoxColliderComponent3D::set_box, {"box"})
            .method("get_scaled_box", &BoxColliderComponent3D::get_scaled_box);

        bind_component_schema<BoxColliderComponent3D>(
            lua,
            meta,
            schemas,
            "box_collider_3d",
            "add_box_collider_3d",
            "get_box_collider_3d",
            {
                {.name = "box", .get_method = "get_box", .set_method = "set_box", .type = field_type::AABB3},
                collider_density,
                collider_friction,
                collider_bounciness,
                collider_layer,
                collider_mask,
                collider_trigger,
            },
            Space::Space3D);

        // SphereColliderComponent3D
        bind_usertype<SphereColliderComponent3D>(lua, meta, Bases<ColliderComponent3D, Component>{})
            .method("get_center", &SphereColliderComponent3D::get_center)
            .method("set_center", &SphereColliderComponent3D::set_center, {"center"})
            .method("get_radius", &SphereColliderComponent3D::get_radius)
            .method("set_radius", &SphereColliderComponent3D::set_radius, {"radius"})
            .method("get_scaled_radius", &SphereColliderComponent3D::get_scaled_radius);

        bind_component_schema<SphereColliderComponent3D>(
            lua,
            meta,
            schemas,
            "sphere_collider_3d",
            "add_sphere_collider_3d",
            "get_sphere_collider_3d",
            {
                {.name = "center", .get_method = "get_center", .set_method = "set_center", .type = field_type::VECTOR3},
                {.name = "radius",
                 .get_method = "get_radius",
                 .set_method = "set_radius",
                 .min = 0.0f,
                 .max = MAX_FLOAT},
                collider_density,
                collider_friction,
                collider_bounciness,
                collider_layer,
                collider_mask,
                collider_trigger,
            },
            Space::Space3D);

        // CapsuleColliderComponent3D
        bind_usertype<CapsuleColliderComponent3D>(lua, meta, Bases<ColliderComponent3D, Component>{})
            .method("get_center", &CapsuleColliderComponent3D::get_center)
            .method("set_center", &CapsuleColliderComponent3D::set_center, {"center"})
            .method("get_radius", &CapsuleColliderComponent3D::get_radius)
            .method("set_radius", &CapsuleColliderComponent3D::set_radius, {"radius"})
            .method("get_height", &CapsuleColliderComponent3D::get_height)
            .method("set_height", &CapsuleColliderComponent3D::set_height, {"height"});

        bind_component_schema<CapsuleColliderComponent3D>(
            lua,
            meta,
            schemas,
            "capsule_collider_3d",
            "add_capsule_collider_3d",
            "get_capsule_collider_3d",
            {
                {.name = "center", .get_method = "get_center", .set_method = "set_center", .type = field_type::VECTOR3},
                {.name = "radius",
                 .get_method = "get_radius",
                 .set_method = "set_radius",
                 .min = 0.0f,
                 .max = MAX_FLOAT},
                {.name = "height",
                 .get_method = "get_height",
                 .set_method = "set_height",
                 .min = 0.0f,
                 .max = MAX_FLOAT},
                collider_density,
                collider_friction,
                collider_bounciness,
                collider_layer,
                collider_mask,
                collider_trigger,
            },
            Space::Space3D);

        // CharacterBodyComponent3D
        bind_usertype<CharacterBodyComponent3D>(lua, meta, Bases<Component>{})
            .method("get_center", &CharacterBodyComponent3D::get_center)
            .method("set_center", &CharacterBodyComponent3D::set_center, {"center"})
            .method("get_radius", &CharacterBodyComponent3D::get_radius)
            .method("set_radius", &CharacterBodyComponent3D::set_radius, {"radius"})
            .method("get_height", &CharacterBodyComponent3D::get_height)
            .method("set_height", &CharacterBodyComponent3D::set_height, {"height"})
            .method("get_collision_layer",
                    [](const CharacterBodyComponent3D& self) {
                        return static_cast<int64_t>(self.get_collision_layer());
                    })
            .method("set_collision_layer",
                    [](CharacterBodyComponent3D& self, int64_t layer) {
                        self.set_collision_layer(static_cast<uint64_t>(layer));
                    },
                    {"layer"})
            .method("get_collision_mask",
                    [](const CharacterBodyComponent3D& self) {
                        return static_cast<int64_t>(self.get_collision_mask());
                    })
            .method("set_collision_mask",
                    [](CharacterBodyComponent3D& self, int64_t mask) {
                        self.set_collision_mask(static_cast<uint64_t>(mask));
                    },
                    {"mask"})
            .method("get_solver_ignore_mask",
                    [](const CharacterBodyComponent3D& self) {
                        return static_cast<int64_t>(self.get_solver_ignore_mask());
                    })
            .method("set_solver_ignore_mask",
                    [](CharacterBodyComponent3D& self, int64_t mask) {
                        self.set_solver_ignore_mask(static_cast<uint64_t>(mask));
                    },
                    {"mask"})
            .method("get_max_slope_deg", &CharacterBodyComponent3D::get_max_slope_deg)
            .method("set_max_slope_deg", &CharacterBodyComponent3D::set_max_slope_deg, {"degrees"})
            .method("move_and_slide", &CharacterBodyComponent3D::move_and_slide, {"velocity", "fixed_dt"})
            .method("is_on_floor", &CharacterBodyComponent3D::is_on_floor)
            .method("get_floor_normal", &CharacterBodyComponent3D::get_floor_normal)
            .method("get_velocity", &CharacterBodyComponent3D::get_velocity)
            .method("set_velocity", &CharacterBodyComponent3D::set_velocity, {"velocity"})
            .method("get_position", &CharacterBodyComponent3D::get_position)
            .method("set_position", &CharacterBodyComponent3D::set_position, {"position"})
            .method("get_rotation", &CharacterBodyComponent3D::get_rotation)
            .method("set_rotation", &CharacterBodyComponent3D::set_rotation, {"rotation"});

        bind_component_schema<CharacterBodyComponent3D>(
            lua,
            meta,
            schemas,
            "character_body_3d",
            "add_character_body_3d",
            "get_character_body_3d",
            {
                {.name = "center", .get_method = "get_center", .set_method = "set_center", .type = field_type::VECTOR3},
                {.name = "radius",
                 .get_method = "get_radius",
                 .set_method = "set_radius",
                 .min = 0.0f,
                 .max = MAX_FLOAT},
                {.name = "height",
                 .get_method = "get_height",
                 .set_method = "set_height",
                 .min = 0.0f,
                 .max = MAX_FLOAT},
                {.name = "collision_layer",
                 .get_method = "get_collision_layer",
                 .set_method = "set_collision_layer",
                 .type = field_type::BITMASK,
                 .enum_name = LuaTypeName<CollisionLayer>::value},
                {.name = "collision_mask",
                 .get_method = "get_collision_mask",
                 .set_method = "set_collision_mask",
                 .type = field_type::BITMASK,
                 .enum_name = LuaTypeName<CollisionLayer>::value},
                {.name = "solver_ignore_mask",
                 .get_method = "get_solver_ignore_mask",
                 .set_method = "set_solver_ignore_mask",
                 .type = field_type::BITMASK,
                 .enum_name = LuaTypeName<CollisionLayer>::value},
                {.name = "max_slope_deg",
                 .get_method = "get_max_slope_deg",
                 .set_method = "set_max_slope_deg",
                 .min = 0.0f,
                 .max = 89.0f},
            },
            Space::Space3D);

        // DirectionalLightComponent
        bind_usertype<DirectionalLightComponent>(lua, meta, Bases<Component>{})
            .method("get_color", &DirectionalLightComponent::get_color)
            .method("set_color", &DirectionalLightComponent::set_color, {"color"})
            .method("get_intensity", &DirectionalLightComponent::get_intensity)
            .method("set_intensity", &DirectionalLightComponent::set_intensity, {"intensity"})
            .method("get_sky_color", &DirectionalLightComponent::get_sky_color)
            .method("set_sky_color", &DirectionalLightComponent::set_sky_color, {"color"})
            .method("get_ground_color", &DirectionalLightComponent::get_ground_color)
            .method("set_ground_color", &DirectionalLightComponent::set_ground_color, {"color"})
            .method("get_fog_density", &DirectionalLightComponent::get_fog_density)
            .method("set_fog_density", &DirectionalLightComponent::set_fog_density, {"density"})
            .method("get_fog_height", &DirectionalLightComponent::get_fog_height)
            .method("set_fog_height", &DirectionalLightComponent::set_fog_height, {"height"})
            .method("get_fog_falloff", &DirectionalLightComponent::get_fog_falloff)
            .method("set_fog_falloff", &DirectionalLightComponent::set_fog_falloff, {"falloff"})
            .method("get_fog_anisotropy", &DirectionalLightComponent::get_fog_anisotropy)
            .method("set_fog_anisotropy", &DirectionalLightComponent::set_fog_anisotropy, {"anisotropy"})
            .method("get_fog_color", &DirectionalLightComponent::get_fog_color)
            .method("set_fog_color", &DirectionalLightComponent::set_fog_color, {"color"})
            .method("get_direction", &DirectionalLightComponent::get_direction);

        bind_component_schema<DirectionalLightComponent>(
            lua,
            meta,
            schemas,
            "directional_light",
            "add_directional_light",
            "get_directional_light",
            {
                {.name = "color", .get_method = "get_color", .set_method = "set_color", .type = field_type::COLOR},
                {.name = "intensity",
                 .get_method = "get_intensity",
                 .set_method = "set_intensity",
                 .min = 0.0f,
                 .max = MAX_FLOAT},
                {.name = "sky_color",
                 .get_method = "get_sky_color",
                 .set_method = "set_sky_color",
                 .type = field_type::COLOR},
                {.name = "ground_color",
                 .get_method = "get_ground_color",
                 .set_method = "set_ground_color",
                 .type = field_type::COLOR},
                {.name = "fog_density",
                 .get_method = "get_fog_density",
                 .set_method = "set_fog_density",
                 .min = 0.0f,
                 .max = MAX_FLOAT},
                {.name = "fog_height",
                 .get_method = "get_fog_height",
                 .set_method = "set_fog_height",
                 .min = -MAX_FLOAT,
                 .max = MAX_FLOAT},
                {.name = "fog_falloff",
                 .get_method = "get_fog_falloff",
                 .set_method = "set_fog_falloff",
                 .min = 0.01f,
                 .max = MAX_FLOAT},
                {.name = "fog_anisotropy",
                 .get_method = "get_fog_anisotropy",
                 .set_method = "set_fog_anisotropy",
                 .min = -0.99f,
                 .max = 0.99f},
                {.name = "fog_color",
                 .get_method = "get_fog_color",
                 .set_method = "set_fog_color",
                 .type = field_type::COLOR},
            },
            Space::Space3D);

        // CameraComponent3D
        bind_usertype<CameraComponent3D>(lua, meta, Bases<Component>{})
            .method("get_fov_deg", &CameraComponent3D::get_fov_deg)
            .method("set_fov_deg", &CameraComponent3D::set_fov_deg, {"value"})
            .method("get_near_plane", &CameraComponent3D::get_near_plane)
            .method("set_near_plane", &CameraComponent3D::set_near_plane, {"value"})
            .method("get_far_plane", &CameraComponent3D::get_far_plane)
            .method("set_far_plane", &CameraComponent3D::set_far_plane, {"value"})
            .method_sig(
                "world_to_screen",
                [](const CameraComponent3D& self, const Vector3& world_pos) -> sol::optional<Vector2> {
                    Vector2 screen_pos;
                    if (!self.world_to_screen(world_pos, screen_pos)) {
                        return sol::nullopt;
                    }
                    return screen_pos;
                },
                "(world_pos: Vector3): Vector2?")
            .method("screen_to_ray", &CameraComponent3D::screen_to_ray, {"screen_pos"});

        bind_component_schema<CameraComponent3D>(lua,
                                                 meta,
                                                 schemas,
                                                 "camera_3d",
                                                 "add_camera_3d",
                                                 "get_camera_3d",
                                                 {
                                                     {.name = "fov_deg",
                                                      .get_method = "get_fov_deg",
                                                      .set_method = "set_fov_deg",
                                                      .min = 1.0f,
                                                      .max = 179.0f},
                                                     {.name = "near_plane",
                                                      .get_method = "get_near_plane",
                                                      .set_method = "set_near_plane",
                                                      .min = 0.001f,
                                                      .max = MAX_FLOAT},
                                                     {.name = "far_plane",
                                                      .get_method = "get_far_plane",
                                                      .set_method = "set_far_plane",
                                                      .min = 0.001f,
                                                      .max = MAX_FLOAT},
                                                 },
                                                 Space::Space3D);

        // MeshRendererComponent
        bind_usertype<MeshRendererComponent>(lua, meta, Bases<Component>{})
            .method("get_mesh", &MeshRendererComponent::get_mesh)
            .method_sig(
                "set_mesh",
                [](MeshRendererComponent& self, const sol::object& value) {
                    if (!value.valid()) {
                        self.set_mesh(nullptr);
                    }
                    else if (value.is<Mesh>()) {
                        self.set_mesh(value.as<MeshRef>());
                    }
                    else {
                        log::lua.error("MeshRendererComponent:set_mesh expects a Mesh or nil");
                    }
                },
                "(mesh: Mesh|nil)")
            .method("get_material", sol::resolve<MaterialRef()>(&MeshRendererComponent::get_material))
            .method_sig(
                "get_material_const",
                [](const MeshRendererComponent& self) -> const MaterialRef& {
                    return self.get_material();
                },
                "(): Material?")
            .method_sig(
                "set_material",
                [](MeshRendererComponent& self, const sol::object& value) {
                    if (!value.valid()) {
                        self.set_material(nullptr);
                    }
                    else if (value.is<Material>()) {
                        self.set_material(value.as<MaterialRef>());
                    }
                    else {
                        log::lua.error("MeshRendererComponent:set_material expects a Material or nil");
                    }
                },
                "(material: Material|nil)")
            .method("get_local_bounds", &MeshRendererComponent::get_local_bounds)
            .method("get_world_bounds", &MeshRendererComponent::get_world_bounds);

        bind_component_schema<MeshRendererComponent>(
            lua,
            meta,
            schemas,
            "mesh_renderer",
            "add_mesh_renderer",
            "get_mesh_renderer",
            {
                {.name = "mesh", .get_method = "get_mesh", .set_method = "set_mesh", .type = field_type::MESH},
                {.name = "material",
                 .get_method = "get_material_const",
                 .set_method = "set_material",
                 .type = field_type::MATERIAL},
            },
            Space::Space3D);

        // RigidbodyComponent
        bind_enum<BodyType>(lua,
                            meta,
                            {
                                {"Static", BodyType::Static},
                                {"Dynamic", BodyType::Dynamic},
                                {"Kinematic", BodyType::Kinematic},
                            });

        bind_usertype<RigidbodyComponent>(lua, meta, Bases<Component>{})
            .method("get_body_type", &RigidbodyComponent::get_body_type)
            .method("set_body_type", &RigidbodyComponent::set_body_type, {"body_type"})
            .method("has_fixed_rotation", &RigidbodyComponent::has_fixed_rotation)
            .method("set_fixed_rotation", &RigidbodyComponent::set_fixed_rotation, {"fixed"})
            .method("get_velocity", &RigidbodyComponent::get_velocity)
            .method("set_velocity", &RigidbodyComponent::set_velocity, {"velocity"})
            .method("get_position", &RigidbodyComponent::get_position)
            .method("set_position", &RigidbodyComponent::set_position, {"position"})
            .method("get_rotation", &RigidbodyComponent::get_rotation)
            .method("set_rotation", &RigidbodyComponent::set_rotation, {"radians"});

        bind_component_schema<RigidbodyComponent>(lua,
                                                  meta,
                                                  schemas,
                                                  "rigidbody",
                                                  "add_rigidbody",
                                                  "get_rigidbody",
                                                  {
                                                      {.name = "body_type",
                                                       .get_method = "get_body_type",
                                                       .set_method = "set_body_type",
                                                       .type = field_type::ENUM,
                                                       .enum_name = LuaTypeName<BodyType>::value},
                                                      {"fixed_rotation", "has_fixed_rotation", "set_fixed_rotation"},
                                                  });

        // CharacterBodyComponent
        bind_usertype<CharacterBodyComponent>(lua, meta, Bases<Component>{})
            // Masks/layers are uint64_t (matching Box2D), but sol2 can't push values above
            // MAX_INT64, so reinterpret to/from int64_t at the Lua boundary (bit pattern is preserved).
            .method("get_collision_layer",
                    [](const CharacterBodyComponent& c) {
                        return static_cast<int64_t>(c.get_collision_layer());
                    })
            .method("set_collision_layer",
                    [](CharacterBodyComponent& c, int64_t layer) {
                        c.set_collision_layer(static_cast<uint64_t>(layer));
                    },
                    {"layer"})
            .method("get_collision_mask",
                    [](const CharacterBodyComponent& c) {
                        return static_cast<int64_t>(c.get_collision_mask());
                    })
            .method("set_collision_mask",
                    [](CharacterBodyComponent& c, int64_t mask) {
                        c.set_collision_mask(static_cast<uint64_t>(mask));
                    },
                    {"mask"})
            .method("get_solver_ignore_mask",
                    [](const CharacterBodyComponent& c) {
                        return static_cast<int64_t>(c.get_solver_ignore_mask());
                    })
            .method("set_solver_ignore_mask",
                    [](CharacterBodyComponent& c, int64_t mask) {
                        c.set_solver_ignore_mask(static_cast<uint64_t>(mask));
                    },
                    {"mask"})
            .method("get_capsule", &CharacterBodyComponent::get_capsule)
            .method("set_capsule", &CharacterBodyComponent::set_capsule, {"capsule"})
            .method("move_and_slide", &CharacterBodyComponent::move_and_slide, {"velocity", "fixed_dt"})
            .method("get_velocity", &CharacterBodyComponent::get_velocity)
            .method("set_velocity", &CharacterBodyComponent::set_velocity, {"velocity"})
            .method("get_position", &CharacterBodyComponent::get_position)
            .method("set_position", &CharacterBodyComponent::set_position, {"position"})
            .method("get_rotation", &CharacterBodyComponent::get_rotation)
            .method("set_rotation", &CharacterBodyComponent::set_rotation, {"radians"});

        bind_component_schema<CharacterBodyComponent>(lua,
                                                      meta,
                                                      schemas,
                                                      "character_body",
                                                      "add_character_body",
                                                      "get_character_body",
                                                      {
                                                          {.name = "collision_layer",
                                                           .get_method = "get_collision_layer",
                                                           .set_method = "set_collision_layer",
                                                           .type = field_type::BITMASK,
                                                           .enum_name = LuaTypeName<CollisionLayer>::value},
                                                          {.name = "collision_mask",
                                                           .get_method = "get_collision_mask",
                                                           .set_method = "set_collision_mask",
                                                           .type = field_type::BITMASK,
                                                           .enum_name = LuaTypeName<CollisionLayer>::value},
                                                          {.name = "solver_ignore_mask",
                                                           .get_method = "get_solver_ignore_mask",
                                                           .set_method = "set_solver_ignore_mask",
                                                           .type = field_type::BITMASK,
                                                           .enum_name = LuaTypeName<CollisionLayer>::value},
                                                          {.name = "capsule",
                                                           .get_method = "get_capsule",
                                                           .set_method = "set_capsule",
                                                           .type = field_type::CAPSULE},
                                                      });

        // ColliderComponent
        bind_usertype<ColliderComponent>(lua, meta, Bases<Component>{})
            .method("get_density", &ColliderComponent::get_density)
            .method("set_density", &ColliderComponent::set_density, {"density"})
            .method("get_friction", &ColliderComponent::get_friction)
            .method("set_friction", &ColliderComponent::set_friction, {"friction"})
            .method("get_bounciness", &ColliderComponent::get_bounciness)
            .method("set_bounciness", &ColliderComponent::set_bounciness, {"bounciness"})
            // Masks/layers are uint64_t (matching Box2D), but sol2 can't push values above
            // MAX_INT64, so reinterpret to/from int64_t at the Lua boundary (bit pattern is preserved).
            .method("get_collision_layer",
                    [](const ColliderComponent& c) {
                        return static_cast<int64_t>(c.get_collision_layer());
                    })
            .method("set_collision_layer",
                    [](ColliderComponent& c, int64_t layer) {
                        c.set_collision_layer(static_cast<uint64_t>(layer));
                    },
                    {"layer"})
            .method("get_collision_mask",
                    [](const ColliderComponent& c) {
                        return static_cast<int64_t>(c.get_collision_mask());
                    })
            .method("set_collision_mask",
                    [](ColliderComponent& c, int64_t mask) {
                        c.set_collision_mask(static_cast<uint64_t>(mask));
                    },
                    {"mask"})
            .method("is_trigger", &ColliderComponent::is_trigger)
            .method("set_trigger", &ColliderComponent::set_trigger, {"trigger"});

        // BoxColliderComponent
        bind_usertype<BoxColliderComponent>(lua, meta, Bases<ColliderComponent, Component>{})
            .method("get_aabb", &BoxColliderComponent::get_aabb)
            .method("set_aabb", &BoxColliderComponent::set_aabb, {"aabb"})
            .method("get_scaled_aabb", &BoxColliderComponent::get_scaled_aabb);

        bind_component_schema<BoxColliderComponent>(
            lua,
            meta,
            schemas,
            "box_collider",
            "add_box_collider",
            "get_box_collider",
            {
                {.name = "aabb", .get_method = "get_aabb", .set_method = "set_aabb", .type = field_type::AABB},
                {.name = "density",
                 .get_method = "get_density",
                 .set_method = "set_density",
                 .min = 0.0f,
                 .max = MAX_FLOAT},
                {.name = "friction",
                 .get_method = "get_friction",
                 .set_method = "set_friction",
                 .min = 0.0f,
                 .max = 1.0f},
                {.name = "bounciness",
                 .get_method = "get_bounciness",
                 .set_method = "set_bounciness",
                 .min = 0.0f,
                 .max = 1.0f},
                {.name = "collision_layer",
                 .get_method = "get_collision_layer",
                 .set_method = "set_collision_layer",
                 .type = field_type::BITMASK,
                 .enum_name = LuaTypeName<CollisionLayer>::value},
                {.name = "collision_mask",
                 .get_method = "get_collision_mask",
                 .set_method = "set_collision_mask",
                 .type = field_type::BITMASK,
                 .enum_name = LuaTypeName<CollisionLayer>::value},
                {"trigger", "is_trigger", "set_trigger"},
            });

        // CapsuleColliderComponent
        bind_usertype<CapsuleColliderComponent>(lua, meta, Bases<ColliderComponent, Component>{})
            .method("get_capsule", &CapsuleColliderComponent::get_capsule)
            .method("set_capsule", &CapsuleColliderComponent::set_capsule, {"capsule"})
            .method("get_scaled_capsule", &CapsuleColliderComponent::get_scaled_capsule);

        bind_component_schema<CapsuleColliderComponent>(lua,
                                                        meta,
                                                        schemas,
                                                        "capsule_collider",
                                                        "add_capsule_collider",
                                                        "get_capsule_collider",
                                                        {
                                                            {.name = "capsule",
                                                             .get_method = "get_capsule",
                                                             .set_method = "set_capsule",
                                                             .type = field_type::CAPSULE},
                                                            {.name = "density",
                                                             .get_method = "get_density",
                                                             .set_method = "set_density",
                                                             .min = 0.0f,
                                                             .max = MAX_FLOAT},
                                                            {.name = "friction",
                                                             .get_method = "get_friction",
                                                             .set_method = "set_friction",
                                                             .min = 0.0f,
                                                             .max = 1.0f},
                                                            {.name = "bounciness",
                                                             .get_method = "get_bounciness",
                                                             .set_method = "set_bounciness",
                                                             .min = 0.0f,
                                                             .max = 1.0f},
                                                            {.name = "collision_layer",
                                                             .get_method = "get_collision_layer",
                                                             .set_method = "set_collision_layer",
                                                             .type = field_type::BITMASK,
                                                             .enum_name = LuaTypeName<CollisionLayer>::value},
                                                            {.name = "collision_mask",
                                                             .get_method = "get_collision_mask",
                                                             .set_method = "set_collision_mask",
                                                             .type = field_type::BITMASK,
                                                             .enum_name = LuaTypeName<CollisionLayer>::value},
                                                            {"trigger", "is_trigger", "set_trigger"},
                                                        });

        // CircleColliderComponent
        bind_usertype<CircleColliderComponent>(lua, meta, Bases<ColliderComponent, Component>{})
            .method("get_circle", &CircleColliderComponent::get_circle)
            .method("set_circle", &CircleColliderComponent::set_circle, {"circle"})
            .method("get_scaled_circle", &CircleColliderComponent::get_scaled_circle);

        bind_component_schema<CircleColliderComponent>(
            lua,
            meta,
            schemas,
            "circle_collider",
            "add_circle_collider",
            "get_circle_collider",
            {
                {.name = "circle", .get_method = "get_circle", .set_method = "set_circle", .type = field_type::CIRCLE},
                {.name = "density",
                 .get_method = "get_density",
                 .set_method = "set_density",
                 .min = 0.0f,
                 .max = MAX_FLOAT},
                {.name = "friction",
                 .get_method = "get_friction",
                 .set_method = "set_friction",
                 .min = 0.0f,
                 .max = 1.0f},
                {.name = "bounciness",
                 .get_method = "get_bounciness",
                 .set_method = "set_bounciness",
                 .min = 0.0f,
                 .max = 1.0f},
                {.name = "collision_layer",
                 .get_method = "get_collision_layer",
                 .set_method = "set_collision_layer",
                 .type = field_type::BITMASK,
                 .enum_name = LuaTypeName<CollisionLayer>::value},
                {.name = "collision_mask",
                 .get_method = "get_collision_mask",
                 .set_method = "set_collision_mask",
                 .type = field_type::BITMASK,
                 .enum_name = LuaTypeName<CollisionLayer>::value},
                {"trigger", "is_trigger", "set_trigger"},
            });

        // InputComponent
        bind_enum<InputEventType>(lua,
                                  meta,
                                  {
                                      {"Axis", InputEventType::Axis},
                                      {"Pressed", InputEventType::Pressed},
                                      {"Released", InputEventType::Released},
                                  });

        bind_usertype<InputComponent>(lua, meta, Bases<Component>{})
            .method_sig(
                "bind_axis",
                [](InputComponent& self, const std::string& name, const sol::protected_function& fn) {
                    auto handler = [fn, name](float v) {
                        auto result = fn(v);
                        if (!result.valid()) {
                            const sol::error err = result;
                            log::sol2.error("Lua error in axis '{}' handler: {}", name, err.what());
                        }
                    };

                    return self.bind_axis(name, std::move(handler));
                },
                "(name: string, fn: fun(value: number)): integer")
            .method_sig(
                "unbind_axis",
                [](InputComponent& self, std::string_view name, BindingId id) {
                    self.unbind_axis(name, id);
                },
                "(name: string, id: integer)")
            .method_sig(
                "bind_action",
                [](InputComponent& self,
                   const std::string& name,
                   InputEventType type,
                   const sol::protected_function& fn) {
                    auto handler = [fn, name]() {
                        auto result = fn();
                        if (!result.valid()) {
                            const sol::error err = result;
                            log::sol2.error("Lua error in action '{}' handler: {}", name, err.what());
                        }
                    };

                    return self.bind_action(name, type, std::move(handler));
                },
                "(name: string, type: InputEventType, fn: fun()): integer")
            .method_sig(
                "unbind_action",
                [](InputComponent& self, std::string_view name, BindingId id) {
                    self.unbind_action(name, id);
                },
                "(name: string, id: integer)")
            .method("clear_all_bindings", &InputComponent::clear_all_bindings);

        bind_component_schema<InputComponent>(lua, meta, schemas, "input", "add_input", "get_input", {}, std::nullopt);

        // SpriteComponent
        bind_usertype<SpriteComponent>(lua, meta, Bases<Component>{})
            .method("get_texture", &SpriteComponent::get_texture)
            .method_sig(
                "set_texture",
                [](SpriteComponent& self, const sol::object& value) {
                    if (!value.valid()) {
                        self.clear_texture();
                    }
                    else if (value.is<Texture>()) {
                        self.set_texture(value.as<TextureRef>());
                    }
                    else if (value.is<std::string>()) {
                        self.set_texture(value.as<std::string_view>());
                    }
                    else {
                        log::lua.error("SpriteComponent:set_texture expects a string path or a Texture");
                    }
                },
                "(path_or_texture: string|Texture|nil)")
            .method("clear_texture", &SpriteComponent::clear_texture)
            .method("get_material", sol::resolve<MaterialRef()>(&SpriteComponent::get_material))
            .method_sig(
                "get_material_const",
                [](const SpriteComponent& self) -> const MaterialRef& {
                    return self.get_material();
                },
                "(): Material?")
            .method_sig(
                "set_material",
                [](SpriteComponent& self, const sol::object& value) {
                    if (!value.valid()) {
                        self.set_material(nullptr);
                    }
                    else if (value.is<Material>()) {
                        self.set_material(value.as<MaterialRef>());
                    }
                    else {
                        log::lua.error("SpriteComponent:set_material expects a Material or nil");
                    }
                },
                "(material: Material|nil)")
            .method("get_pivot", &SpriteComponent::get_pivot)
            .method("set_pivot", &SpriteComponent::set_pivot, {"pivot"})
            .method("get_scale", &SpriteComponent::get_scale)
            .method("set_scale", &SpriteComponent::set_scale, {"scale"})
            .method("get_z_index", &SpriteComponent::get_z_index)
            .method("set_z_index",
                    [](SpriteComponent& self, int64_t z_index) {
                        self.set_z_index(lua_narrow<int32_t>(z_index, "SpriteComponent:set_z_index"));
                    },
                    {"z_index"})
            .method("get_pixels_per_meter", &SpriteComponent::get_pixels_per_meter)
            .method("set_pixels_per_meter",
                    [](SpriteComponent& self, int64_t value) {
                        self.set_pixels_per_meter(lua_narrow<uint32_t>(value, "SpriteComponent:set_pixels_per_meter"));
                    },
                    {"value"});

        bind_component_schema<SpriteComponent>(lua,
                                               meta,
                                               schemas,
                                               "sprite",
                                               "add_sprite",
                                               "get_sprite",
                                               {
                                                   {.name = "texture",
                                                    .get_method = "get_texture",
                                                    .set_method = "set_texture",
                                                    .type = field_type::TEXTURE},
                                                   {.name = "material",
                                                    .get_method = "get_material_const",
                                                    .set_method = "set_material",
                                                    .type = field_type::MATERIAL},
                                                   {"pivot", "get_pivot", "set_pivot"},
                                                   {"scale", "get_scale", "set_scale"},
                                                   {"z_index", "get_z_index", "set_z_index"},
                                                   {.name = "pixels_per_meter",
                                                    .get_method = "get_pixels_per_meter",
                                                    .set_method = "set_pixels_per_meter",
                                                    .min = 1.0f,
                                                    .max = MAX_FLOAT},
                                               });

        // SpriteAnimatorComponent
        Renderer& renderer = m_engine.get_renderer();
        bind_usertype<AnimationClip>(lua, meta, Bases<Asset>{})
            .factory_ctor(
                [&renderer](const sol::table& animclip_t) {
                    auto clip = std::make_shared<AnimationClip>();
                    clip->set_looping(animclip_t.get_or("looping", true));

                    if (animclip_t.get<sol::optional<sol::table>>("textures")) {
                        clip->add_track(build_texture_track(renderer, animclip_t));
                    }

                    if (auto tracks = animclip_t.get<sol::optional<sol::table>>("tracks")) {
                        for (int32_t i = 1; i <= tracks->size(); ++i) {
                            if (auto track_t = tracks->get<sol::optional<sol::table>>(i)) {
                                build_track(*track_t, *clip);
                            }
                        }
                    }

                    float duration = animclip_t.get_or("duration", 0.0f);
                    for (const AnimationTrackRef& track : clip->get_tracks()) {
                        duration = std::max(duration, track->get_duration());
                    }
                    clip->set_duration(duration);

                    return clip;
                },
                {"config"})
            .method("get_duration", &AnimationClip::get_duration)
            .method("get_looping", &AnimationClip::get_looping);

        bind_asset_factory_schema<AnimationClip>(asset_factory_schemas,
                                                 "DefineAnimationClip",
                                                 def_registry::ANIMATION_CLIPS,
                                                 {"textures", "fps", "looping", "tracks", "duration"});

        bind_usertype<SpriteAnimatorComponent>(lua, meta, Bases<Component>{})
            .method("add_clip", &SpriteAnimatorComponent::add_clip, {"name", "clip"})
            .method("clear_clips", &SpriteAnimatorComponent::clear_clips)
            .method("set_default_clip", &SpriteAnimatorComponent::set_default_clip, {"name"})
            .method("get_default_clip", &SpriteAnimatorComponent::get_default_clip)
            .method("get_current_clip", &SpriteAnimatorComponent::get_current_clip)
            .method("play", &SpriteAnimatorComponent::play, {"name"})
            .method("resume", &SpriteAnimatorComponent::resume)
            .method("pause", &SpriteAnimatorComponent::pause)
            .method("stop", &SpriteAnimatorComponent::stop)
            .method("is_playing", &SpriteAnimatorComponent::is_playing)
            .method_sig(
                "get_clips",
                [](const SpriteAnimatorComponent& self, sol::this_state ts) {
                    sol::state_view lua(ts);
                    sol::table clips_t = lua.create_table();
                    for (const auto& [name, clip] : self.get_clips()) {
                        clips_t[name] = clip;
                    }
                    return clips_t;
                },
                "(): table<string, AnimationClip>")
            .method_sig(
                "set_clips",
                [](SpriteAnimatorComponent& self, const sol::table& clips_t) {
                    AnimationClips clips;
                    for (auto& kv : clips_t) {
                        if (!kv.first.is<std::string>()) {
                            continue;
                        }
                        const std::string name = kv.first.as<std::string>();
                        auto clip = kv.second.as<sol::optional<AnimationClipRef>>();
                        if (clip) {
                            clips.emplace(name, *clip);
                        }
                    }
                    self.set_clips(std::move(clips));
                },
                "(clips: table<string, AnimationClip>)");

        bind_component_schema<SpriteAnimatorComponent>(
            lua,
            meta,
            schemas,
            "sprite_animator",
            "add_sprite_animator",
            "get_sprite_animator",
            {
                {.name = "clips", .get_method = "get_clips", .set_method = "set_clips", .hide_in_inspector = true},
                {"default_clip", "get_default_clip", "set_default_clip"},
            });

        // SocketsComponent
        bind_usertype<SocketsComponent>(lua, meta, Bases<Component>{})
            .method("get_socket", &SocketsComponent::get_socket, {"name"})
            .method("has_socket", &SocketsComponent::has_socket, {"name"})
            .method_sig(
                "set_sockets",
                [](SocketsComponent& self, const sol::table& sockets_t) {
                    std::unordered_set<std::string> names;
                    for (auto& kv : sockets_t) {
                        if (!kv.first.is<std::string>()) {
                            continue;
                        }

                        const std::string name = kv.first.as<std::string>();
                        names.insert(name);
                        if (!kv.second.is<sol::table>()) {
                            log::lua.error("SocketsComponent:set_sockets expects {{position, rotation}} for '{}'",
                                           name);
                            continue;
                        }

                        const sol::table pose = kv.second.as<sol::table>();
                        const Vector2 position = pose.get<sol::optional<Vector2>>("position").value_or(Vector2());
                        const float rotation_deg = pose.get_or("rotation", 0.0f);
                        self.add_socket(name, position, rotation_deg * DEG_TO_RAD);
                    }
                    self.retain_sockets(names);
                },
                "(sockets: table<string, { position: Vector2, rotation: number }>)");

        bind_component_schema_map<SocketsComponent>(
            lua, meta, schemas, "sockets", "add_sockets", "get_sockets", "set_sockets");

        // CameraComponent
        bind_usertype<CameraComponent>(lua, meta, Bases<Component>{})
            .method("get_pixels_per_meter", &CameraComponent::get_pixels_per_meter)
            .method("set_pixels_per_meter",
                    [](CameraComponent& self, int64_t value) {
                        self.set_pixels_per_meter(lua_narrow<uint32_t>(value, "CameraComponent:set_pixels_per_meter"));
                    },
                    {"value"})
            .method("get_zoom", &CameraComponent::get_zoom)
            .method("set_zoom", &CameraComponent::set_zoom, {"multiplier"})
            .method("world_to_screen",
                    sol::resolve<Vector2(const Vector2&) const>(&CameraComponent::world_to_screen),
                    {"world_pos"})
            .method("screen_to_world",
                    sol::resolve<Vector2(const Vector2&) const>(&CameraComponent::screen_to_world),
                    {"screen_pos"});

        bind_component_schema<CameraComponent>(lua,
                                               meta,
                                               schemas,
                                               "camera",
                                               "add_camera",
                                               "get_camera",
                                               {
                                                   {.name = "pixels_per_meter",
                                                    .get_method = "get_pixels_per_meter",
                                                    .set_method = "set_pixels_per_meter",
                                                    .min = 1.0f,
                                                    .max = MAX_FLOAT},
                                               });

        // AudioComponent
        bind_usertype<AudioComponent>(lua, meta, Bases<Component>{})
            .method("play", &AudioComponent::play)
            .method("stop", &AudioComponent::stop)
            .method("is_playing", &AudioComponent::is_playing)
            .method("get_clip", &AudioComponent::get_clip)
            .method_sig(
                "set_clip",
                [](AudioComponent& self, const sol::object& value) {
                    if (!value.valid()) {
                        self.set_clip(nullptr);
                    }
                    else if (value.is<AudioClip>()) {
                        self.set_clip(value.as<AudioClipRef>());
                    }
                    else {
                        log::lua.error("AudioComponent:set_clip expects an AudioClip or nil");
                    }
                },
                "(clip: AudioClip|nil)")
            .method("get_volume", &AudioComponent::get_volume)
            .method("set_volume", &AudioComponent::set_volume, {"volume"})
            .method("is_looping", &AudioComponent::is_looping)
            .method("set_looping", &AudioComponent::set_looping, {"looping"})
            .method("is_spatial", &AudioComponent::is_spatial)
            .method("set_spatial", &AudioComponent::set_spatial, {"spatial"})
            .method("get_max_distance", &AudioComponent::get_max_distance)
            .method("set_max_distance", &AudioComponent::set_max_distance, {"max_distance"})
            .method("get_autoplay", &AudioComponent::get_autoplay)
            .method("set_autoplay", &AudioComponent::set_autoplay, {"autoplay"});

        bind_component_schema<AudioComponent>(
            lua,
            meta,
            schemas,
            "audio",
            "add_audio",
            "get_audio",
            {
                {.name = "clip", .get_method = "get_clip", .set_method = "set_clip", .type = field_type::AUDIO_CLIP},
                {.name = "volume", .get_method = "get_volume", .set_method = "set_volume", .min = 0.0f, .max = 1.0f},
                {"looping", "is_looping", "set_looping"},
                {"spatial", "is_spatial", "set_spatial"},
                {.name = "max_distance",
                 .get_method = "get_max_distance",
                 .set_method = "set_max_distance",
                 .min = 0.0f,
                 .max = MAX_FLOAT},
                {"autoplay", "get_autoplay", "set_autoplay"},
            });
    }
} // namespace hob
