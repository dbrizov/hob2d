local spawned_listeners = {}
local destroyed_listeners = {}
local cleared_listeners = {}

function on_entity_spawned(listener)
    spawned_listeners[#spawned_listeners + 1] = listener
end

function on_entity_destroyed(listener)
    destroyed_listeners[#destroyed_listeners + 1] = listener
end

function on_entities_cleared(listener)
    cleared_listeners[#cleared_listeners + 1] = listener
end

function __on_entity_spawned(entity_id)
    for _, listener in ipairs(spawned_listeners) do
        listener(entity_id)
    end
end

function __on_entity_destroyed(entity_id)
    for _, listener in ipairs(destroyed_listeners) do
        listener(entity_id)
    end
end

function __on_entities_cleared()
    for _, listener in ipairs(cleared_listeners) do
        listener()
    end
end
