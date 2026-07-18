#include "timer.h"

#include <SDL3/SDL_timer.h>

#include "engine/core/engine_config.h"
#include "engine/core/logging.h"

namespace hob {
    Timer::Timer(const GraphicsConfig& graphics_config)
        : m_target_fps(graphics_config.target_fps)
        , m_vsync_enabled(graphics_config.vsync_enabled)
        , m_time_scale(1.0f)
        , m_delta_time(0.0f)
        , m_game_time(0.0f)
        , m_real_time(0.0f)
        , m_frequency(0)
        , m_frame_start_ticks(0)
        , m_last_frame_start_ticks(0) {
        m_frequency = SDL_GetPerformanceFrequency();
        m_frame_start_ticks = SDL_GetPerformanceCounter();
        m_last_frame_start_ticks = SDL_GetPerformanceCounter();

        log::engine.info("Timer::Initialise ({} fps, vsync {})", m_target_fps, m_vsync_enabled ? "on" : "off");
    }

    Timer::~Timer() {
        log::engine.info("Timer::Shutdown");
    }

    uint32_t Timer::get_fps() const {
        return m_target_fps;
    }

    void Timer::set_fps(uint32_t fps) {
        m_target_fps = fps;
    }

    float Timer::get_time_scale() const {
        return m_time_scale;
    }

    void Timer::set_time_scale(float time_scale) {
        m_time_scale = time_scale;
    }

    float Timer::get_delta_time() const {
        return m_delta_time;
    }

    float Timer::get_game_time() const {
        return m_game_time;
    }

    float Timer::get_real_time() const {
        return m_real_time;
    }

    void Timer::frame_start() {
        const uint64_t now_ticks = SDL_GetPerformanceCounter();
        const uint64_t diff_ticks = now_ticks - m_last_frame_start_ticks;

        m_frame_start_ticks = now_ticks; // Remember (for frame_end)
        m_last_frame_start_ticks = now_ticks; // Remember (for next frame_start)

        double dt_seconds = static_cast<double>(diff_ticks) / static_cast<double>(m_frequency);

        // After stalls (debugger, window focus loss, OS scheduling),
        // dt can become very large. Clamp it to avoid excessive physics
        // catch-up and the "spiral of death".
        if (dt_seconds > 0.25) {
            dt_seconds = 0.25;
        }

        m_delta_time = static_cast<float>(dt_seconds);
        m_game_time += m_delta_time * m_time_scale;
        m_real_time += m_delta_time;
    }

    void Timer::frame_end() {
        if (m_vsync_enabled || m_target_fps == 0) {
            return;
        }

        const double target_frame_seconds = 1.0 / static_cast<double>(m_target_fps);

        while (true) {
            const uint64_t now_ticks = SDL_GetPerformanceCounter();
            const double elapsed_seconds =
                static_cast<double>(now_ticks - m_frame_start_ticks) / static_cast<double>(m_frequency);

            if (elapsed_seconds >= target_frame_seconds) {
                break;
            }

            // Sleep most of the remaining time (avoid oversleep, because SDL_Delay tends to oversleep)
            const double remaining_seconds = target_frame_seconds - elapsed_seconds;
            if (remaining_seconds > 0.002 /* ~2ms */) {
                // We subtract 1ms from the remaining time to prevent oversleep.
                // 1ms is usually larger than the OS wake-up jitter.
                // It gives us a safety margin so we don’t overshoot.
                const uint32_t milliseconds = static_cast<uint32_t>((remaining_seconds - 0.001) * 1000.0);
                SDL_Delay(milliseconds);
            }
        }
    }
} // namespace hob
