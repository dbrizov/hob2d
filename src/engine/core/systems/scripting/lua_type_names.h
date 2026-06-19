#pragma once

// HOB_LUA_TYPE specializations for every C++ type exposed to Lua.
// Included by every lua_bind_*.cpp so meta-name lookup resolves uniformly,
// regardless of which TU is the first to bind a given type.

#include "lua_meta.h"

namespace hob {
    struct Vector2;
    struct AABB;
    struct Capsule;
    struct Circle;
    struct Color;
    class Texture;
    struct Material;
    struct AnimationClip;
    struct RaycastHit;
    class EntityRef;
    class Component;
    class TransformComponent;
    class SpriteComponent;
    class SpriteAnimatorComponent;
    class CameraComponent;
    class RigidbodyComponent;
    class ColliderComponent;
    class BoxColliderComponent;
    class CapsuleColliderComponent;
    class CircleColliderComponent;
    class CharacterBodyComponent;
    class InputComponent;
    enum class BodyType;
    enum class InputEventType;

    // clang-format off
    HOB_LUA_TYPE(Vector2, "Vector2")
    HOB_LUA_TYPE(AABB, "AABB")
    HOB_LUA_TYPE(Capsule, "Capsule")
    HOB_LUA_TYPE(Circle, "Circle")
    HOB_LUA_TYPE(Color, "Color")
    HOB_LUA_TYPE(Texture, "Texture")
    HOB_LUA_TYPE(Material, "Material")
    HOB_LUA_TYPE(AnimationClip, "AnimationClip")
    HOB_LUA_TYPE(RaycastHit, "RaycastHit")
    HOB_LUA_TYPE(EntityRef, "Entity")
    HOB_LUA_TYPE(Component, "Component")
    HOB_LUA_TYPE(TransformComponent, "TransformComponent")
    HOB_LUA_TYPE(SpriteComponent, "SpriteComponent")
    HOB_LUA_TYPE(SpriteAnimatorComponent, "SpriteAnimatorComponent")
    HOB_LUA_TYPE(CameraComponent, "CameraComponent")
    HOB_LUA_TYPE(RigidbodyComponent, "RigidbodyComponent")
    HOB_LUA_TYPE(ColliderComponent, "ColliderComponent")
    HOB_LUA_TYPE(BoxColliderComponent, "BoxColliderComponent")
    HOB_LUA_TYPE(CapsuleColliderComponent, "CapsuleColliderComponent")
    HOB_LUA_TYPE(CircleColliderComponent, "CircleColliderComponent")
    HOB_LUA_TYPE(CharacterBodyComponent, "CharacterBodyComponent")
    HOB_LUA_TYPE(InputComponent, "InputComponent")
    HOB_LUA_TYPE(BodyType, "BodyType")
    HOB_LUA_TYPE(InputEventType, "InputEventType")
    // clang-format on
} // namespace hob
