#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "shader.h"
#include "texture.h"

namespace hob {
    class Material;
    using MaterialRef = std::shared_ptr<Material>;
    using MaterialWeakRef = std::weak_ptr<Material>;

    class Material {
        std::string m_name;
        ShaderRef m_shader;
        std::vector<uint8_t> m_params; // mirrors the shader's Material cbuffer, ready to push as-is
        std::unordered_map<std::string, TextureRef> m_textures;

    public:
        Material() = default;
        explicit Material(ShaderRef shader);

        const std::string& get_name() const;
        void set_name(std::string name);

        const Shader* get_shader() const;

        bool get_param(const std::string& name, float* out, uint32_t count) const;
        bool set_param(const std::string& name, const float* values, uint32_t count);

        const uint8_t* get_params_data() const;
        uint32_t get_params_size() const;

        TextureRef get_texture(const std::string& name) const;
        bool set_texture(const std::string& name, TextureRef texture);

        MaterialRef clone() const;

    private:
        bool resolve_param(const char* op, const std::string& name, uint32_t count, uint32_t& out_offset) const;
    };
} // namespace hob
