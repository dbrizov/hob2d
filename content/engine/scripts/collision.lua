---@enum Collision
Collision = {
    None = 0,
    Static = 1 << 0,
    Dynamic = 1 << 1,
    Kinematic = 1 << 2,
    Trigger = 1 << 3,
}
