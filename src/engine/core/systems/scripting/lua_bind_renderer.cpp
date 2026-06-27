#include <string>

#include "engine/core/engine.h"
#include "engine/core/logging.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/math/color.h"
#include "engine/math/vector2.h"
#include "lua_meta.h"
#include "lua_schema_factory.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    namespace {
        void apply_param(Material& mat, const std::string& name, ShaderParamType type, const sol::table& cfg) {
            switch (type) {
                case ShaderParamType::Float:
                    if (auto v = cfg.get<sol::optional<float>>(name)) {
                        const float f = *v;
                        mat.set_param(name, &f, 1);
                    }
                    break;
                case ShaderParamType::Float2:
                    if (auto v = cfg.get<sol::optional<Vector2>>(name)) {
                        const float f[2] = {v->x, v->y};
                        mat.set_param(name, f, 2);
                    }
                    break;
                case ShaderParamType::Float4:
                    if (auto v = cfg.get<sol::optional<Color>>(name)) {
                        const float f[4] = {v->r, v->g, v->b, v->a};
                        mat.set_param(name, f, 4);
                    }
                    break;
                default:
                    break;
            }
        }
    } // namespace

    void LuaScriptSystem::bind_renderer() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;
        LuaFactorySchemaRegistry& factory_schemas = m_impl->factory_schemas;
        Renderer& renderer = m_engine.get_renderer();

        bind_usertype<Material>(lua, meta)
            .factory_ctor(
                [&renderer](const sol::table& cfg) -> MaterialRef {
                    ShaderRef shader;
                    if (auto path = cfg.get<sol::optional<std::string>>("shader")) {
                        shader = renderer.get_or_build_sprite_shader(*path);
                    }

                    MaterialRef mat = renderer.create_material(shader);

                    if (const Shader* s = mat->get_shader()) {
                        for (const auto& [name, param] : s->params()) {
                            apply_param(*mat, name, param.type, cfg);
                        }
                    }
                    return mat;
                },
                {"config"})
            .method("get_param",
                    [&lua](const Material& self, const std::string& name) -> sol::object {
                        const Shader* shader = self.get_shader();
                        const ShaderParam* param = shader ? shader->find_param(name) : nullptr;
                        if (!param) {
                            return sol::make_object(lua, sol::lua_nil);
                        }

                        switch (param->type) {
                            case ShaderParamType::Float: {
                                float v = 0.0f;
                                self.get_param(name, &v, 1);
                                return sol::make_object(lua, v);
                            }
                            case ShaderParamType::Float2: {
                                float v[2] = {0.0f, 0.0f};
                                self.get_param(name, v, 2);
                                return sol::make_object(lua, Vector2(v[0], v[1]));
                            }
                            case ShaderParamType::Float4: {
                                float v[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                                self.get_param(name, v, 4);
                                return sol::make_object(lua, Color(v[0], v[1], v[2], v[3]));
                            }
                            default:
                                return sol::make_object(lua, sol::lua_nil);
                        }
                    },
                    {"name"})
            .method("set_param",
                    [](Material& self, const std::string& name, const sol::object& value) {
                        if (value.is<Color>()) {
                            const Color c = value.as<Color>();
                            const float v[4] = {c.r, c.g, c.b, c.a};
                            self.set_param(name, v, 4);
                        }
                        else if (value.is<Vector2>()) {
                            const Vector2 vec = value.as<Vector2>();
                            const float v[2] = {vec.x, vec.y};
                            self.set_param(name, v, 2);
                        }
                        else if (value.get_type() == sol::type::number) {
                            const float v = value.as<float>();
                            self.set_param(name, &v, 1);
                        }
                        else {
                            log::lua.error("Material:set_param '{}' expects a number, Vector2, or Color", name);
                        }
                    },
                    {"name", "value"})
            .method("set_shader",
                    [&renderer](Material& self, const std::string& path) {
                        self.set_shader(renderer.get_or_build_sprite_shader(path));
                    },
                    {"path"});

        bind_factory_schema<Material>(factory_schemas, "DefineMaterial", "Materials", {"shader"});
    }
} // namespace hob
