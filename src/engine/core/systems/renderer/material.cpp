#include "material.h"

#include <cstring>

#include "engine/core/logging.h"

namespace hob {
    Material::Material(ShaderRef shader)
        : m_shader(std::move(shader))
        , m_params(m_shader ? m_shader->default_params() : std::vector<uint8_t>{}) {}

    const std::string& Material::get_name() const {
        return m_name;
    }

    void Material::set_name(std::string name) {
        m_name = std::move(name);
    }

    const Shader* Material::get_shader() const {
        return m_shader.get();
    }

    bool Material::get_param(const std::string& name, float* out, uint32_t count) const {
        uint32_t offset = 0;
        if (!resolve_param("get_param", name, count, offset)) {
            return false;
        }

        std::memcpy(out, m_params.data() + offset, count * sizeof(float));
        return true;
    }

    bool Material::set_param(const std::string& name, const float* values, uint32_t count) {
        uint32_t offset = 0;
        if (!resolve_param("set_param", name, count, offset)) {
            return false;
        }

        std::memcpy(m_params.data() + offset, values, count * sizeof(float));
        return true;
    }

    const uint8_t* Material::param_data() const {
        return m_params.data();
    }

    uint32_t Material::param_size() const {
        return static_cast<uint32_t>(m_params.size());
    }

    MaterialRef Material::clone() const {
        return std::make_shared<Material>(*this);
    }

    bool Material::resolve_param(const char* op, const std::string& name, uint32_t count, uint32_t& out_offset) const {
        if (!m_shader) {
            log::renderer.error("Material::{}: '{}' on a material with no shader", op, name);
            return false;
        }

        const ShaderParam* param = m_shader->find_param(name);
        if (!param) {
            log::renderer.error("Material::{}: shader '{}' has no param '{}'", op, m_shader->path(), name);
            return false;
        }

        const uint32_t capacity = component_count(param->type);
        if (capacity == 0 || count > capacity) {
            log::renderer.error("Material::{}: param '{}' is {} ({} components), got {}",
                                op,
                                name,
                                to_string(param->type),
                                capacity,
                                count);
            return false;
        }

        const uint32_t bytes = count * static_cast<uint32_t>(sizeof(float));
        if (param->offset + bytes > m_params.size()) {
            log::renderer.error("Material::{}: param '{}' write of {} bytes exceeds the {}-byte buffer",
                                op,
                                name,
                                bytes,
                                m_params.size());
            return false;
        }

        out_offset = param->offset;
        return true;
    }
} // namespace hob
