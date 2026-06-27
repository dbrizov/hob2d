DefineEntity.Player = {
    ticking = true,
    input = {},
    character_body = {
        collision_layer = Collision.Kinematic,
        collision_mask = Collision.Static | Collision.Dynamic | Collision.Trigger,
        solver_ignore_mask = Collision.Trigger,
        capsule = Capsule(Vector2.zero(), Vector2.zero(), 1.2),
    },
    sprite = {
        texture = Textures.PlayerIdle01,
        material = Materials.Distort,
        z_index = 1,
    },
    sprite_animator = {
        clips = {
            idle = AnimationClips.PlayerIdle,
            run = AnimationClips.PlayerRun,
        },
        default_clip = "idle",
    },
    lua_components = { Components.Player, Components.ContactLogger },
}
