#include "texture.h"

#include "renderer.h"

namespace hob {
    Texture::Texture(Renderer& renderer, SDL_GPUTexture* gpu_texture, uint32_t width, uint32_t height, std::string path)
        : m_renderer(&renderer)
        , m_gpu_texture(gpu_texture)
        , m_width(width)
        , m_height(height)
        , m_path(std::move(path)) {}

    Texture::~Texture() {
        if (m_renderer != nullptr) {
            m_renderer->release_texture(*this);
        }
    }

    SDL_GPUTexture* Texture::get_gpu_texture() const {
        return m_gpu_texture;
    }

    SDL_GPUSampler* Texture::get_sampler() const {
        return m_sampler;
    }

    const SamplerDesc& Texture::get_sampler_desc() const {
        return m_sampler_desc;
    }

    void Texture::set_sampler(SDL_GPUSampler* sampler, const SamplerDesc& desc) {
        m_sampler = sampler;
        m_sampler_desc = desc;
    }

    uint32_t Texture::get_width() const {
        return m_width;
    }

    uint32_t Texture::get_height() const {
        return m_height;
    }

    const std::string& Texture::get_path() const {
        return m_path;
    }
} // namespace hob
