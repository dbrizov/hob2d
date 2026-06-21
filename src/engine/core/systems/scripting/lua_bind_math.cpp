#include "engine/math/aabb.h"
#include "engine/math/capsule.h"
#include "engine/math/circle.h"
#include "engine/math/color.h"
#include "engine/math/constants.h"
#include "engine/math/mathf.h"
#include "engine/math/vector2.h"
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
            .constant("MIN_INTEGER", MIN_INT64)
            .constant("MAX_INTEGER", MAX_INT64)
            .constant("MIN_NUMBER", MIN_DOUBLE)
            .constant("MAX_NUMBER", MAX_DOUBLE)
            .func("normalize_angle", &math::normalize_angle, {"angle_deg"})
            .func("lerp", &math::lerp, {"a", "b", "t"})
            .func("lerp_angle", &math::lerp_angle, {"a_deg", "b_deg", "t"});

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

        bind_usertype<AABB>(m_lua, m_meta)
            .ctors<sol::types<const Vector2&, const Vector2&>>()
            .field("center", &AABB::center)
            .field("extents", &AABB::extents)
            .method("min", &AABB::min)
            .method("max", &AABB::max)
            .method("size", &AABB::size);

        bind_usertype<Capsule>(m_lua, m_meta)
            .ctors<sol::types<const Vector2&, const Vector2&, float>>()
            .field("center_a", &Capsule::center_a)
            .field("center_b", &Capsule::center_b)
            .field("radius", &Capsule::radius)
            .method("get_height", &Capsule::get_height);

        bind_usertype<Circle>(m_lua, m_meta)
            .ctors<sol::types<const Vector2&, float>>()
            .field("center", &Circle::center)
            .field("radius", &Circle::radius);

        bind_usertype<Color>(m_lua, m_meta)
            .ctors<sol::types<>, sol::types<float, float, float>, sol::types<float, float, float, float>>()
            .field("r", &Color::r)
            .field("g", &Color::g)
            .field("b", &Color::b)
            .field("a", &Color::a)
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
