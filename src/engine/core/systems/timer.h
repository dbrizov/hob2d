#pragma once

namespace hob {
    struct GraphicsConfig;

    class Timer {
        uint32_t m_target_fps;
        bool m_vsync_enabled;
        float m_time_scale;
        float m_play_time;
        float m_delta_time;

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

        float get_play_time() const;
        void set_play_time(float play_time);

        float get_delta_time() const;

    private:
        friend class Engine;
        void frame_start();
        void frame_end();
    };
} // namespace hob
