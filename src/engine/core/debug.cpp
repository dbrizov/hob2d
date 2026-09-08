#include "debug.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include "engine/components/camera_component.h"
#include "engine/components/camera_component_3d.h"
#include "engine/math/constants.h"
#include "engine/math/quaternion.h"
#include "systems/renderer/renderer.h"

namespace hob::debug {
    namespace {
        std::vector<DebugLine> lines;
        std::vector<DebugCircle> circles;
        std::vector<DebugLine3D> lines_3d;
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
            for (int32_t i = 1; i <= circle.segments; ++i) {
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
                                 const CameraComponent3D* camera_3d,
                                 const Vector2& window_size,
                                 float delta_time) {
        const Vector2 logical_size = renderer.get_logical_size();

        if (camera != nullptr) {
            for (const auto& line : lines) {
                r_draw_line(renderer, camera, window_size, logical_size, line);
            }

            for (const auto& circle : circles) {
                r_draw_circle(renderer, camera, window_size, logical_size, circle);
            }
        }

        if (camera_3d != nullptr) {
            const float pixel_ratio = logical_to_window_pixel_ratio(window_size, logical_size);
            for (const auto& line : lines_3d) {
                Vector2 start;
                Vector2 end;
                if (camera_3d->project_segment(line.start, line.end, start, end)) {
                    renderer.draw_debug_line(start, end, line.color, line.thickness * pixel_ratio);
                }
            }
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

        std::erase_if(lines_3d, [delta_time](DebugLine3D& l) {
            l.duration -= delta_time;
            return l.duration <= 0.0f;
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
        const Vector2& center, float radius, const Color& color, float duration, float thickness, int32_t segments) {
        circles.emplace_back(center, radius, color, duration, thickness, segments);
    }

    void draw_line_3d(const Vector3& start, const Vector3& end, const Color& color, float duration, float thickness) {
        lines_3d.emplace_back(start, end, color, duration, thickness);
    }

    void draw_aabb3(const AABB3& box, const Matrix4x4& world, const Color& color, float duration, float thickness) {
        std::array<Vector3, 8> corners;
        box.get_corners(corners);
        for (Vector3& corner : corners) {
            corner = world.transform_point(corner);
        }

        constexpr std::pair<int32_t, int32_t> edges[12] = {
            {0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7}, {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& [a, b] : edges) {
            lines_3d.emplace_back(corners[a], corners[b], color, duration, thickness);
        }
    }

    namespace {
        void draw_ring_3d(const Vector3& center,
                          const Vector3& axis_u,
                          const Vector3& axis_v,
                          float radius,
                          const Color& color,
                          float duration,
                          float thickness,
                          int32_t segments) {
            Vector3 prev_point = center + axis_u * radius;
            for (int32_t i = 1; i <= segments; ++i) {
                const float angle = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * PI;
                const Vector3 point = center + (axis_u * std::cos(angle) + axis_v * std::sin(angle)) * radius;
                lines_3d.emplace_back(prev_point, point, color, duration, thickness);
                prev_point = point;
            }
        }
    } // namespace

    void draw_sphere_3d(
        const Vector3& center, float radius, const Color& color, float duration, float thickness, int32_t segments) {
        draw_ring_3d(center, Vector3::right(), Vector3::up(), radius, color, duration, thickness, segments);
        draw_ring_3d(center, Vector3::right(), Vector3::forward(), radius, color, duration, thickness, segments);
        draw_ring_3d(center, Vector3::up(), Vector3::forward(), radius, color, duration, thickness, segments);
    }

    void draw_capsule_3d(const Vector3& center_a,
                         const Vector3& center_b,
                         float radius,
                         const Color& color,
                         float duration,
                         float thickness,
                         int32_t segments) {
        const Vector3 axis = (center_b - center_a).normalized();
        const Quaternion rotation =
            axis == Vector3::zero() ? Quaternion::identity() : Quaternion::from_to_rotation(Vector3::up(), axis);
        const Vector3 u = rotation.rotate(Vector3::right());
        const Vector3 v = rotation.rotate(Vector3::forward());

        draw_sphere_3d(center_a, radius, color, duration, thickness, segments);
        draw_sphere_3d(center_b, radius, color, duration, thickness, segments);
        draw_ring_3d(center_a, u, v, radius, color, duration, thickness, segments);
        draw_ring_3d(center_b, u, v, radius, color, duration, thickness, segments);

        const Vector3 offsets[4] = {u * radius, u * -radius, v * radius, v * -radius};
        for (const Vector3& offset : offsets) {
            lines_3d.emplace_back(center_a + offset, center_b + offset, color, duration, thickness);
        }
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
