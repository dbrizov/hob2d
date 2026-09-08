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

        console.register_cvar("r_log_mesh_queue",
                              "Log mesh queue (shader, mesh) each frame",
                              to_cvar_string(m_cvar_log_mesh_queue),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_log_mesh_queue = cvar.bool_value();
                              });

        console.register_cvar("r_show_mesh_queue",
                              "Show mesh queue window (shader, mesh, material)",
                              to_cvar_string(m_cvar_show_mesh_queue),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_show_mesh_queue = cvar.bool_value();
                              });

        console.register_cvar("r_msaa",
                              "MSAA sample count for the 3D pass (1, 2, 4, 8); falls back to the nearest supported",
                              to_cvar_string(m_cvar_msaa),
                              ConsoleVariableType::Int,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_msaa = cvar.int_value();
                                  if (!m_initialized) {
                                      return;
                                  }

                                  m_msaa_sample_count = probe_msaa_sample_count(m_cvar_msaa);
                                  rebuild_mesh_shader_pipelines();
                                  if (!init_offscreen_targets()) {
                                      log::renderer.error("r_msaa: failed to recreate offscreen targets");
                                  }
                              });

        console.register_cvar("r_sky",
                              "Draw the procedural sky behind the 3D pass",
                              to_cvar_string(m_cvar_sky),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_sky = cvar.bool_value();
                              });

        console.register_cvar("r_shadows",
                              "Render the sun shadow map and apply it in PBR shaders",
                              to_cvar_string(m_cvar_shadows),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_shadows = cvar.bool_value();
                              });

        console.register_cvar("r_shadow_bias",
                              "Depth bias subtracted before the shadow comparison (light clip depth units)",
                              to_cvar_string(m_cvar_shadow_bias),
                              ConsoleVariableType::Float,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_shadow_bias = cvar.float_value();
                              });

        console.register_cvar("r_show_shadow_map",
                              "Show the sun shadow map",
                              to_cvar_string(m_cvar_show_shadow_map),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_show_shadow_map = cvar.bool_value();
                              });

        console.register_cvar("r_ssao",
                              "Depth/normal prepass and screen-space ambient occlusion for the 3D pass",
                              to_cvar_string(m_cvar_ssao),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_ssao = cvar.bool_value();
                              });

        console.register_cvar("r_ssao_radius",
                              "SSAO sampling radius in metres",
                              to_cvar_string(m_cvar_ssao_radius),
                              ConsoleVariableType::Float,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_ssao_radius = cvar.float_value();
                              });

        console.register_cvar("r_ssao_intensity",
                              "SSAO occlusion strength",
                              to_cvar_string(m_cvar_ssao_intensity),
                              ConsoleVariableType::Float,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_ssao_intensity = cvar.float_value();
                              });

        console.register_cvar("r_show_ssao",
                              "Show the blurred SSAO buffer of the game view",
                              to_cvar_string(m_cvar_show_ssao),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_show_ssao = cvar.bool_value();
                              });

        console.register_cvar("r_show_frame_stats",
                              "Show frame time, draw counts and 3D target settings",
                              to_cvar_string(m_cvar_show_frame_stats),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_show_frame_stats = cvar.bool_value();
                              });

        console.register_cvar("r_fog",
                              "Volumetric height fog lit by the sun (needs a directional light with fog density)",
                              to_cvar_string(m_cvar_fog),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_fog = cvar.bool_value();
                              });

        console.register_cvar("r_fog_steps",
                              "Ray-march steps per pixel for the volumetric fog",
                              to_cvar_string(m_cvar_fog_steps),
                              ConsoleVariableType::Int,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_fog_steps = cvar.int_value();
                              });

        console.register_cvar("r_show_fog",
                              "Show the half-resolution fog in-scatter buffer of the game view",
                              to_cvar_string(m_cvar_show_fog),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_show_fog = cvar.bool_value();
                              });

        console.register_cvar("r_bloom",
                              "Bloom on the 3D pass",
                              to_cvar_string(m_cvar_bloom),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_bloom = cvar.bool_value();
                              });

        console.register_cvar("r_bloom_threshold",
                              "Linear brightness above which pixels bloom",
                              to_cvar_string(m_cvar_bloom_threshold),
                              ConsoleVariableType::Float,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_bloom_threshold = cvar.float_value();
                              });

        console.register_cvar("r_bloom_intensity",
                              "Bloom contribution added before tonemapping",
                              to_cvar_string(m_cvar_bloom_intensity),
                              ConsoleVariableType::Float,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_bloom_intensity = cvar.float_value();
                              });

        console.register_cvar("r_exposure",
                              "Exposure multiplier applied before tonemapping the 3D pass",
                              to_cvar_string(m_cvar_exposure),
                              ConsoleVariableType::Float,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_exposure = cvar.float_value();
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
                                  if (m_initialized && !init_offscreen_targets()) {
                                      log::renderer.error("r_render_scale: failed to recreate offscreen targets");
                                  }
                              });
    }

    void Renderer::debug_shadow_map() {
        if (!m_cvar_show_shadow_map) {
            return;
        }

        if (ImGui::Begin(" Shadow Map ###Shadow Map", nullptr, DEBUG_WINDOW_FLAGS)) {
            ImGui::Text("%ux%u, %.0f m extent", SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, SHADOW_EXTENT_METERS);
            ImGui::Image(reinterpret_cast<ImTextureID>(m_shadow_map), ImVec2(512.0f, 512.0f));
        }
        ImGui::End();
    }

    void Renderer::debug_frame_stats() {
        if (!m_cvar_show_frame_stats) {
            return;
        }

        if (ImGui::Begin(" Frame Stats ###Frame Stats", nullptr, DEBUG_WINDOW_FLAGS)) {
            const float frame_ms = m_stats_frame_seconds * 1000.0f;
            ImGui::Text("Frame: %.2f ms (%.0f fps)", frame_ms, frame_ms > 0.0f ? 1000.0f / frame_ms : 0.0f);
            ImGui::Text("Sprite draws: %u", m_stats_sprite_draws);
            ImGui::Text("Mesh draws: %u (%u triangles)", m_stats_mesh_draws, m_stats_mesh_triangles);
            ImGui::Text("3D target: %ux%u, MSAA %dx",
                        m_offscreen_targets_3d.width,
                        m_offscreen_targets_3d.height,
                        1 << static_cast<int32_t>(m_msaa_sample_count));
            ImGui::Text("Shadows %s | SSAO %s | Fog %s | Bloom %s | Sky %s",
                        m_cvar_shadows ? "on" : "off",
                        m_cvar_ssao ? "on" : "off",
                        m_cvar_fog ? "on" : "off",
                        m_cvar_bloom ? "on" : "off",
                        m_cvar_sky ? "on" : "off");
        }
        ImGui::End();
    }

    void Renderer::debug_fog() {
        if (!m_cvar_show_fog || m_offscreen_targets_3d.fog_half == nullptr) {
            return;
        }

        if (ImGui::Begin(" Volumetric Fog ###Volumetric Fog", nullptr, DEBUG_WINDOW_FLAGS)) {
            const float aspect = m_offscreen_targets_3d.height > 0
                                     ? static_cast<float>(m_offscreen_targets_3d.width) /
                                           static_cast<float>(m_offscreen_targets_3d.height)
                                     : 1.0f;
            ImGui::Image(reinterpret_cast<ImTextureID>(m_offscreen_targets_3d.fog_half),
                         ImVec2(640.0f, 640.0f / aspect),
                         ImVec2(0.0f, 1.0f),
                         ImVec2(1.0f, 0.0f));
        }
        ImGui::End();
    }

    void Renderer::debug_ssao() {
        if (!m_cvar_show_ssao || m_offscreen_targets_3d.ssao_blurred == nullptr) {
            return;
        }

        if (ImGui::Begin(" SSAO ###SSAO", nullptr, DEBUG_WINDOW_FLAGS)) {
            const float aspect = m_offscreen_targets_3d.height > 0
                                     ? static_cast<float>(m_offscreen_targets_3d.width) /
                                           static_cast<float>(m_offscreen_targets_3d.height)
                                     : 1.0f;
            ImGui::Image(reinterpret_cast<ImTextureID>(m_offscreen_targets_3d.ssao_blurred),
                         ImVec2(640.0f, 640.0f / aspect),
                         ImVec2(0.0f, 1.0f),
                         ImVec2(1.0f, 0.0f));
        }
        ImGui::End();
    }

    void Renderer::debug_textures() {
        if (!m_cvar_show_textures) {
            return;
        }

        if (ImGui::Begin(" Texture Refs ###Texture Refs", nullptr, DEBUG_WINDOW_FLAGS)) {
            int32_t total_refs = 0;
            for (const auto& [path, weak] : m_textures) {
                if (auto tex = weak.lock()) {
                    // Subtract 1 because `tex` itself is a strong ref held only for this iteration.
                    total_refs += static_cast<int32_t>(tex.use_count()) - 1;
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
                    const int32_t refs = static_cast<int32_t>(tex.use_count()) - 1;

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
        if (ImGui::Begin(" Shaders ###Shaders", nullptr, DEBUG_WINDOW_FLAGS)) {
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

        // mat is a temporary lock (+1).
        const auto ref_count = [](const MaterialRef& mat) {
            return static_cast<int32_t>(mat.use_count()) - 1;
        };

        if (ImGui::Begin(" Materials ###Materials", nullptr, DEBUG_WINDOW_FLAGS)) {
            size_t live = 0;
            int32_t total_refs = 0;
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
            if (ImGui::Begin(" Sprite Queue ###Sprite Queue", nullptr, DEBUG_WINDOW_FLAGS)) {
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

                const int32_t columns = 4;
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
    void Renderer::debug_mesh_queue() {
        if (m_cvar_log_mesh_queue) {
            for (const uint32_t index : m_mesh_draw_order) {
                const MeshDrawData& draw = m_mesh_draws[index];
                log::renderer.info("[mesh queue] shader={} mesh={} material={}",
                                   draw.get_shader() ? draw.get_shader()->get_path() : "<none>",
                                   draw.mesh ? draw.mesh->get_source() : "<none>",
                                   draw.material ? draw.material->get_name() : "<none>");
            }
        }

        if (!m_cvar_show_mesh_queue) {
            return;
        }

        if (ImGui::Begin("Mesh Queue", &m_cvar_show_mesh_queue)) {
            ImGui::Text("Draws: %zu  Meshes: %zu", m_mesh_draws.size(), m_meshes.size());
            if (ImGui::BeginTable("mesh_queue", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("#");
                ImGui::TableSetupColumn("Shader");
                ImGui::TableSetupColumn("Mesh");
                ImGui::TableSetupColumn("Material");
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < m_mesh_draw_order.size(); ++i) {
                    const MeshDrawData& draw = m_mesh_draws[m_mesh_draw_order[i]];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%zu", i);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(draw.get_shader() ? draw.get_shader()->get_path().c_str() : "<none>");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(draw.mesh ? draw.mesh->get_source().c_str() : "<none>");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(draw.material ? draw.material->get_name().c_str() : "<none>");
                }

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
} // namespace hob
