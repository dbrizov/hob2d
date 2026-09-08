#include "engine/math/aabb.h"
#include "engine/math/aabb3.h"
#include "engine/math/capsule.h"
#include "engine/math/circle.h"
#include "engine/math/color.h"
#include "engine/math/constants.h"
#include "engine/math/mathf.h"
#include "engine/math/quaternion.h"
#include "engine/math/ray.h"
#include "engine/math/vector2.h"
#include "engine/math/vector3.h"
#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    void LuaScriptSystem::bind_math() {
        sol::state& m_lua = m_impl->lua;
        LuaMetaRegistry& m_meta = m_impl->meta;

        bind_table(m_lua, m_meta, "Math")
            .constant("PI", PI)
            .constant("EPSILON", EPSILON)
            .constant("DEG_TO_RAD", DEG_TO_RAD)
            .constant("RAD_TO_DEG", RAD_TO_DEG)
            .constant("MIN_INT64", MIN_INT64)
            .constant("MAX_INT64", MAX_INT64)
            .constant("MIN_FLOAT", MIN_FLOAT)
            .constant("MAX_FLOAT", MAX_FLOAT)
            .constant("MIN_DOUBLE", MIN_DOUBLE)
            .constant("MAX_DOUBLE", MAX_DOUBLE)
            .func("normalize_angle_deg", &math::normalize_angle_deg, {"angle_deg"})
            .func("wrap_angle_rad", &math::wrap_angle_rad, {"angle_rad"})
            .func("lerp", &math::lerp, {"a", "b", "t"})
            .func("lerp_angle", &math::lerp_angle, {"a_deg", "b_deg", "t"})
            .func_sig(
                "approx_equal",
                [](float a, float b, sol::optional<float> epsilon) {
                    return math::approx_equal(a, b, epsilon.value_or(EPSILON));
                },
                "(a: number, b: number, epsilon: number?): boolean");

        bind_usertype<Vector2>(m_lua, m_meta)
            .ctors<sol::types<>, sol::types<float, float>>()
            .field("x", &Vector2::x)
            .field("y", &Vector2::y)
            .method("length", &Vector2::length)
            .method("length_sqr", &Vector2::length_sqr)
            .method("normalized", &Vector2::normalized)
            .op_add(&Vector2::operator+)
            .op_sub(sol::resolve<Vector2(const Vector2&) const>(&Vector2::operator-))
            .op_unm(sol::resolve<Vector2() const>(&Vector2::operator-))
            .op_mul(&Vector2::operator*)
            .op_div(&Vector2::operator/)
            .op_eq(&Vector2::operator==)
            .op_tostring(&Vector2::to_string)
            .method("zero", &Vector2::zero)
            .method("one", &Vector2::one)
            .method("left", &Vector2::left)
            .method("right", &Vector2::right)
            .method("up", &Vector2::up)
            .method("down", &Vector2::down)
            .method("dot", &Vector2::dot, {"a", "b"})
            .method("distance", &Vector2::distance, {"a", "b"})
            .method("lerp", &Vector2::lerp, {"a", "b", "t"})
            .method("rotate_around", &Vector2::rotate_around, {"point", "pivot", "radians"});

        bind_usertype<Vector3>(m_lua, m_meta)
            .ctors<sol::types<>, sol::types<float, float, float>>()
            .field("x", &Vector3::x)
            .field("y", &Vector3::y)
            .field("z", &Vector3::z)
            .method("length", &Vector3::length)
            .method("length_sqr", &Vector3::length_sqr)
            .method("normalized", &Vector3::normalized)
            .op_add(&Vector3::operator+)
            .op_sub(sol::resolve<Vector3(const Vector3&) const>(&Vector3::operator-))
            .op_unm(sol::resolve<Vector3() const>(&Vector3::operator-))
            .op_mul(&Vector3::operator*)
            .op_div(&Vector3::operator/)
            .op_eq(&Vector3::operator==)
            .op_tostring(&Vector3::to_string)
            .method("zero", &Vector3::zero)
            .method("one", &Vector3::one)
            .method("left", &Vector3::left)
            .method("right", &Vector3::right)
            .method("up", &Vector3::up)
            .method("down", &Vector3::down)
            .method("forward", &Vector3::forward)
            .method("back", &Vector3::back)
            .method("dot", &Vector3::dot, {"a", "b"})
            .method("cross", &Vector3::cross, {"a", "b"})
            .method("scale", &Vector3::scale, {"a", "b"})
            .method("distance", &Vector3::distance, {"a", "b"})
            .method("lerp", &Vector3::lerp, {"a", "b", "t"})
            .method("min", &Vector3::min, {"a", "b"})
            .method("max", &Vector3::max, {"a", "b"})
            .method("abs", &Vector3::abs, {"a"})
            .method("project_on_plane", &Vector3::project_on_plane, {"vector", "plane_normal"});

        bind_usertype<Quaternion>(m_lua, m_meta)
            .ctors<sol::types<>, sol::types<float, float, float, float>>()
            .field("x", &Quaternion::x)
            .field("y", &Quaternion::y)
            .field("z", &Quaternion::z)
            .field("w", &Quaternion::w)
            .method("normalized", &Quaternion::normalized)
            .method("conjugate", &Quaternion::conjugate)
            .method("inverse", &Quaternion::inverse)
            .method("to_euler_deg", &Quaternion::to_euler_deg)
            .method("rotate", &Quaternion::rotate, {"vector"})
            .method("get_forward", &Quaternion::get_forward)
            .method("get_right", &Quaternion::get_right)
            .method("get_up", &Quaternion::get_up)
            .op_mul(&Quaternion::operator*)
            .op_eq(&Quaternion::operator==)
            .op_tostring(&Quaternion::to_string)
            .method("identity", &Quaternion::identity)
            .method("from_axis_angle", &Quaternion::from_axis_angle, {"axis", "radians"})
            .method("from_euler_deg", &Quaternion::from_euler_deg, {"euler_deg"})
            .method_sig(
                "look_rotation",
                [](const Vector3& forward, sol::optional<Vector3> up) {
                    return Quaternion::look_rotation(forward, up.value_or(Vector3::up()));
                },
                "(forward: Vector3, up: Vector3?): Quaternion",
                true)
            .method("from_to_rotation", &Quaternion::from_to_rotation, {"from", "to"})
            .method("dot", &Quaternion::dot, {"a", "b"})
            .method("slerp", &Quaternion::slerp, {"a", "b", "t"})
            .method("angle_deg", &Quaternion::angle_deg, {"a", "b"});

        bind_usertype<AABB3>(m_lua, m_meta)
            .ctors<sol::types<>, sol::types<const Vector3&, const Vector3&>>()
            .field("center", &AABB3::center)
            .field("extents", &AABB3::extents)
            .op_eq(&AABB3::operator==)
            .op_tostring(&AABB3::to_string)
            .method("min", &AABB3::min)
            .method("max", &AABB3::max)
            .method("size", &AABB3::size)
            .method("contains", &AABB3::contains, {"point"})
            .method_sig(
                "raycast",
                [](const AABB3& self, const Ray& ray) -> sol::optional<float> {
                    float distance = 0.0f;
                    if (!self.raycast(ray, distance)) {
                        return sol::nullopt;
                    }
                    return distance;
                },
                "(ray: Ray): number?")
            .method("from_min_max", &AABB3::from_min_max, {"min", "max"})
            .method("combine", &AABB3::combine, {"a", "b"});

        bind_usertype<Ray>(m_lua, m_meta)
            .ctors<sol::types<>, sol::types<const Vector3&, const Vector3&>>()
            .field("origin", &Ray::origin)
            .field("direction", &Ray::direction)
            .op_tostring(&Ray::to_string)
            .method("point_at", &Ray::point_at, {"distance"});

        bind_usertype<AABB>(m_lua, m_meta)
            .ctors<sol::types<>, sol::types<const Vector2&, const Vector2&>>()
            .field("center", &AABB::center)
            .field("extents", &AABB::extents)
            .op_eq(&AABB::operator==)
            .op_tostring(&AABB::to_string)
            .method("min", &AABB::min)
            .method("max", &AABB::max)
            .method("size", &AABB::size);

        bind_usertype<Capsule>(m_lua, m_meta)
            .ctors<sol::types<>, sol::types<const Vector2&, const Vector2&, float>>()
            .field("center_a", &Capsule::center_a)
            .field("center_b", &Capsule::center_b)
            .field("radius", &Capsule::radius)
            .op_eq(&Capsule::operator==)
            .op_tostring(&Capsule::to_string)
            .method("get_height", &Capsule::get_height);

        bind_usertype<Circle>(m_lua, m_meta)
            .ctors<sol::types<>, sol::types<const Vector2&, float>>()
            .field("center", &Circle::center)
            .field("radius", &Circle::radius)
            .op_eq(&Circle::operator==)
            .op_tostring(&Circle::to_string);

        bind_usertype<Color>(m_lua, m_meta)
            .ctors<sol::types<>, sol::types<float, float, float>, sol::types<float, float, float, float>>()
            .field("r", &Color::r)
            .field("g", &Color::g)
            .field("b", &Color::b)
            .field("a", &Color::a)
            .op_eq(&Color::operator==)
            .op_tostring(&Color::to_string)
            .method("black", &Color::black)
            .method("white", &Color::white)
            .method("gray", &Color::gray)
            .method("red", &Color::red)
            .method("green", &Color::green)
            .method("blue", &Color::blue)
            .method("yellow", &Color::yellow)
            .method("magenta", &Color::magenta)
            .method("cyan", &Color::cyan)
            .method("orange", &Color::orange);
    }
} // namespace hob
