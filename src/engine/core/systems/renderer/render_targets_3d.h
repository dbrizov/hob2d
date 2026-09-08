#pragma once

#include <SDL3/SDL_gpu.h>

namespace hob {
    constexpr uint32_t BLOOM_MIP_COUNT = 5;

    struct RenderTargets3D {
        SDL_GPUTexture* hdr_color = nullptr;
        SDL_GPUTexture* depth = nullptr;
        SDL_GPUTexture* hdr_resolved = nullptr;
        SDL_GPUTexture* prepass_normal = nullptr;
        SDL_GPUTexture* prepass_depth = nullptr;
        SDL_GPUTexture* ssao_raw = nullptr;
        SDL_GPUTexture* ssao_blurred = nullptr;
        SDL_GPUTexture* fog_half = nullptr;
        SDL_GPUTexture* hdr_fogged = nullptr;
        SDL_GPUTexture* bloom_mips[BLOOM_MIP_COUNT] = {};
        uint32_t bloom_widths[BLOOM_MIP_COUNT] = {};
        uint32_t bloom_heights[BLOOM_MIP_COUNT] = {};
        uint32_t width = 0;
        uint32_t height = 0;
        SDL_GPUSampleCount sample_count = SDL_GPU_SAMPLECOUNT_1;

        bool is_valid() const {
            return hdr_color != nullptr && depth != nullptr;
        }

        bool is_multisampled() const {
            return sample_count != SDL_GPU_SAMPLECOUNT_1;
        }

        SDL_GPUTexture* get_resolved_color() const {
            return is_multisampled() ? hdr_resolved : hdr_color;
        }
    };
} // namespace hob
