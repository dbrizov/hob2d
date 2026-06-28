#include "shader.h"

#include <cstring>

#include "engine/core/logging.h"

namespace hob {
    const char* blend_mode_to_string(BlendMode mode) {
        switch (mode) {
            case BlendMode::Alpha:
                return "alpha";
            case BlendMode::Additive:
                return "additive";
            case BlendMode::Premultiplied:
                return "premultiplied";
            case BlendMode::Opaque:
                return "opaque";
        }
        return "?";
    }

    bool blend_mode_from_string(std::string_view str, BlendMode& out) {
        if (str == "alpha") {
            out = BlendMode::Alpha;
        }
        else if (str == "additive") {
            out = BlendMode::Additive;
        }
        else if (str == "premultiplied") {
            out = BlendMode::Premultiplied;
        }
        else if (str == "opaque") {
            out = BlendMode::Opaque;
        }
        else {
            return false;
        }
        return true;
    }

    const char* cull_mode_to_string(CullMode mode) {
        switch (mode) {
            case CullMode::None:
                return "none";
            case CullMode::Back:
                return "back";
            case CullMode::Front:
                return "front";
        }
        return "?";
    }

    bool cull_mode_from_string(std::string_view str, CullMode& out) {
        if (str == "none") {
            out = CullMode::None;
        }
        else if (str == "back") {
            out = CullMode::Back;
        }
        else if (str == "front") {
            out = CullMode::Front;
        }
        else {
            return false;
        }
        return true;
    }

    Shader::Shader(SDL_GPUDevice* device,
                   SDL_GPUGraphicsPipeline* pipeline,
                   std::string relative_path,
                   BlendMode blend,
                   CullMode cull)
        : m_device(device)
        , m_pipeline(pipeline)
        , m_path(std::move(relative_path))
        , m_blend_mode(blend)
        , m_cull_mode(cull) {}

    Shader::~Shader() {
        if (m_pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipeline);
        }
    }

    const std::string& Shader::get_path() const {
        return m_path;
    }

    BlendMode Shader::get_blend_mode() const {
        return m_blend_mode;
    }

    CullMode Shader::get_cull_mode() const {
        return m_cull_mode;
    }

    SDL_GPUGraphicsPipeline* Shader::get_pipeline() const {
        return m_pipeline;
    }

    uint32_t Shader::get_engine_slot() const {
        return m_engine_slot;
    }

    uint32_t Shader::get_engine_size() const {
        return m_engine_size;
    }

    int32_t Shader::get_engine_offset(EngineBuiltin builtin) const {
        return m_engine_offsets[static_cast<size_t>(builtin)];
    }

    void Shader::set_engine_layout(uint32_t slot,
                                   uint32_t size,
                                   const std::array<int32_t, ENGINE_BUILTIN_COUNT>& offsets) {
        m_engine_slot = slot;
        m_engine_size = size;
        m_engine_offsets = offsets;
    }

    uint32_t Shader::get_material_slot() const {
        return m_material_slot;
    }

    uint32_t Shader::get_material_size() const {
        return m_material_size;
    }

    void Shader::set_material_layout(uint32_t slot,
                                     uint32_t size,
                                     std::unordered_map<std::string, ShaderParam> params) {
        m_material_slot = slot;
        m_material_size = size;
        m_params = std::move(params);
    }

    const std::unordered_map<std::string, ShaderParam>& Shader::get_params() const {
        return m_params;
    }

    const std::vector<uint8_t>& Shader::get_default_params() const {
        return m_default_params;
    }

    void Shader::set_default_params(std::vector<uint8_t> defaults) {
        m_default_params = std::move(defaults);
    }

    bool Shader::set_default_param(const std::string& name, const float* values, uint32_t count) {
        const ShaderParam* param = find_param(name);
        if (!param) {
            log::renderer.error("Shader::set_default_param: shader '{}' has no param '{}'", m_path, name);
            return false;
        }

        const uint32_t capacity = component_count(param->type);
        if (capacity == 0 || count > capacity) {
            log::renderer.error("Shader::set_default_param: param '{}' is {} ({} components), got {}",
                                name,
                                to_string(param->type),
                                capacity,
                                count);
            return false;
        }

        const uint32_t bytes = count * static_cast<uint32_t>(sizeof(float));
        if (param->offset + bytes > m_default_params.size()) {
            log::renderer.error("Shader::set_default_param: param '{}' write of {} bytes exceeds the {}-byte buffer",
                                name,
                                bytes,
                                m_default_params.size());
            return false;
        }

        std::memcpy(m_default_params.data() + param->offset, values, bytes);
        return true;
    }

    const ShaderParam* Shader::find_param(const std::string& name) const {
        auto it = m_params.find(name);
        return it != m_params.end() ? &it->second : nullptr;
    }

    const std::vector<ShaderTexture>& Shader::get_textures() const {
        return m_textures;
    }

    void Shader::set_textures(std::vector<ShaderTexture> textures) {
        m_textures = std::move(textures);
    }

    const ShaderTexture* Shader::find_texture(const std::string& name) const {
        for (const ShaderTexture& tex : m_textures) {
            if (tex.name == name) {
                return &tex;
            }
        }
        return nullptr;
    }
} // namespace hob
