#include "shader.h"

namespace hob {
    Shader::Shader(SDL_GPUDevice* device, SDL_GPUGraphicsPipeline* pipeline, std::string path)
        : m_device(device)
        , m_pipeline(pipeline)
        , m_path(std::move(path)) {}

    Shader::~Shader() {
        if (m_pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipeline);
        }
    }

    const std::string& Shader::path() const {
        return m_path;
    }

    SDL_GPUGraphicsPipeline* Shader::pipeline() const {
        return m_pipeline;
    }

    uint32_t Shader::engine_slot() const {
        return m_engine_slot;
    }

    uint32_t Shader::material_slot() const {
        return m_material_slot;
    }

    uint32_t Shader::material_size() const {
        return m_material_size;
    }

    const std::unordered_map<std::string, ShaderParam>& Shader::params() const {
        return m_params;
    }

    const std::vector<uint8_t>& Shader::default_params() const {
        return m_default_params;
    }

    const ShaderParam* Shader::find_param(const std::string& name) const {
        auto it = m_params.find(name);
        return it != m_params.end() ? &it->second : nullptr;
    }

    void Shader::set_engine_slot(uint32_t slot) {
        m_engine_slot = slot;
    }

    void Shader::set_material_layout(uint32_t slot,
                                     uint32_t size,
                                     std::unordered_map<std::string, ShaderParam> params) {
        m_material_slot = slot;
        m_material_size = size;
        m_params = std::move(params);
    }

    void Shader::set_default_params(std::vector<uint8_t> defaults) {
        m_default_params = std::move(defaults);
    }
} // namespace hob
