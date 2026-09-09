#include "sdl_context.h"

#include <SDL3/SDL.h>

#include "engine/core/assert.h"
#include "engine/core/logging.h"

namespace hob {
    namespace {
        void SDLCALL sdl_log_output(void* userdata, int32_t category, SDL_LogPriority priority, const char* message) {
            (void)userdata;
            (void)category;
            if (priority >= SDL_LOG_PRIORITY_ERROR) {
                log::sdl.error("{}", message);
            }
            else {
                log::sdl.info("{}", message);
            }
        }
    } // namespace

    SdlContext::SdlContext() {
        SDL_SetLogOutputFunction(sdl_log_output, nullptr);

        const bool sdl_initialized = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD);
        HOB_CHECK(sdl_initialized, "SDL_Init failed: {}", SDL_GetError());

        log::sdl.info("SDL_Init");

        const SDL_GPUShaderFormat shader_formats =
            SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL;

#ifndef NDEBUG
        const bool debug_mode = true;
#else
        const bool debug_mode = false;
#endif

        m_gpu_device = SDL_CreateGPUDevice(shader_formats, debug_mode, nullptr);
        HOB_CHECK(m_gpu_device != nullptr, "SDL_CreateGPUDevice failed: {}", SDL_GetError());

        log::sdl.info("SDL_CreateGPUDevice ({})", SDL_GetGPUDeviceDriver(m_gpu_device));
    }

    SdlContext::~SdlContext() {
        if (m_gpu_device != nullptr) {
            SDL_DestroyGPUDevice(m_gpu_device);
            m_gpu_device = nullptr;
            log::sdl.info("SDL_DestroyGPUDevice");
        }

        SDL_Quit();
        log::sdl.info("SDL_Quit");
    }

    SDL_GPUDevice* SdlContext::get_gpu_device() const {
        return m_gpu_device;
    }
} // namespace hob
