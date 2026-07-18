#include "engine/components/camera_component.h"
#include "engine/components/transform_component.h"
#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/entity/entity.h"
#include "engine/entity/entity_ref.h"
#include "engine/math/vector2.h"
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
                      return cam ? cam->get_pixels_per_meter() : 0.0f;
                  })
            .func("set_pixels_per_meter",
                  [&engine](float value) {
                      CameraComponent* cam = engine.get_active_camera();
                      if (cam != nullptr) {
                          cam->set_pixels_per_meter(value);
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
    }
} // namespace hob
