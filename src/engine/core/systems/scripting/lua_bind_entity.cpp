#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "engine/components/audio_component.h"
#include "engine/components/camera_component.h"
#include "engine/components/input_component.h"
#include "engine/components/lua_script_component.h"
#include "engine/components/lua_script_component_impl.h"
#include "engine/components/physics/box_collider_component.h"
#include "engine/components/physics/capsule_collider_component.h"
#include "engine/components/physics/character_body_component.h"
#include "engine/components/physics/circle_collider_component.h"
#include "engine/components/physics/rigidbody_component.h"
#include "engine/components/sprite_animator_component.h"
#include "engine/components/sprite_component.h"
#include "engine/components/transform_component.h"
#include "engine/core/logging.h"
#include "engine/entity/entity.h"
#include "engine/entity/entity_ref.h"
#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    void LuaScriptSystem::bind_entity() {
        sol::state& m_lua = m_impl->lua;
        LuaMetaRegistry& m_meta = m_impl->meta;

        bind_usertype<EntityRef>(m_lua, m_meta)
            .method("get_id", &EntityRef::get_id)
            .method("get_name",
                    [](const EntityRef& r) -> std::string_view {
                        Entity* e = r.resolve();
                        return e ? std::string_view(e->get_name()) : std::string_view();
                    })
            .method("set_name",
                    [](const EntityRef& r, std::string name) {
                        if (Entity* e = r.resolve()) {
                            e->set_name(std::move(name));
                        }
                    },
                    {"name"})
            .method("get_prefab_name",
                    [](const EntityRef& r) -> std::string_view {
                        Entity* e = r.resolve();
                        return e ? std::string_view(e->get_prefab_name()) : std::string_view();
                    })
            .method("set_prefab_name",
                    [](const EntityRef& r, std::string name) {
                        if (Entity* e = r.resolve()) {
                            e->set_prefab_name(std::move(name));
                        }
                    },
                    {"name"})
            .method("is_valid", &EntityRef::is_valid)
            .method("is_in_play",
                    [](const EntityRef& r) {
                        Entity* e = r.resolve();
                        return e != nullptr && e->is_in_play();
                    })
            .method("is_ticking",
                    [](const EntityRef& r) {
                        Entity* e = r.resolve();
                        return e != nullptr && e->is_ticking();
                    })
            .method("set_ticking",
                    [](const EntityRef& r, bool v) {
                        if (Entity* e = r.resolve()) {
                            e->set_ticking(v);
                        }
                    },
                    {"ticking"})
            .method_sig(
                "add_lua_component",
                [](const EntityRef& r, std::string class_name) -> sol::object {
                    Entity* e = r.resolve();
                    if (e == nullptr) {
                        return sol::lua_nil;
                    }

                    LuaScriptComponent* lua_comp = e->add_lua_component(std::move(class_name));
                    if (lua_comp == nullptr) {
                        return sol::lua_nil;
                    }

                    return lua_comp->impl().lua_instance;
                },
                "(class_name: string): LuaComponent?")
            .method("get_transform",
                    [](const EntityRef& r) -> TransformComponent* {
                        Entity* e = r.resolve();
                        return e ? e->get_transform() : nullptr;
                    })
            .method("get_rigidbody",
                    [](const EntityRef& r) -> RigidbodyComponent* {
                        Entity* e = r.resolve();
                        return e ? e->get_rigidbody() : nullptr;
                    })
            .method("get_character_body",
                    [](const EntityRef& r) -> CharacterBodyComponent* {
                        Entity* e = r.resolve();
                        return e ? e->get_component<CharacterBodyComponent>() : nullptr;
                    })
            .method("get_box_collider",
                    [](const EntityRef& r) -> BoxColliderComponent* {
                        Entity* e = r.resolve();
                        return e ? e->get_component<BoxColliderComponent>() : nullptr;
                    })
            .method("get_capsule_collider",
                    [](const EntityRef& r) -> CapsuleColliderComponent* {
                        Entity* e = r.resolve();
                        return e ? e->get_component<CapsuleColliderComponent>() : nullptr;
                    })
            .method("get_circle_collider",
                    [](const EntityRef& r) -> CircleColliderComponent* {
                        Entity* e = r.resolve();
                        return e ? e->get_component<CircleColliderComponent>() : nullptr;
                    })
            .method("get_input",
                    [](const EntityRef& r) -> InputComponent* {
                        Entity* e = r.resolve();
                        return e ? e->get_component<InputComponent>() : nullptr;
                    })
            .method("get_sprite",
                    [](const EntityRef& r) -> SpriteComponent* {
                        Entity* e = r.resolve();
                        return e ? e->get_component<SpriteComponent>() : nullptr;
                    })
            .method("get_sprite_animator",
                    [](const EntityRef& r) -> SpriteAnimatorComponent* {
                        Entity* e = r.resolve();
                        return e ? e->get_component<SpriteAnimatorComponent>() : nullptr;
                    })
            .method("get_camera",
                    [](const EntityRef& r) -> CameraComponent* {
                        Entity* e = r.resolve();
                        return e ? e->get_component<CameraComponent>() : nullptr;
                    })
            .method("get_audio",
                    [](const EntityRef& r) -> AudioComponent* {
                        Entity* e = r.resolve();
                        return e ? e->get_component<AudioComponent>() : nullptr;
                    })
            .method_sig(
                "get_lua_component",
                [](const EntityRef& r, std::string_view class_name) -> sol::object {
                    Entity* e = r.resolve();
                    if (e == nullptr) {
                        return sol::lua_nil;
                    }

                    LuaScriptComponent* found = e->get_lua_component(class_name);
                    if (found != nullptr) {
                        return found->impl().lua_instance;
                    }

                    return sol::lua_nil;
                },
                "(class_name: string): LuaComponent?")
            .method_sig(
                "get_lua_components",
                [this](const EntityRef& r) {
                    sol::table out = m_impl->lua.create_table();
                    Entity* e = r.resolve();
                    if (e == nullptr) {
                        return out;
                    }

                    e->for_each_component<LuaScriptComponent>([&out](LuaScriptComponent* lua_comp) {
                        out.add(lua_comp->impl().lua_instance);
                    });

                    return out;
                },
                "(): LuaComponent[]")
            .method_sig(
                "get_components",
                [](const EntityRef& r) -> std::vector<Component*> {
                    Entity* e = r.resolve();
                    if (e == nullptr) {
                        return {};
                    }

                    return e->get_components<Component>();
                },
                "(): Component[]")
            .method_sig(
                "for_each_component",
                [](const EntityRef& r,
                   const sol::protected_function& func,
                   const sol::optional<sol::protected_function>& until_predicate) {
                    Entity* e = r.resolve();
                    if (e == nullptr) {
                        return;
                    }

                    const auto visit = [&func](Component* component) {
                        const sol::protected_function_result result = func(component);
                        if (!result.valid()) {
                            const sol::error err = result;
                            log::sol2.error("Lua error in Entity.for_each_component: {}", err.what());
                        }
                    };

                    if (!until_predicate) {
                        e->for_each_component<Component>(visit);
                        return;
                    }

                    e->for_each_component<Component>(visit, [&until_predicate](Component* component) {
                        const sol::protected_function_result result = (*until_predicate)(component);
                        if (!result.valid()) {
                            const sol::error err = result;
                            log::sol2.error("Lua error in Entity.for_each_component until_predicate: {}", err.what());
                            return true;
                        }

                        const sol::optional<bool> stop = result;
                        return stop.value_or(false);
                    });
                },
                "(func: fun(component: Component), until_predicate: (fun(component: Component): boolean)?)")
            .op_tostring([](const EntityRef& r) {
                Entity* e = r.resolve();
                return e ? e->to_string() : std::format("Entity(invalid, id = {})", r.get_id());
            })
            .op_concat([](const EntityRef& r) {
                Entity* e = r.resolve();
                return e ? e->to_string() : std::format("Entity(invalid, id = {})", r.get_id());
            });
    }
} // namespace hob
