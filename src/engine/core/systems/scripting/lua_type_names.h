#pragma once

// HOB_LUA_TYPE specializations for every C++ type exposed to Lua.
// Included by every lua_bind_*.cpp so meta-name lookup resolves uniformly,
// regardless of which TU is the first to bind a given type.

#include "lua_meta.h"

namespace hob {
    struct Vector2;
    struct Vector3;
    struct Quaternion;
    struct AABB3;
    struct Ray;
    struct AABB;
    struct Capsule;
    struct Circle;
    struct Color;
    class Asset;
    class Texture;
    class Shader;
    class Material;
    class Mesh;
    class AnimationClip;
    class AudioClip;
    struct RaycastHit;
    struct RaycastHit3D;
    class EntityRef;
    class Component;
    class TransformComponent;
    class TransformComponent3D;
    enum class CollisionLayer : uint64_t;
    enum class BodyType;
    enum class MotionLock : uint64_t;
    class RigidbodyComponent;
    class CharacterBodyComponent;
    class ColliderComponent;
    class BoxColliderComponent;
    class CapsuleColliderComponent;
    class CircleColliderComponent;
    class RigidbodyComponent3D;
    class ColliderComponent3D;
    class BoxColliderComponent3D;
    class SphereColliderComponent3D;
    class CapsuleColliderComponent3D;
    class CharacterBodyComponent3D;
    enum class InputEventType;
    class InputComponent;
    class SpriteComponent;
    class SpriteAnimatorComponent;
    class SocketsComponent;
    class CameraComponent;
    class CameraComponent3D;
    class DirectionalLightComponent;
    class MeshRendererComponent;
    class AudioComponent;

    // clang-format off
    HOB_LUA_TYPE(Vector2, "Vector2")
    HOB_LUA_TYPE(Vector3, "Vector3")
    HOB_LUA_TYPE(Quaternion, "Quaternion")
    HOB_LUA_TYPE(AABB3, "AABB3")
    HOB_LUA_TYPE(Ray, "Ray")
    HOB_LUA_TYPE(AABB, "AABB")
    HOB_LUA_TYPE(Capsule, "Capsule")
    HOB_LUA_TYPE(Circle, "Circle")
    HOB_LUA_TYPE(Color, "Color")
    HOB_LUA_TYPE(Asset, "Asset")
    HOB_LUA_TYPE(Texture, "Texture")
    HOB_LUA_TYPE(Shader, "Shader")
    HOB_LUA_TYPE(Material, "Material")
    HOB_LUA_TYPE(Mesh, "Mesh")
    HOB_LUA_TYPE(AnimationClip, "AnimationClip")
    HOB_LUA_TYPE(AudioClip, "AudioClip")
    HOB_LUA_TYPE(RaycastHit, "RaycastHit")
    HOB_LUA_TYPE(RaycastHit3D, "RaycastHit3D")
    HOB_LUA_TYPE(EntityRef, "Entity")
    HOB_LUA_TYPE(Component, "Component")
    HOB_LUA_TYPE(TransformComponent, "TransformComponent")
    HOB_LUA_TYPE(TransformComponent3D, "TransformComponent3D")
    HOB_LUA_TYPE(CollisionLayer, "CollisionLayer")
    HOB_LUA_TYPE(BodyType, "BodyType")
    HOB_LUA_TYPE(MotionLock, "MotionLock")
    HOB_LUA_TYPE(RigidbodyComponent, "RigidbodyComponent")
    HOB_LUA_TYPE(CharacterBodyComponent, "CharacterBodyComponent")
    HOB_LUA_TYPE(ColliderComponent, "ColliderComponent")
    HOB_LUA_TYPE(BoxColliderComponent, "BoxColliderComponent")
    HOB_LUA_TYPE(CapsuleColliderComponent, "CapsuleColliderComponent")
    HOB_LUA_TYPE(CircleColliderComponent, "CircleColliderComponent")
    HOB_LUA_TYPE(RigidbodyComponent3D, "RigidbodyComponent3D")
    HOB_LUA_TYPE(ColliderComponent3D, "ColliderComponent3D")
    HOB_LUA_TYPE(BoxColliderComponent3D, "BoxColliderComponent3D")
    HOB_LUA_TYPE(SphereColliderComponent3D, "SphereColliderComponent3D")
    HOB_LUA_TYPE(CapsuleColliderComponent3D, "CapsuleColliderComponent3D")
    HOB_LUA_TYPE(CharacterBodyComponent3D, "CharacterBodyComponent3D")
    HOB_LUA_TYPE(InputEventType, "InputEventType")
    HOB_LUA_TYPE(InputComponent, "InputComponent")
    HOB_LUA_TYPE(SpriteComponent, "SpriteComponent")
    HOB_LUA_TYPE(SpriteAnimatorComponent, "SpriteAnimatorComponent")
    HOB_LUA_TYPE(SocketsComponent, "SocketsComponent")
    HOB_LUA_TYPE(CameraComponent, "CameraComponent")
    HOB_LUA_TYPE(CameraComponent3D, "CameraComponent3D")
    HOB_LUA_TYPE(DirectionalLightComponent, "DirectionalLightComponent")
    HOB_LUA_TYPE(MeshRendererComponent, "MeshRendererComponent")
    HOB_LUA_TYPE(AudioComponent, "AudioComponent")
    // clang-format on
} // namespace hob
