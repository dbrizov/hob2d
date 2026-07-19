#include <cstring>
#include <format>
#include <unordered_map>

#include <imgui.h>

#include "engine/core/logging.h"
#include "engine/core/systems/console.h"
#include "renderer.h"

namespace hob {
    namespace {
        constexpr ImGuiWindowFlags DEBUG_WINDOW_FLAGS =
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;

        std::string shader_label(const Shader* shader) {
            if (!shader) {
                return "<none>";
            }
            return std::format("{} [{}, {}]",
                               shader->get_path(),
                               blend_mode_to_string(shader->get_blend_mode()),
                               cull_mode_to_string(shader->get_cull_mode()));
        }

        std::string material_textures_label(const Material& material) {
            const Shader* shader = material.get_shader();
            if (!shader || shader->get_textures().empty()) {
                return "-";
            }

            std::string label;
            for (const ShaderTexture& st : shader->get_textures()) {
                const TextureRef& tex = material.get_texture(st.name);
                if (!label.empty()) {
                    label += ", ";
                }
                label += std::format("{}={}", st.name, tex ? tex->get_path() : "<none>");
            }
            return label;
        }
    } // namespace

    void Renderer::register_cvars(Console& console) {
        console.register_cvar("r_log_textures",
                              "Log every texture load/cache-hit/destroy",
                              to_cvar_string(m_cvar_log_textures),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_log_textures = cvar.bool_value();
                              });

        console.register_cvar("r_show_textures",
                              "Show texture cache window (size, refs, filter, wrap, path)",
                              to_cvar_string(m_cvar_show_textures),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_show_textures = cvar.bool_value();
                              });

        console.register_cvar("r_log_materials",
                              "Log every material create/clone",
                              to_cvar_string(m_cvar_log_materials),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_log_materials = cvar.bool_value();
                              });

        console.register_cvar("r_show_materials",
                              "Show material window (refs, shader, textures)",
                              to_cvar_string(m_cvar_show_materials),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_show_materials = cvar.bool_value();
                              });

        console.register_cvar("r_log_shaders",
                              "Log every shader build/cache-hit",
                              to_cvar_string(m_cvar_log_shaders),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_log_shaders = cvar.bool_value();
                              });

        console.register_cvar("r_show_shaders",
                              "Show shader window (refs, path)",
                              to_cvar_string(m_cvar_show_shaders),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_show_shaders = cvar.bool_value();
                              });

        console.register_cvar("r_log_shader_reflection",
                              "Log each shader's reflected cbuffers/textures/inputs at load",
                              to_cvar_string(m_cvar_log_shader_reflection),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_log_shader_reflection = cvar.bool_value();
                              });

        console.register_cvar("r_log_sprite_queue",
                              "Log sprite queue (z_index, shader_id, texture) each frame",
                              to_cvar_string(m_cvar_log_sprite_queue),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_log_sprite_queue = cvar.bool_value();
                              });

        console.register_cvar("r_show_sprite_queue",
                              "Show sprite queue window (z_index, shader_id, texture)",
                              to_cvar_string(m_cvar_show_sprite_queue),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_show_sprite_queue = cvar.bool_value();
                              });

        console.register_cvar("r_render_scale",
                              "Offscreen supersample factor (offscreen pixels = logical size * scale * pixel density)",
                              to_cvar_string(m_render_scale),
                              ConsoleVariableType::Float,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  const float scale = cvar.float_value();
                                  if (scale <= 0.0f) {
                                      log::renderer.error("r_render_scale must be > 0, got {}", scale);
                                      return;
                                  }

                                  m_render_scale = scale;
                                  if (m_initialized && !init_offscreen_target()) {
                                      log::renderer.error("r_render_scale: failed to recreate offscreen target");
                                  }
                              });
    }

    void Renderer::debug_textures() {
        if (!m_cvar_show_textures) {
            return;
        }

        if (ImGui::Begin("Texture Refs", nullptr, DEBUG_WINDOW_FLAGS)) {
            // Count per-texture refs held by the renderer's draw data.
            // Sprites hold a persistent TextureRef that inflates use_count() without being a "logical" holder.
            std::unordered_map<const Texture*, int> pending_refs;
            for (const auto& draw : m_sprite_draws) {
                if (draw.texture) {
                    pending_refs[draw.texture.get()] += 1;
                }
            }

            int total_refs = 0;
            for (const auto& [path, weak] : m_textures) {
                if (auto tex = weak.lock()) {
                    // Subtract 1 because `tex` itself is a strong ref held only for this iteration.
                    const int all = static_cast<int>(tex.use_count()) - 1;
                    const auto pit = pending_refs.find(tex.get());
                    const int pending = pit != pending_refs.end() ? pit->second : 0;
                    total_refs += all - pending;
                }
            }
            ImGui::Text("Textures: %zu | Refs: %d", m_textures.size(), total_refs);

            const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;

            if (ImGui::BeginTable("textures", 5, flags)) {
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Filter", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Wrap", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();

                for (const auto& [path, weak] : m_textures) {
                    auto tex = weak.lock();
                    if (!tex) {
                        continue;
                    }
                    const int all = static_cast<int>(tex.use_count()) - 1;
                    const auto pit = pending_refs.find(tex.get());
                    const int pending = pit != pending_refs.end() ? pit->second : 0;
                    const int refs = all - pending;

                    // A texture with no explicit sampler is drawn with the engine default sampler.
                    const SamplerDesc& sampler = tex->get_sampler() ? tex->get_sampler_desc() : m_default_sampler_desc;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%ux%u", tex->get_width(), tex->get_height());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", refs);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(texture_filter_to_string(sampler.filter));
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(texture_wrap_to_string(sampler.wrap));
                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextUnformatted(tex->get_path().c_str());
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void Renderer::debug_shaders() {
        if (!m_cvar_show_shaders) {
            return;
        }

        // use_count() counts the m_shaders map entry plus every Material/ShaderRef holder; subtract
        // 1 for the map's own ref to show external holders.
        if (ImGui::Begin("Shaders", nullptr, DEBUG_WINDOW_FLAGS)) {
            ImGui::Text("Shaders: %zu", m_shaders.size());

            const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("shaders", 2, flags)) {
                ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Shader", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();

                for (const auto& [key, shader] : m_shaders) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%ld", static_cast<long>(shader.use_count() - 1));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(shader_label(shader.get()).c_str());
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void Renderer::debug_materials() {
        if (!m_cvar_show_materials) {
            return;
        }

        // Each on-screen sprite's SpriteDrawData co-owns a MaterialRef (transient render-queue
        // bookkeeping, not a logical owner), so exclude those copies from the count.
        std::unordered_map<const Material*, int> draw_refs;
        for (const auto& draw : m_sprite_draws) {
            if (draw.material) {
                draw_refs[draw.material.get()] += 1;
            }
        }

        // mat is a temporary lock (+1); also subtract the render-queue copies.
        const auto ref_count = [&draw_refs](const MaterialRef& mat) {
            const auto it = draw_refs.find(mat.get());
            const int pending = it != draw_refs.end() ? it->second : 0;
            return static_cast<int>(mat.use_count()) - 1 - pending;
        };

        if (ImGui::Begin("Materials", nullptr, DEBUG_WINDOW_FLAGS)) {
            size_t live = 0;
            int total_refs = 0;
            for (const auto& weak : m_materials) {
                if (auto mat = weak.lock()) {
                    total_refs += ref_count(mat);
                    live += 1;
                }
            }
            ImGui::Text("Materials: %zu | Refs: %d", live, total_refs);

            const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("materials", 4, flags)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Shader", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Textures", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();

                for (const auto& weak : m_materials) {
                    auto mat = weak.lock();
                    if (!mat) {
                        continue;
                    }
                    const Shader* shader = mat->get_shader();

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(mat->get_name().empty() ? "<inline>" : mat->get_name().c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", ref_count(mat));
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(shader_label(shader).c_str());
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(material_textures_label(*mat).c_str());
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void Renderer::debug_sprite_queue() {
        if (m_cvar_log_sprite_queue) {
            log::renderer.info("Renderer sprite order ({} draws):", m_sprite_draw_order.size());
            for (size_t i = 0; i < m_sprite_draw_order.size(); ++i) {
                const SpriteDrawData& draw = m_sprite_draws[m_sprite_draw_order[i]];
                const char* tex_path = draw.texture ? draw.texture->get_path().c_str() : "<unknown>";
                const Shader* shader = draw.get_shader();
                log::renderer.info("  [{}] z={} shader={} texture={}", i, draw.z_index, shader_label(shader), tex_path);
            }
        }

        if (m_cvar_show_sprite_queue) {
            if (ImGui::Begin("Sprite Queue", nullptr, DEBUG_WINDOW_FLAGS)) {
                // Collapse consecutive draws that share (z_index, shader_id, texture path) into a
                // single row with a count. Adjacent identical draws form one batch, so grouping by
                // runs keeps the draw order meaningful while cutting the row count.
                const auto draw_path = [](const SpriteDrawData& draw) {
                    return draw.texture ? draw.texture->get_path().c_str() : "<unknown>";
                };

                const auto same_group = [&](const SpriteDrawData& a, const SpriteDrawData& b) {
                    return a.z_index == b.z_index && a.get_shader() == b.get_shader() &&
                           std::strcmp(draw_path(a), draw_path(b)) == 0;
                };

                // Pre-count the collapsed groups so the header can show it before the table.
                size_t group_count = 0;
                for (size_t i = 0; i < m_sprite_draw_order.size();) {
                    const SpriteDrawData& draw = m_sprite_draws[m_sprite_draw_order[i]];
                    size_t run_end = i + 1;
                    while (run_end < m_sprite_draw_order.size() &&
                           same_group(draw, m_sprite_draws[m_sprite_draw_order[run_end]])) {
                        run_end += 1;
                    }
                    group_count += 1;
                    i = run_end;
                }

                ImGui::Text("Total: %zu draws | %zu groups", m_sprite_draw_order.size(), group_count);

                const int columns = 4;
                const ImGuiTabBarFlags flags =
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;

                if (ImGui::BeginTable("queue", columns, flags)) {
                    ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                    ImGui::TableSetupColumn("Z Index", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                    ImGui::TableSetupColumn("Shader", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Texture", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableHeadersRow();

                    for (size_t i = 0; i < m_sprite_draw_order.size();) {
                        const SpriteDrawData& draw = m_sprite_draws[m_sprite_draw_order[i]];

                        size_t run_end = i + 1;
                        while (run_end < m_sprite_draw_order.size() &&
                               same_group(draw, m_sprite_draws[m_sprite_draw_order[run_end]])) {
                            run_end += 1;
                        }
                        const size_t run_length = run_end - i;

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%zu", run_length);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%d", draw.z_index);
                        ImGui::TableSetColumnIndex(2);
                        const Shader* shader = draw.get_shader();
                        ImGui::TextUnformatted(shader_label(shader).c_str());
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextUnformatted(draw_path(draw));

                        i = run_end;
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::End();
        }
    }
} // namespace hob
