#pragma once

#include <memory>
#include <string>

#include <SDL3/SDL_gpu.h>

#include "sampler.h"

namespace hob {
    class Renderer;

    class Texture;
    using TextureRef = std::shared_ptr<Texture>;
    using TextureWeakRef = std::weak_ptr<Texture>;

    class Texture {
        friend class Renderer;

        Renderer* m_renderer = nullptr;
        SDL_GPUTexture* m_gpu_texture = nullptr;
        SDL_GPUSampler* m_sampler = nullptr;
        SamplerDesc m_sampler_desc;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        std::string m_path;

        Texture(Renderer& renderer,
                SDL_GPUTexture* gpu_texture,
                uint32_t width,
                uint32_t height,
                std::string relative_path);

    public:
        ~Texture();

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        Texture(Texture&&) = delete;
        Texture& operator=(Texture&&) = delete;

        SDL_GPUTexture* get_gpu_texture() const;
        SDL_GPUSampler* get_sampler() const;
        const SamplerDesc& get_sampler_desc() const;
        void set_sampler(SDL_GPUSampler* sampler, const SamplerDesc& desc);
        uint32_t get_width() const;
        uint32_t get_height() const;
        const std::string& get_path() const;
    };
} // namespace hob
