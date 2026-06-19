#pragma once

#include <format>
#include <iostream>
#include <string>

#include "engine/math/color.h"
#include "engine/math/vector2.h"

namespace hob {
    class CameraComponent;
    class Renderer;

    namespace debug {
        constexpr Color DEFAULT_DRAW_COLOR = Color::white();
        constexpr float DEFAULT_DRAW_DURATION = 0.0f;
        constexpr float DEFAULT_LINE_THICKNESS = 1.0f;
        constexpr int DEFAULT_CIRCLE_SEGMENTS = 16;

        constexpr Color DEFAULT_MESSAGE_COLOR = Color::white();
        constexpr float DEFAULT_MESSAGE_DURATION = 5.0f;
        constexpr float MESSAGE_MARGIN_X = 8.0f;
        constexpr float MESSAGE_MARGIN_Y = 8.0f;
        constexpr float MESSAGE_FADE_DURATION = 0.3f;
        constexpr uint32_t MAX_ON_SCREEN_MESSAGES = 32;

        // Console logging
        template<typename... Args>
        void log(std::format_string<Args...> fmt, Args&&... args) {
            std::cout << std::format(fmt, std::forward<Args>(args)...) << std::endl;
        }

        template<typename... Args>
        void log_error(std::format_string<Args...> fmt, Args&&... args) {
            std::cerr << std::format(fmt, std::forward<Args>(args)...) << std::endl;
        }

        // Debug primitives
        struct DebugLine {
            Vector2 start;
            Vector2 end;
            Color color = DEFAULT_DRAW_COLOR;
            float duration = DEFAULT_DRAW_DURATION;
            float thickness = DEFAULT_LINE_THICKNESS;
        };

        struct DebugCircle {
            Vector2 center;
            float radius = 0.5f;
            Color color = DEFAULT_DRAW_COLOR;
            float duration = DEFAULT_DRAW_DURATION;
            float thickness = DEFAULT_LINE_THICKNESS;
            int segments = DEFAULT_CIRCLE_SEGMENTS;
        };

        struct DebugMessage {
            std::string text;
            Color color = DEFAULT_MESSAGE_COLOR;
            float duration = DEFAULT_MESSAGE_DURATION;
        };

        void flush_draws_to_renderer(Renderer& renderer,
                                     const CameraComponent* camera,
                                     const Vector2& window_size,
                                     float delta_time);

        void draw_line(const Vector2& start,
                       const Vector2& end,
                       const Color& color = DEFAULT_DRAW_COLOR,
                       float duration = DEFAULT_DRAW_DURATION,
                       float thickness = DEFAULT_LINE_THICKNESS);

        void draw_circle(const Vector2& center,
                         float radius,
                         const Color& color = DEFAULT_DRAW_COLOR,
                         float duration = DEFAULT_DRAW_DURATION,
                         float thickness = DEFAULT_LINE_THICKNESS,
                         int segments = DEFAULT_CIRCLE_SEGMENTS);

        // On-screen messages
        namespace detail {
            void add_on_screen_debug_message(std::string text, const Color& color, float duration);

            template<typename... Args>
            void print_dispatch(const Color& color, float duration, std::format_string<Args...> fmt, Args&&... args) {
                std::string text = std::format(fmt, std::forward<Args>(args)...);
                log("{}", text);
                add_on_screen_debug_message(std::move(text), color, duration);
            }
        } // namespace detail

        template<typename... Args>
        void print(std::format_string<Args...> fmt, Args&&... args) {
            detail::print_dispatch(DEFAULT_MESSAGE_COLOR, DEFAULT_MESSAGE_DURATION, fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void print(const Color& color, float duration, std::format_string<Args...> fmt, Args&&... args) {
            detail::print_dispatch(color, duration, fmt, std::forward<Args>(args)...);
        }
    } // namespace debug
} // namespace hob
