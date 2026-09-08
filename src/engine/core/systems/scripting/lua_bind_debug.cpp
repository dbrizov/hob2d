#include <string_view>

#include "engine/core/debug.h"
#include "engine/math/aabb3.h"
#include "engine/math/matrix4x4.h"
#include "engine/math/vector2.h"
#include "engine/math/vector3.h"
#include "lua_bind_helpers.h"
#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    void LuaScriptSystem::bind_debug() {
        sol::state& m_lua = m_impl->lua;
        LuaMetaRegistry& m_meta = m_impl->meta;

        bind_table(m_lua, m_meta, "Debug")
            .func_sig(
                "print",
                [](sol::this_state ts,
                   std::string_view message,
                   sol::optional<Color> color,
                   sol::optional<float> duration,
                   sol::optional<bool> log) {
                    (void)ts;
                    debug::print(color.value_or(debug::DEFAULT_MESSAGE_COLOR),
                                 duration.value_or(debug::DEFAULT_MESSAGE_DURATION),
                                 log.value_or(debug::DEFAULT_MESSAGE_LOG),
                                 "{}",
                                 message);
                },
                "(message: string, color: Color?, duration: number?, log: boolean?)")
            .func_sig(
                "draw_line",
                [](const Vector2& from,
                   const Vector2& to,
                   sol::optional<Color> color,
                   sol::optional<float> duration,
                   sol::optional<float> thickness) {
                    debug::draw_line(from,
                                     to,
                                     color.value_or(debug::DEFAULT_DRAW_COLOR),
                                     duration.value_or(debug::DEFAULT_DRAW_DURATION),
                                     thickness.value_or(debug::DEFAULT_LINE_THICKNESS));
                },
                "(from: Vector2, to: Vector2, color: Color?, duration: number?, thickness: number?)")
            .func_sig(
                "draw_circle",
                [](const Vector2& center,
                   float radius,
                   sol::optional<Color> color,
                   sol::optional<float> duration,
                   sol::optional<float> thickness,
                   sol::optional<int64_t> segments) {
                    debug::draw_circle(center,
                                       radius,
                                       color.value_or(debug::DEFAULT_DRAW_COLOR),
                                       duration.value_or(debug::DEFAULT_DRAW_DURATION),
                                       thickness.value_or(debug::DEFAULT_LINE_THICKNESS),
                                       segments ? lua_narrow<int32_t>(*segments, "Debug.draw_circle segments")
                                                : debug::DEFAULT_CIRCLE_SEGMENTS);
                },
                "(center: Vector2, radius: number, color: Color?, duration: number?, thickness: number?, segments: integer?)")
            .func_sig(
                "draw_line_3d",
                [](const Vector3& from,
                   const Vector3& to,
                   sol::optional<Color> color,
                   sol::optional<float> duration,
                   sol::optional<float> thickness) {
                    debug::draw_line_3d(from,
                                        to,
                                        color.value_or(debug::DEFAULT_DRAW_COLOR),
                                        duration.value_or(debug::DEFAULT_DRAW_DURATION),
                                        thickness.value_or(debug::DEFAULT_LINE_THICKNESS));
                },
                "(from: Vector3, to: Vector3, color: Color?, duration: number?, thickness: number?)")
            .func_sig(
                "draw_aabb3",
                [](const AABB3& box,
                   sol::optional<Color> color,
                   sol::optional<float> duration,
                   sol::optional<float> thickness) {
                    debug::draw_aabb3(box,
                                      Matrix4x4::identity(),
                                      color.value_or(debug::DEFAULT_DRAW_COLOR),
                                      duration.value_or(debug::DEFAULT_DRAW_DURATION),
                                      thickness.value_or(debug::DEFAULT_LINE_THICKNESS));
                },
                "(box: AABB3, color: Color?, duration: number?, thickness: number?)")
            .func_sig(
                "draw_sphere_3d",
                [](const Vector3& center,
                   float radius,
                   sol::optional<Color> color,
                   sol::optional<float> duration,
                   sol::optional<float> thickness,
                   sol::optional<int64_t> segments) {
                    debug::draw_sphere_3d(center,
                                          radius,
                                          color.value_or(debug::DEFAULT_DRAW_COLOR),
                                          duration.value_or(debug::DEFAULT_DRAW_DURATION),
                                          thickness.value_or(debug::DEFAULT_LINE_THICKNESS),
                                          segments ? lua_narrow<int32_t>(*segments, "Debug.draw_sphere_3d segments")
                                                   : debug::DEFAULT_CIRCLE_SEGMENTS);
                },
                "(center: Vector3, radius: number, color: Color?, duration: number?, thickness: number?, segments: integer?)");
    }
} // namespace hob
