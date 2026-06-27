#pragma once

namespace hob {
    struct GraphicsConfig;

    class Timer {
        friend class Engine;

        uint32_t m_target_fps;
        bool m_vsync_enabled;
        float m_time_scale;
        float m_delta_time;
        float m_game_time;
        float m_real_time;

        // Used for limiting FPS
        uint64_t m_frequency;
        uint64_t m_frame_start_ticks;
        uint64_t m_last_frame_start_ticks;

    public:
        explicit Timer(const GraphicsConfig& graphics_config);

        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;

        Timer(Timer&&) = delete;
        Timer& operator=(Timer&&) = delete;

        uint32_t get_fps() const;
        void set_fps(uint32_t fps);

        float get_time_scale() const;
        void set_time_scale(float time_scale);

        float get_delta_time() const;
        float get_game_time() const;
        float get_real_time() const;

    private:
        void frame_start();
        void frame_end();
    };
} // namespace hob
