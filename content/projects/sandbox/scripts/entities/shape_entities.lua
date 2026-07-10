DefineEntity.StaticBox = {
    rigidbody = {},
    box_collider = {
        collision_layer = Collision.Static,
        collision_mask = Collision.Static | Collision.Dynamic | Collision.Kinematic,
    },
    sprite = {
        texture = Textures.WhiteRect,
        material = Material {
            tint = Color.orange(),
        },
    },
}

DefineEntity.DynamicBox = {
    rigidbody = {
        body_type = BodyType.Dynamic,
    },
    box_collider = {
        collision_layer = Collision.Dynamic,
        collision_mask = Collision.Static | Collision.Dynamic | Collision.Kinematic | Collision.Trigger,
    },
    sprite = {
        texture = Textures.WhiteRect,
        material = Material {
            tint = Color.green(),
        },
    },
}

DefineEntity.TriggerBox = {
    rigidbody = {},
    box_collider = {
        trigger = true,
        collision_layer = Collision.Trigger,
        collision_mask = Collision.Static | Collision.Dynamic | Collision.Kinematic,
    },
    sprite = {
        texture = Textures.WhiteRect,
        material = Material {
            tint = Color.cyan(),
        },
    },
}

DefineEntity.StaticCircle = {
    rigidbody = {},
    circle_collider = {
        collision_layer = Collision.Static,
        collision_mask = Collision.Static | Collision.Dynamic | Collision.Kinematic,
    },
}

DefineEntity.DynamicCircle = {
    rigidbody = {
        body_type = BodyType.Dynamic,
    },
    circle_collider = {
        collision_layer = Collision.Dynamic,
        collision_mask = Collision.Static | Collision.Dynamic | Collision.Kinematic | Collision.Trigger,
    },
}

DefineEntity.TriggerCircle = {
    rigidbody = {},
    circle_collider = {
        trigger = true,
        collision_layer = Collision.Trigger,
        collision_mask = Collision.Static | Collision.Dynamic | Collision.Kinematic,
    },
}
