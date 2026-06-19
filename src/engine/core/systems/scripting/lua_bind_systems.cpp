#include <functional>
#include <string>
#include <vector>

#include "engine/components/camera_component.h"
#include "engine/components/physics/collider_component.h"
#include "engine/components/transform_component.h"
#include "engine/core/debug.h"
#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/input.h"
#include "engine/core/systems/physics.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/core/systems/timer.h"
#include "engine/entity/entity.h"
#include "engine/entity/entity_ref.h"
#include "lua_meta.h"
#include "lua_schema_factory.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    namespace {
        void bind_renderer(sol::state& lua,
                           LuaMetaRegistry& meta,
                           LuaFactorySchemaRegistry& factory_schemas,
                           Renderer& renderer) {
            bind_usertype<Material>(lua, meta)
                .factory_ctor(
                    [&renderer](const sol::table& mat_t) {
                        Material mat;
                        if (auto path = mat_t.get<sol::optional<std::string>>("shader")) {
                            mat.shader_id = renderer.get_or_build_sprite_shader(*path);
                        }
                        if (auto tint = mat_t.get<sol::optional<Color>>("tint")) {
                            mat.tint = *tint;
                        }
                        if (auto oc = mat_t.get<sol::optional<Color>>("outline_color")) {
                            mat.outline_color = *oc;
                        }
                        if (auto ow = mat_t.get<sol::optional<float>>("outline_width")) {
                            mat.outline_width = *ow;
                        }
                        if (auto at = mat_t.get<sol::optional<float>>("alpha_threshold")) {
                            mat.alpha_threshold = *at;
                        }
                        return mat;
                    },
                    {"config"})
                .method("get_tint",
                        [](const Material& self) {
                            return self.tint;
                        })
                .method("set_tint",
                        [](Material& self, const Color& tint) {
                            self.tint = tint;
                        },
                        {"tint"})
                .method("get_outline_color",
                        [](const Material& self) {
                            return self.outline_color;
                        })
                .method("set_outline_color",
                        [](Material& self, const Color& color) {
                            self.outline_color = color;
                        },
                        {"color"})
                .method("get_outline_width",
                        [](const Material& self) {
                            return self.outline_width;
                        })
                .method("set_outline_width",
                        [](Material& self, float width) {
                            self.outline_width = width;
                        },
                        {"width"})
                .method("get_alpha_threshold",
                        [](const Material& self) {
                            return self.alpha_threshold;
                        })
                .method("set_alpha_threshold",
                        [](Material& self, float threshold) {
                            self.alpha_threshold = threshold;
                        },
                        {"threshold"})
                .method("set_shader",
                        [&renderer](Material& self, const std::string& path) {
                            self.shader_id = renderer.get_or_build_sprite_shader(path);
                        },
                        {"path"});

            bind_factory_schema<Material>(factory_schemas,
                                          "DefineMaterial",
                                          "Materials",
                                          {"shader", "tint", "outline_color", "outline_width", "alpha_threshold"});
        }

        void bind_timer(sol::state& lua, LuaMetaRegistry& meta, Timer& timer) {
            bind_table(lua, meta, "Timer")
                .func("get_fps",
                      [&timer]() {
                          return timer.get_fps();
                      })
                .func("set_fps",
                      [&timer](uint32_t v) {
                          timer.set_fps(v);
                      },
                      {"fps"})
                .func("get_time_scale",
                      [&timer]() {
                          return timer.get_time_scale();
                      })
                .func("set_time_scale",
                      [&timer](float v) {
                          timer.set_time_scale(v);
                      },
                      {"scale"})
                .func("get_play_time",
                      [&timer]() {
                          return timer.get_play_time();
                      })
                .func("get_delta_time", [&timer]() {
                    return timer.get_delta_time();
                });
        }

        void bind_input(sol::state& lua, LuaMetaRegistry& meta, Input& input) {
            bind_table(lua, meta, "Input")
                .func("get_mouse_screen_position",
                      [&input]() {
                          return input.get_mouse_screen_position();
                      })
                .func("is_gamepad_connected", [&input]() {
                    return input.is_gamepad_connected();
                });
        }

        void bind_physics(sol::state& lua, LuaMetaRegistry& meta, Physics& physics) {
            bind_usertype<RaycastHit>(lua, meta)
                .field("collider", &RaycastHit::collider)
                .field("point", &RaycastHit::point)
                .field("normal", &RaycastHit::normal)
                .field("distance", &RaycastHit::distance)
                .field("hit", &RaycastHit::hit)
                .property_sig(
                    "entity",
                    [](const RaycastHit& h, const sol::this_state ts) -> sol::object {
                        const sol::state_view sv(ts);
                        if (h.collider == nullptr) {
                            return sol::lua_nil;
                        }

                        return sol::make_object(sv,
                                                EntityRef(h.collider->get_entity().get_id(),
                                                          h.collider->get_engine().get_entity_spawner()));
                    },
                    "Entity?");

            // Masks/layers are uint64_t (matching Box2D), but sol2 can't push values above
            // MAX_INT64, so reinterpret to/from int64_t at the Lua boundary (bit pattern is preserved).
            bind_table(lua, meta, "Physics")
                .func("raycast",
                      [&physics](const Vector2& origin,
                                 const Vector2& direction,
                                 float distance,
                                 sol::optional<int64_t> layer_mask) {
                          const uint64_t mask = layer_mask ? static_cast<uint64_t>(*layer_mask) : ~0ull;
                          return physics.raycast(origin, direction, distance, mask);
                      },
                      {"origin", "direction", "distance", "layer_mask"})
                .func("raycast_all",
                      [&physics](const Vector2& origin,
                                 const Vector2& direction,
                                 float distance,
                                 sol::optional<int64_t> layer_mask) {
                          const uint64_t mask = layer_mask ? static_cast<uint64_t>(*layer_mask) : ~0ull;
                          return physics.raycast_all(origin, direction, distance, mask);
                      },
                      {"origin", "direction", "distance", "layer_mask"});
        }

        void bind_entity_spawner(sol::state& lua, LuaMetaRegistry& meta, EntitySpawner& spawner) {
            bind_table(lua, meta, "EntitySpawner")
                .func("spawn_entity_c",
                      [&spawner]() {
                          return EntityRef(spawner.spawn_entity().get_id(), spawner);
                      })
                .func("destroy_entity_c",
                      [&spawner](const EntityRef& r) {
                          spawner.destroy_entity(r.get_id());
                      },
                      {"entity"})
                .func("get_entity",
                      [&spawner](EntityId id) {
                          return EntityRef(id, spawner);
                      },
                      {"id"})
                .func_sig(
                    "for_each",
                    [&spawner](const sol::function& fn) {
                        std::vector<Entity*> entities;
                        spawner.get_entities(entities);
                        for (Entity* e : entities) {
                            fn(EntityRef(e->get_id(), spawner));
                        }
                    },
                    "(fn: fun(entity: Entity))");
        }

        void bind_scripts(sol::state& lua,
                          LuaMetaRegistry& meta,
                          std::function<bool(const std::string&)> run_file,
                          std::function<bool(const std::string&, const std::vector<std::string>&)> run_folder) {
            bind_table(lua, meta, "Scripts")
                .func("run_file",
                      [run_file = std::move(run_file)](const std::string& relative_path) {
                          return run_file(relative_path);
                      },
                      {"relative_path"})
                .func_sig(
                    "run_folder",
                    [run_folder = std::move(run_folder)](const std::string& relative_path,
                                                         const sol::optional<sol::table>& excludes) {
                        std::vector<std::string> exclude_list;
                        if (excludes) {
                            for (const auto& kv : *excludes) {
                                exclude_list.push_back(kv.second.as<std::string>());
                            }
                        }
                        return run_folder(relative_path, exclude_list);
                    },
                    "(relative_path: string, excludes: string[]?): boolean");
        }

        void bind_camera(sol::state& lua, LuaMetaRegistry& meta, Engine& engine) {
            bind_table(lua, meta, "Camera")
                .func_sig(
                    "get_active",
                    [&engine](sol::this_state ts) -> sol::object {
                        CameraComponent* cam = engine.get_active_camera();
                        if (cam == nullptr) {
                            return sol::lua_nil;
                        }
                        return sol::make_object(sol::state_view(ts),
                                                EntityRef(cam->get_entity().get_id(), engine.get_entity_spawner()));
                    },
                    "(): Entity?")
                .func("world_to_screen",
                      [&engine](const Vector2& world_pos) {
                          CameraComponent* cam = engine.get_active_camera();
                          return cam ? cam->world_to_screen(world_pos) : Vector2();
                      },
                      {"world_pos"})
                .func("screen_to_world",
                      [&engine](const Vector2& screen_pos) {
                          CameraComponent* cam = engine.get_active_camera();
                          return cam ? cam->screen_to_world(screen_pos) : Vector2();
                      },
                      {"screen_pos"})
                .func("get_position",
                      [&engine]() {
                          CameraComponent* cam = engine.get_active_camera();
                          return cam ? cam->get_entity().get_transform()->get_position() : Vector2();
                      })
                .func("set_position",
                      [&engine](const Vector2& p) {
                          CameraComponent* cam = engine.get_active_camera();
                          if (cam != nullptr) {
                              cam->get_entity().get_transform()->set_position(p);
                          }
                      },
                      {"position"})
                .func("get_zoom",
                      [&engine]() {
                          CameraComponent* cam = engine.get_active_camera();
                          return cam ? cam->get_zoom() : 1.0f;
                      })
                .func("set_zoom",
                      [&engine](float multiplier) {
                          CameraComponent* cam = engine.get_active_camera();
                          if (cam != nullptr) {
                              cam->set_zoom(multiplier);
                          }
                      },
                      {"multiplier"});
        }
    } // namespace

    void LuaScriptSystem::bind_systems() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;
        LuaFactorySchemaRegistry& factory_schemas = m_impl->factory_schemas;

        bind_renderer(lua, meta, factory_schemas, m_engine.get_renderer());
        bind_timer(lua, meta, m_engine.get_timer());
        bind_input(lua, meta, m_engine.get_input());
        bind_physics(lua, meta, m_engine.get_physics());
        bind_entity_spawner(lua, meta, m_engine.get_entity_spawner());
        bind_scripts(
            lua,
            meta,
            [this](const std::string& p) {
                return run_file(p);
            },
            [this](const std::string& p, const std::vector<std::string>& ex) {
                return run_folder(p, ex);
            });
        bind_camera(lua, meta, m_engine);
    }
} // namespace hob
