-- Which file declared each definition, and the uniqueness rule built on it.

__def_sources = {}

function __clear_def_sources()
    __def_sources = {}
end

local DEF_FILE_EXTENSION = {
    [DefRegistry.SCENES] = FileExtension.SCENE,
    [DefRegistry.ENTITIES] = FileExtension.PREFAB,
    [DefRegistry.MATERIALS] = FileExtension.MATERIAL,
    [DefRegistry.ANIMATION_CLIPS] = FileExtension.ANIMATION_CLIP,
    [DefRegistry.SHADERS] = FileExtension.SHADER,
    [DefRegistry.TEXTURES] = FileExtension.META,
    [DefRegistry.AUDIO_CLIPS] = FileExtension.META,
}

local function check_file_name_matches(registry, name, path)
    local extension = DEF_FILE_EXTENSION[registry]
    if extension == nil or path:sub(- #extension) ~= extension then
        return
    end

    local expected = __def_name_from_file(path, extension)
    if expected ~= name then
        Log.error(registry .. "." .. tostring(name) .. " is declared in '" .. path .. "', which names '" ..
            tostring(expected) .. "'. A definition is named after its file.")
    end
end

-- debug.getinfo counts frames outwards from here: 1 is this function, 2 the DefineX __newindex
-- that called it, 3 the chunk that made the assignment. Callers must therefore be metamethods.
local DEFINITION_STACK_LEVEL = 3

function __record_def_source(registry, name)
    -- luaL_loadfile names a file chunk "@<path>". Anything else (a string chunk, precompiled)
    -- carries no path, so it registers untracked rather than being rejected.
    local info = debug.getinfo(DEFINITION_STACK_LEVEL, "S")
    local source = info and info.source
    if source == nil or source:sub(1, 1) ~= "@" then
        return true
    end

    local path = source:sub(2)

    local by_name = __def_sources[registry]
    if by_name == nil then
        by_name = {}
        __def_sources[registry] = by_name
    end

    local recorded = by_name[name]
    if recorded ~= nil and recorded ~= path then
        Log.error(registry .. "." .. tostring(name) .. " is already declared in '" .. recorded ..
            "', so the one in '" .. path .. "' is rejected. Definition names must be unique.")
        return false
    end

    by_name[name] = path
    check_file_name_matches(registry, name, path)

    return true
end

function __get_def_source(registry, name)
    local by_name = __def_sources[registry]
    return by_name and by_name[name] or nil
end

function __count_defs_in_file(path)
    local count = 0
    for _, by_name in pairs(__def_sources) do
        for _, source in pairs(by_name) do
            if source == path then
                count = count + 1
            end
        end
    end

    return count
end

function __def_name_from_file(path, suffix)
    local file_name = path:match("[^/\\]+$") or path
    if file_name:sub(- #suffix) ~= suffix then
        return nil
    end

    local stem = file_name:sub(1, #file_name - #suffix)

    if suffix == FileExtension.META then
        stem = stem:gsub("%.[^.]*$", "")
    end

    return stem
end
