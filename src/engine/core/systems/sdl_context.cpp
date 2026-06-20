#include "sdl_context.h"

#include <SDL3/SDL.h>

#include "engine/core/debug.h"
#include "engine/core/engine_config.h"

namespace hob {
    SdlContext::SdlContext(const GraphicsConfig& graphics_config) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            debug::log_error("SDL_Init Error: {}", SDL_GetError());
            return;
        }

        debug::log("SDL_Init");

        // Treat window size as physical pixels.
        // SDL_CreateWindow takes logical points, and on HiDPI displays (e.g. macOS Retina) the logical desktop
        // is smaller than the pixel resolution — so passing pixels as points makes the window overflow the display.
        float pixel_density = 1.0f;
        const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
        if (mode != nullptr && mode->pixel_density > 0.0f) {
            pixel_density = mode->pixel_density;
        }

        const int window_width_points =
            static_cast<int>(static_cast<float>(graphics_config.window_width) / pixel_density);
        const int window_height_points =
            static_cast<int>(static_cast<float>(graphics_config.window_height) / pixel_density);

        m_window = SDL_CreateWindow(graphics_config.window_title.c_str(),
                                    window_width_points,
                                    window_height_points,
                                    SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

        if (!m_window) {
            debug::log_error("SDL_CreateWindow Error: {}", SDL_GetError());
            SDL_Quit();
            return;
        }

        int pixel_width = 0;
        int pixel_height = 0;
        SDL_GetWindowSizeInPixels(m_window, &pixel_width, &pixel_height);
        debug::log("SDL_CreateWindow ({}x{} pts, {}x{} px, density {})",
                   window_width_points,
                   window_height_points,
                   pixel_width,
                   pixel_height,
                   pixel_density);

        const SDL_GPUShaderFormat shader_formats =
            SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL;

#ifndef NDEBUG
        const bool debug_mode = true;
#else
        const bool debug_mode = false;
#endif

        m_gpu_device = SDL_CreateGPUDevice(shader_formats, debug_mode, nullptr);
        if (!m_gpu_device) {
            debug::log_error("SDL_CreateGPUDevice Error: {}", SDL_GetError());
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
            SDL_Quit();
            return;
        }

        debug::log("SDL_CreateGPUDevice ({})", SDL_GetGPUDeviceDriver(m_gpu_device));

        if (!SDL_ClaimWindowForGPUDevice(m_gpu_device, m_window)) {
            debug::log_error("SDL_ClaimWindowForGPUDevice Error: {}", SDL_GetError());
            SDL_DestroyGPUDevice(m_gpu_device);
            m_gpu_device = nullptr;
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
            SDL_Quit();
            return;
        }

        debug::log("SDL_ClaimWindowForGPUDevice");

        SDL_GPUPresentMode present_mode = SDL_GPU_PRESENTMODE_VSYNC;
        if (!graphics_config.vsync_enabled &&
            SDL_WindowSupportsGPUPresentMode(m_gpu_device, m_window, SDL_GPU_PRESENTMODE_MAILBOX)) {
            present_mode = SDL_GPU_PRESENTMODE_MAILBOX;
        }

        SDL_SetGPUSwapchainParameters(m_gpu_device, m_window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, present_mode);

        m_is_initialized = true;
    }

    SdlContext::~SdlContext() {
        if (m_gpu_device && m_window) {
            SDL_ReleaseWindowFromGPUDevice(m_gpu_device, m_window);
            debug::log("SDL_ReleaseWindowFromGPUDevice");
        }

        if (m_gpu_device) {
            SDL_DestroyGPUDevice(m_gpu_device);
            m_gpu_device = nullptr;
            debug::log("SDL_DestroyGPUDevice");
        }

        if (m_window) {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
            debug::log("SDL_DestroyWindow");
        }

        SDL_Quit();
        debug::log("SDL_Quit");
    }

    bool SdlContext::is_initialized() const {
        return m_is_initialized;
    }

    SDL_Window* SdlContext::get_window() const {
        return m_window;
    }

    SDL_GPUDevice* SdlContext::get_gpu_device() const {
        return m_gpu_device;
    }

    Vector2 SdlContext::get_window_size() const {
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(m_window, &width, &height);
        return Vector2(static_cast<float>(width), static_cast<float>(height));
    }

    void SdlContext::get_window_size_px(int& width, int& height) const {
        SDL_GetWindowSizeInPixels(m_window, &width, &height);
    }

    float SdlContext::get_pixel_density() const {
        const float density = SDL_GetWindowPixelDensity(m_window);
        return (density > 0.0f) ? density : 1.0f;
    }
} // namespace hob
