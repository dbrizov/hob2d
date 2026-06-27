#pragma once

#include <string_view>

#include <SDL3/SDL_gpu.h>

namespace hob {
    enum class TextureFilter {
        Nearest,
        Linear,
    };

    enum class TextureWrap {
        Clamp,
        Repeat,
        Mirror,
    };

    const char* texture_filter_to_string(TextureFilter filter);
    bool texture_filter_from_string(std::string_view str, TextureFilter& out);

    const char* texture_wrap_to_string(TextureWrap wrap);
    bool texture_wrap_from_string(std::string_view str, TextureWrap& out);

    struct SamplerDesc {
        TextureFilter filter = TextureFilter::Linear;
        TextureWrap wrap = TextureWrap::Clamp;

        uint32_t key() const;
    };

    SDL_GPUSamplerCreateInfo to_sdl_sampler_create_info(const SamplerDesc& desc);
} // namespace hob
