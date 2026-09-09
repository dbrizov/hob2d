#include <string>
#include <string_view>

#include "engine/core/asset.h"
#include "engine/core/engine.h"
#include "engine/core/logging.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/math/color.h"
#include "engine/math/vector2.h"
#include "lua_bind_helpers.h"
#include "lua_meta.h"
#include "lua_schema_asset_factory.h"
#include "lua_schema_keys.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    namespace {
        // For each reflected param present in `cfg` (as its typed Lua value), write it through `set`
        // (Material::set_param or Shader::set_default_param — both take name/floats/count).
        template<typename Setter>
        void apply_params(const ShaderParamMap& params, const sol::table& cfg, Setter set) {
            for (const auto& [name, param] : params) {
                switch (param.type) {
                    case ShaderParamType::Float:
                        if (auto v = cfg.get<sol::optional<float>>(name)) {
                            const float f = *v;
                            set(name, &f, 1);
                        }
                        break;
                    case ShaderParamType::Float2:
                        if (auto v = cfg.get<sol::optional<Vector2>>(name)) {
                            const float f[2] = {v->x, v->y};
                            set(name, f, 2);
                        }
                        break;
                    case ShaderParamType::Float4:
                        if (auto v = cfg.get<sol::optional<Color>>(name)) {
                            const float f[4] = {v->r, v->g, v->b, v->a};
                            set(name, f, 4);
                        }
                        break;
                    default:
                        break;
                }
            }
        }

        BlendMode parse_blend(const sol::optional<std::string>& value) {
            BlendMode mode = BlendMode::Alpha;
            if (value && !blend_mode_from_string(*value, mode)) {
                log::lua.error("DefineShader: unknown blend '{}' (expected alpha|additive|premultiplied|opaque)",
                               *value);
            }
            return mode;
        }

        CullMode parse_cull(const sol::optional<std::string>& value) {
            CullMode mode = CullMode::None;
            if (value && !cull_mode_from_string(*value, mode)) {
                log::lua.error("DefineShader: unknown cull '{}' (expected none|back|front)", *value);
            }
            return mode;
        }
    } // namespace

    void LuaScriptSystem::bind_renderer() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;
        LuaAssetFactorySchemaRegistry& asset_factory_schemas = m_impl->asset_factory_schemas;
        Renderer& renderer = m_engine.get_renderer();

        // Texture
        bind_usertype<Texture>(lua, meta, Bases<Asset>{})
            .factory_ctor(
                [&renderer](const sol::table& cfg) -> TextureRef {
                    const std::string path = cfg.get<sol::optional<std::string>>("path").value_or("");
                    if (path.empty()) {
                        log::lua.error("DefineTexture requires a 'path'");
                        return nullptr;
                    }

                    TextureRef texture = renderer.get_or_load_texture(path);
                    if (!texture) {
                        return nullptr;
                    }

                    const auto filter = cfg.get<sol::optional<std::string>>("filter");
                    const auto wrap = cfg.get<sol::optional<std::string>>("wrap");
                    if (filter || wrap) {
                        SamplerDesc desc = renderer.get_default_sampler_desc();
                        if (filter && !texture_filter_from_string(*filter, desc.filter)) {
                            log::lua.error(
                                "DefineTexture '{}': unknown filter '{}' (expected nearest|linear)", path, *filter);
                        }
                        if (wrap && !texture_wrap_from_string(*wrap, desc.wrap)) {
                            log::lua.error(
                                "DefineTexture '{}': unknown wrap '{}' (expected clamp|repeat|mirror)", path, *wrap);
                        }
                        texture->set_sampler(renderer.get_or_create_sampler(desc), desc);
                    }
                    return texture;
                },
                {"config"})
            .method("get_width", &Texture::get_width)
            .method("get_height", &Texture::get_height)
            .method("get_path", &Texture::get_path);

        bind_asset_factory_schema<Texture>(
            asset_factory_schemas, "DefineTexture", def_registry::TEXTURES, {"path", "wrap", "filter"});

        // Shader
        bind_usertype<Shader>(lua, meta, Bases<Asset>{})
            .factory_ctor(
                [&renderer](const sol::table& cfg) -> ShaderRef {
                    const std::string path = cfg.get<sol::optional<std::string>>("path").value_or("");
                    if (path.empty()) {
                        log::lua.error("DefineShader requires a 'path'");
                        return renderer.get_default_shader();
                    }

                    const BlendMode blend = parse_blend(cfg.get<sol::optional<std::string>>("blend"));
                    const CullMode cull = parse_cull(cfg.get<sol::optional<std::string>>("cull"));

                    ShaderRef shader = renderer.get_or_build_shader(path, blend, cull);

                    // Skip baking when the build fell back to the shared default shader (don't clobber it).
                    if (shader && shader != renderer.get_default_shader()) {
                        if (auto defaults = cfg.get<sol::optional<sol::table>>("defaults")) {
                            apply_params(shader->get_params(),
                                         *defaults,
                                         [&shader](const auto& name, const float* v, uint32_t n) {
                                             shader->set_default_param(name, v, n);
                                         });
                        }
                    }
                    return shader;
                },
                {"config"});

        bind_asset_factory_schema<Shader>(
            asset_factory_schemas, "DefineShader", def_registry::SHADERS, {"path", "blend", "cull", "defaults"});

        // Material
        bind_usertype<Material>(lua, meta, Bases<Asset>{})
            .factory_ctor(
                [&renderer](const sol::table& cfg) -> MaterialRef {
                    ShaderRef shader;
                    const sol::object shader_obj = cfg.get<sol::object>("shader");
                    if (shader_obj.is<Shader>()) {
                        shader = shader_obj.as<ShaderRef>();
                    }
                    else if (shader_obj.valid()) {
                        log::lua.error("DefineMaterial: 'shader' must be a Shaders.X reference");
                    }

                    MaterialRef mat = renderer.create_material(shader);

                    if (const Shader* s = mat->get_shader()) {
                        apply_params(s->get_params(), cfg, [&mat](const auto& name, const float* v, uint32_t n) {
                            mat->set_param(name, v, n);
                        });
                    }

                    if (auto textures = cfg.get<sol::optional<sol::table>>("textures")) {
                        for (const auto& [key, value] : *textures) {
                            const std::string name = key.as<std::string>();
                            const TextureRef texture = resolve_texture(renderer, value);
                            if (!texture) {
                                log::lua.error("DefineMaterial: texture '{}' must be a Textures.X reference or a path",
                                               name);
                                continue;
                            }
                            mat->set_texture(name, texture);
                        }
                    }
                    return mat;
                },
                {"config"})
            .method("get_param",
                    [&lua](const Material& self, std::string_view name) -> sol::object {
                        const Shader* shader = self.get_shader();
                        const ShaderParam* param = shader != nullptr ? shader->find_param(name) : nullptr;
                        if (param == nullptr) {
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
                    [](Material& self, std::string_view name, const sol::object& value) {
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
            .method("clone", [&renderer](Material& self) {
                return renderer.clone_material(self);
            });

        bind_asset_factory_schema<Material>(
            asset_factory_schemas, "DefineMaterial", def_registry::MATERIALS, {"shader", "textures"});
    }
} // namespace hob
