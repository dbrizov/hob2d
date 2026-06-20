#include <string>

#include "engine/core/engine.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/math/color.h"
#include "lua_meta.h"
#include "lua_schema_factory.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    void LuaScriptSystem::bind_renderer() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;
        LuaFactorySchemaRegistry& factory_schemas = m_impl->factory_schemas;
        Renderer& renderer = m_engine.get_renderer();

        bind_usertype<Material>(lua, meta)
            .factory_ctor(
                [&renderer](const sol::table& mat_t) {
                    Material mat;
                    if (auto path = mat_t.get<sol::optional<std::string>>("shader")) {
                        mat.shader_id = renderer.get_or_build_sprite_shader(*path);
                    }
                    if (auto tint = mat_t.get<sol::optional<Color>>("tint")) {
                        mat.tint = *tint;
                    }
                    if (auto oc = mat_t.get<sol::optional<Color>>("outline_color")) {
                        mat.outline_color = *oc;
                    }
                    if (auto ow = mat_t.get<sol::optional<float>>("outline_width")) {
                        mat.outline_width = *ow;
                    }
                    if (auto at = mat_t.get<sol::optional<float>>("alpha_threshold")) {
                        mat.alpha_threshold = *at;
                    }
                    return mat;
                },
                {"config"})
            .method("get_tint",
                    [](const Material& self) {
                        return self.tint;
                    })
            .method("set_tint",
                    [](Material& self, const Color& tint) {
                        self.tint = tint;
                    },
                    {"tint"})
            .method("get_outline_color",
                    [](const Material& self) {
                        return self.outline_color;
                    })
            .method("set_outline_color",
                    [](Material& self, const Color& color) {
                        self.outline_color = color;
                    },
                    {"color"})
            .method("get_outline_width",
                    [](const Material& self) {
                        return self.outline_width;
                    })
            .method("set_outline_width",
                    [](Material& self, float width) {
                        self.outline_width = width;
                    },
                    {"width"})
            .method("get_alpha_threshold",
                    [](const Material& self) {
                        return self.alpha_threshold;
                    })
            .method("set_alpha_threshold",
                    [](Material& self, float threshold) {
                        self.alpha_threshold = threshold;
                    },
                    {"threshold"})
            .method("set_shader",
                    [&renderer](Material& self, const std::string& path) {
                        self.shader_id = renderer.get_or_build_sprite_shader(path);
                    },
                    {"path"});

        bind_factory_schema<Material>(factory_schemas,
                                      "DefineMaterial",
                                      "Materials",
                                      {"shader", "tint", "outline_color", "outline_width", "alpha_threshold"});
    }
} // namespace hob
