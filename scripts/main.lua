local SCALE = 2.0
local ARENA_HALF_W = 10
local ARENA_HALF_H = 6
local S = Vector2(SCALE, SCALE)

-- Camera (owns world-to-screen scale). Must be spawned before any rendering.
EntitySpawner.spawn_entity(Entities.Camera, Vector2(0.0, 0.0))

-- Player at the center
EntitySpawner.spawn_entity(Entities.Player, Vector2(0.0, 0.0))

-- Stationary enemies for testing
EntitySpawner.spawn_entity(Entities.Enemy, Vector2(0.0, 4.0), -90.0)
EntitySpawner.spawn_entity(Entities.Enemy, Vector2(-4.0, 0.0))
EntitySpawner.spawn_entity(Entities.Enemy, Vector2(4.0, 0.0), 180.0)

-- Perimeter walls
for x = -ARENA_HALF_W, ARENA_HALF_W do
    EntitySpawner.spawn_entity(Entities.StaticBox, Vector2(x * SCALE, ARENA_HALF_H * SCALE), 0, S)  -- top
    EntitySpawner.spawn_entity(Entities.StaticBox, Vector2(x * SCALE, -ARENA_HALF_H * SCALE), 0, S) -- bottom
end
for y = -ARENA_HALF_H + 1, ARENA_HALF_H - 1 do
    EntitySpawner.spawn_entity(Entities.StaticBox, Vector2(-ARENA_HALF_W * SCALE, y * SCALE), 0, S) -- left
    EntitySpawner.spawn_entity(Entities.StaticBox, Vector2(ARENA_HALF_W * SCALE, y * SCALE), 0, S)  -- right
end

-- Interior crates: two cover lines (top and bottom)
local crate_xs = { -3, -2, 2, 3 }
for _, x in ipairs(crate_xs) do
    EntitySpawner.spawn_entity(Entities.StaticBox, Vector2(x * SCALE, 2.0 * SCALE), 0, S)
    EntitySpawner.spawn_entity(Entities.StaticBox, Vector2(x * SCALE, -2.0 * SCALE), 0, S)
end

-- Corner pillars (circles)
EntitySpawner.spawn_entity(Entities.StaticCircle, Vector2(-7.0 * SCALE, 4.0 * SCALE), 0, S)
EntitySpawner.spawn_entity(Entities.StaticCircle, Vector2(7.0 * SCALE, 4.0 * SCALE), 0, S)
EntitySpawner.spawn_entity(Entities.StaticCircle, Vector2(-7.0 * SCALE, -4.0 * SCALE), 0, S)
EntitySpawner.spawn_entity(Entities.StaticCircle, Vector2(7.0 * SCALE, -4.0 * SCALE), 0, S)

-- Mid-line pillars (gives the center extra cover to weave around)
EntitySpawner.spawn_entity(Entities.StaticCircle, Vector2(-5.0 * SCALE, 0.0), 0, S)
EntitySpawner.spawn_entity(Entities.StaticCircle, Vector2(5.0 * SCALE, 0.0), 0, S)

-- Checkpoint triggers (top and bottom of the arena)
EntitySpawner.spawn_entity(Entities.TriggerCircle, Vector2(0.0, 4.0 * SCALE), 0, S)
EntitySpawner.spawn_entity(Entities.TriggerCircle, Vector2(0.0, -4.0 * SCALE), 0, S)

-- UI test (Step 3): load and show the test document from Lua, log clicks from Lua
local ui_doc = UI.load_document("ui/test.rml")
UI.show_document(ui_doc)

local btn = UI.get_element(ui_doc, "btn")
local btn_listener = UI.add_event_listener(btn, "click", function()
    Debug.log("[Lua] button clicked")
end)
-- UI.remove_event_listener(btn_listener)

-- Dynamic entities for collision testing. With Y-up gravity these fall
-- and pile up on the bottom wall and crates.
-- local dyn_y = (ARENA_HALF_H - 1) * SCALE
-- for x = -8, 8, 2 do
--     EntitySpawner.spawn_entity("DynamicBox", Vector2(x, dyn_y))
--     EntitySpawner.spawn_entity("DynamicCircle", Vector2(x + 1.0, dyn_y - 2.0))
-- end
