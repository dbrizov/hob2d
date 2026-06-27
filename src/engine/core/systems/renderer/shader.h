#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "engine/math/constants.h"
#include "shader_reflection.h"

namespace hob {
    class Shader;
    using ShaderRef = std::shared_ptr<Shader>;

    constexpr uint32_t INVALID_SHADER_SLOT = MAX_UINT32;

    struct ShaderParam {
        ShaderParamType type = ShaderParamType::Unknown;
        uint32_t offset = 0;
        uint32_t size = 0;
    };

    class Shader {
        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUGraphicsPipeline* m_pipeline = nullptr;
        std::string m_path;

        uint32_t m_engine_slot = INVALID_SHADER_SLOT; // engine-filled cbuffer (texel_size/time)
        uint32_t m_material_slot = INVALID_SHADER_SLOT; // user-facing "Material" cbuffer
        uint32_t m_material_size = 0;
        std::unordered_map<std::string, ShaderParam> m_params;
        std::vector<uint8_t> m_default_params;

    public:
        Shader(SDL_GPUDevice* device, SDL_GPUGraphicsPipeline* pipeline, std::string path);
        ~Shader();

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;
        Shader(Shader&&) = delete;
        Shader& operator=(Shader&&) = delete;

        const std::string& path() const;
        SDL_GPUGraphicsPipeline* pipeline() const;

        uint32_t engine_slot() const;
        uint32_t material_slot() const;
        uint32_t material_size() const;
        const std::unordered_map<std::string, ShaderParam>& params() const;
        const std::vector<uint8_t>& default_params() const;
        const ShaderParam* find_param(const std::string& name) const;

        void set_engine_slot(uint32_t slot);
        void set_material_layout(uint32_t slot, uint32_t size, std::unordered_map<std::string, ShaderParam> params);
        void set_default_params(std::vector<uint8_t> defaults);
    };
} // namespace hob
