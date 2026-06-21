DefineComponent.ContactLogger = {}
---@class ContactLogger : LuaComponent
local ContactLogger = ContactLogger

function ContactLogger:on_collision_enter(other)
    Debug.print("collision_enter: " .. other, Color.white(), 5.0)
end

function ContactLogger:on_collision_exit(other)
    Debug.print("collision_exit: " .. other, Color.white(), 5.0)
end

function ContactLogger:on_trigger_enter(other)
    Debug.print("trigger_enter: " .. other, Color.white(), 5.0)
end

function ContactLogger:on_trigger_exit(other)
    Debug.print("trigger_exit: " .. other, Color.white(), 5.0)
end
