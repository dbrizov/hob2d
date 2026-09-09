#include "engine/core/logging.h"
#include "engine/core/systems/console.h"
#include "renderer.h"

namespace hob {
    void Renderer::register_cvars(Console& console) {
        console.register_cvar("r_log_textures",
                              "Log every texture load/cache-hit/destroy",
                              to_cvar_string(m_cvar_log_textures),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_log_textures = cvar.bool_value();
                              });

        console.register_cvar("r_log_materials",
                              "Log every material create/clone",
                              to_cvar_string(m_cvar_log_materials),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_log_materials = cvar.bool_value();
                              });

        console.register_cvar("r_log_shaders",
                              "Log every shader build/cache-hit",
                              to_cvar_string(m_cvar_log_shaders),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_log_shaders = cvar.bool_value();
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
                                  if (m_initialized && !init_offscreen_color_target()) {
                                      log::renderer.error("r_render_scale: failed to recreate offscreen target");
                                  }
                              });
    }

    void Renderer::log_sprite_queue() {
        if (!m_cvar_log_sprite_queue) {
            return;
        }

        log::renderer.info("Renderer sprite order ({} draws):", m_sprite_draw_order.size());
        for (size_t i = 0; i < m_sprite_draw_order.size(); ++i) {
            const SpriteDrawData& draw = m_sprite_draws[m_sprite_draw_order[i]];
            const Shader* shader = draw.get_shader();
            log::renderer.info("  [{}] z={} shader={} texture={}",
                               i,
                               draw.z_index,
                               shader != nullptr ? shader->get_debug_label() : "<none>",
                               draw.texture != nullptr ? draw.texture->get_path() : "<unknown>");
        }
    }
} // namespace hob
