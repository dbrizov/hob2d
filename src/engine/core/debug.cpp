#include "debug.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "engine/components/camera_component.h"
#include "engine/math/constants.h"
#include "systems/renderer/renderer.h"

namespace hob::debug {
    namespace {
        std::vector<DebugLine> lines;
        std::vector<DebugCircle> circles;
        std::vector<DebugMessage> messages;

        float logical_to_window_pixel_ratio(const Vector2& window_size, const Vector2& logical_size) {
            const float ratio_x = (window_size.x > 0.0f) ? (logical_size.x / window_size.x) : 1.0f;
            const float ratio_y = (window_size.y > 0.0f) ? (logical_size.y / window_size.y) : 1.0f;
            // Average the two axes so the result is isotropic when the window's aspect drifts
            // away from the logical aspect.
            return 0.5f * (ratio_x + ratio_y);
        }

        void r_draw_line(Renderer& renderer,
                         const CameraComponent* camera,
                         const Vector2& window_size,
                         const Vector2& logical_size,
                         const DebugLine& line) {
            const Vector2 start = camera->world_to_screen(line.start);
            const Vector2 end = camera->world_to_screen(line.end);
            const float pixel_ratio = logical_to_window_pixel_ratio(window_size, logical_size);
            const float thickness = line.thickness * pixel_ratio;
            renderer.draw_debug_line(start, end, line.color, thickness);
        }

        void r_draw_circle(Renderer& renderer,
                           const CameraComponent* camera,
                           const Vector2& window_size,
                           const Vector2& logical_size,
                           const DebugCircle& circle) {
            Vector2 prev_point = circle.center + Vector2(circle.radius, 0.0f);
            for (int i = 1; i <= circle.segments; ++i) {
                const float ratio = static_cast<float>(i) / static_cast<float>(circle.segments);
                const float angle = ratio * 2.0f * PI;
                const Vector2 point = circle.center + Vector2(std::cos(angle), std::sin(angle)) * circle.radius;
                const DebugLine line{prev_point, point, circle.color, 0.0f, circle.thickness};
                r_draw_line(renderer, camera, window_size, logical_size, line);

                prev_point = point;
            }
        }
    } // namespace

    void flush_draws_to_renderer(Renderer& renderer,
                                 const CameraComponent* camera,
                                 const Vector2& window_size,
                                 float delta_time) {
        const Vector2 logical_size = renderer.get_logical_size();

        for (const auto& line : lines) {
            r_draw_line(renderer, camera, window_size, logical_size, line);
        }

        for (const auto& circle : circles) {
            r_draw_circle(renderer, camera, window_size, logical_size, circle);
        }

        const float scale_factor = logical_to_window_pixel_ratio(window_size, logical_size);
        const float base_line_height = static_cast<float>(renderer.get_debug_font_line_height());
        const float margin_x = MESSAGE_MARGIN_X * scale_factor;
        float pen_y = MESSAGE_MARGIN_Y * scale_factor;
        for (const DebugMessage& msg : messages) {
            renderer.draw_debug_text(Vector2(margin_x, pen_y), msg.text, msg.color, scale_factor);
            pen_y += base_line_height * scale_factor;
        }

        std::erase_if(lines, [delta_time](DebugLine& l) {
            l.duration -= delta_time;
            return l.duration <= 0.0f;
        });

        std::erase_if(circles, [delta_time](DebugCircle& c) {
            c.duration -= delta_time;
            return c.duration <= 0.0f;
        });

        std::erase_if(messages, [delta_time](DebugMessage& m) {
            m.duration -= delta_time;
            return m.duration <= 0.0f;
        });
    }

    void draw_line(const Vector2& start, const Vector2& end, const Color& color, float duration, float thickness) {
        lines.emplace_back(start, end, color, duration, thickness);
    }

    void draw_circle(
        const Vector2& center, float radius, const Color& color, float duration, float thickness, int segments) {
        circles.emplace_back(center, radius, color, duration, thickness, segments);
    }

    namespace detail {
        void add_on_screen_debug_message(std::string text, const Color& color, float duration) {
            if (messages.size() >= MAX_ON_SCREEN_MESSAGES) {
                messages.erase(messages.begin());
            }

            messages.emplace_back(std::move(text), color, duration);
        }
    } // namespace detail
} // namespace hob::debug
