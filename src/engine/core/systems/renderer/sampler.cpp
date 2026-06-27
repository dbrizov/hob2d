#include "sampler.h"

namespace hob {
    namespace {
        SDL_GPUFilter to_sdl_filter(TextureFilter filter) {
            return filter == TextureFilter::Linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        }

        SDL_GPUSamplerAddressMode to_sdl_address_mode(TextureWrap wrap) {
            switch (wrap) {
                case TextureWrap::Clamp:
                    return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                case TextureWrap::Repeat:
                    return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
                case TextureWrap::Mirror:
                    return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
            }
            return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        }
    } // namespace

    const char* texture_filter_to_string(TextureFilter filter) {
        switch (filter) {
            case TextureFilter::Nearest:
                return "nearest";
            case TextureFilter::Linear:
                return "linear";
        }
        return "?";
    }

    bool texture_filter_from_string(std::string_view str, TextureFilter& out) {
        if (str == "nearest") {
            out = TextureFilter::Nearest;
        }
        else if (str == "linear") {
            out = TextureFilter::Linear;
        }
        else {
            return false;
        }
        return true;
    }

    const char* texture_wrap_to_string(TextureWrap wrap) {
        switch (wrap) {
            case TextureWrap::Clamp:
                return "clamp";
            case TextureWrap::Repeat:
                return "repeat";
            case TextureWrap::Mirror:
                return "mirror";
        }
        return "?";
    }

    bool texture_wrap_from_string(std::string_view str, TextureWrap& out) {
        if (str == "clamp") {
            out = TextureWrap::Clamp;
        }
        else if (str == "repeat") {
            out = TextureWrap::Repeat;
        }
        else if (str == "mirror") {
            out = TextureWrap::Mirror;
        }
        else {
            return false;
        }
        return true;
    }

    uint32_t SamplerDesc::key() const {
        return (static_cast<uint32_t>(filter) << 8) | static_cast<uint32_t>(wrap);
    }

    SDL_GPUSamplerCreateInfo to_sdl_sampler_create_info(const SamplerDesc& desc) {
        const SDL_GPUFilter filter = to_sdl_filter(desc.filter);
        const SDL_GPUSamplerAddressMode address = to_sdl_address_mode(desc.wrap);

        SDL_GPUSamplerCreateInfo info{};
        info.min_filter = filter;
        info.mag_filter = filter;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.address_mode_u = address;
        info.address_mode_v = address;
        info.address_mode_w = address;
        return info;
    }
} // namespace hob
