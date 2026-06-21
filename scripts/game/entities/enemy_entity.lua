DefineEntity.Enemy = {
    ticking = true,
    transform = {
        interpolate_physics = false,
    },
    character_body = {
        collision_layer = Collision.Kinematic,
        collision_mask = Collision.Static | Collision.Dynamic | Collision.Trigger,
        solver_ignore_mask = Collision.Trigger,
        capsule = Capsule(Vector2.zero(), Vector2.zero(), 1.2),
    },
    sprite = {
        texture = Textures.PlayerIdle01,
        material = Materials.RedOutline,
        z_index = 0,
    },
    sprite_animator = {
        clips = {
            idle = AnimationClips.PlayerIdle,
            run = AnimationClips.PlayerRun,
        },
        default_clip = "idle",
    },
    lua_components = { Components.ContactLogger, Components.EnemyHealthbar },
}
