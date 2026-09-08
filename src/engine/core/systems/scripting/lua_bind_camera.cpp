#include "engine/components/camera_component.h"
#include "engine/components/camera_component_3d.h"
#include "engine/components/transform_component.h"
#include "engine/components/transform_component_3d.h"
#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/entity/entity.h"
#include "engine/entity/entity_ref.h"
#include "engine/math/ray.h"
#include "engine/math/vector2.h"
#include "engine/math/vector3.h"
#include "lua_bind_helpers.h"
#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    void LuaScriptSystem::bind_camera() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;
        Engine& engine = m_engine;

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
            .func("get_pixels_per_meter",
                  [&engine]() {
                      CameraComponent* cam = engine.get_active_camera();
                      return cam ? cam->get_pixels_per_meter() : 0u;
                  })
            .func("set_pixels_per_meter",
                  [&engine](int64_t value) {
                      CameraComponent* cam = engine.get_active_camera();
                      if (cam != nullptr) {
                          cam->set_pixels_per_meter(lua_narrow<uint32_t>(value, "Camera.set_pixels_per_meter"));
                      }
                  },
                  {"value"})
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
                  {"multiplier"})
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
                  {"position"});

        bind_table(lua, meta, "Camera3D")
            .func_sig(
                "get_active",
                [&engine](sol::this_state ts) -> sol::object {
                    CameraComponent3D* cam = engine.get_active_camera_3d();
                    if (cam == nullptr) {
                        return sol::lua_nil;
                    }
                    return sol::make_object(sol::state_view(ts),
                                            EntityRef(cam->get_entity().get_id(), engine.get_entity_spawner()));
                },
                "(): Entity?")
            .func("get_fov_deg",
                  [&engine]() {
                      CameraComponent3D* cam = engine.get_active_camera_3d();
                      return cam ? cam->get_fov_deg() : 0.0f;
                  })
            .func("set_fov_deg",
                  [&engine](float value) {
                      CameraComponent3D* cam = engine.get_active_camera_3d();
                      if (cam != nullptr) {
                          cam->set_fov_deg(value);
                      }
                  },
                  {"value"})
            .func_sig(
                "world_to_screen",
                [&engine](const Vector3& world_pos) -> sol::optional<Vector2> {
                    CameraComponent3D* cam = engine.get_active_camera_3d();
                    Vector2 screen_pos;
                    if (cam == nullptr || !cam->world_to_screen(world_pos, screen_pos)) {
                        return sol::nullopt;
                    }
                    return screen_pos;
                },
                "(world_pos: Vector3): Vector2?")
            .func("screen_to_ray",
                  [&engine](const Vector2& screen_pos) {
                      CameraComponent3D* cam = engine.get_active_camera_3d();
                      return cam ? cam->screen_to_ray(screen_pos) : Ray();
                  },
                  {"screen_pos"})
            .func("get_position",
                  [&engine]() {
                      CameraComponent3D* cam = engine.get_active_camera_3d();
                      return cam ? cam->get_position() : Vector3();
                  })
            .func("set_position",
                  [&engine](const Vector3& p) {
                      CameraComponent3D* cam = engine.get_active_camera_3d();
                      if (cam != nullptr) {
                          cam->get_entity().get_transform_3d()->set_position(p);
                      }
                  },
                  {"position"});
    }
} // namespace hob
