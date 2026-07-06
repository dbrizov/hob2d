#pragma once

#include <format>
#include <string_view>

namespace hob::log {
    namespace detail {
        void write_out(std::string_view message);
        void write_err(std::string_view message);
        void write_out(std::string_view tag, std::string_view message);
        void write_err(std::string_view tag, std::string_view message);
    } // namespace detail

    template<typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) {
        detail::write_out(std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) {
        detail::write_err(std::format(fmt, std::forward<Args>(args)...));
    }

    class LogChannel {
        std::string_view m_tag;

    public:
        constexpr explicit LogChannel(std::string_view tag)
            : m_tag(tag) {}

        template<typename... Args>
        void info(std::format_string<Args...> fmt, Args&&... args) const {
            detail::write_out(m_tag, std::format(fmt, std::forward<Args>(args)...));
        }

        template<typename... Args>
        void error(std::format_string<Args...> fmt, Args&&... args) const {
            detail::write_err(m_tag, std::format(fmt, std::forward<Args>(args)...));
        }

        std::string_view tag() const {
            return m_tag;
        }
    };

    // Engine systems
    inline constexpr LogChannel engine{"Engine"};
    inline constexpr LogChannel renderer{"Renderer"};
    inline constexpr LogChannel input{"Input"};
    inline constexpr LogChannel ui{"UI"};
    inline constexpr LogChannel physics{"Physics"};
    inline constexpr LogChannel audio{"Audio"};
    inline constexpr LogChannel lua{"Lua"};

    // Libraries
    inline constexpr LogChannel sdl{"SDL"};
    inline constexpr LogChannel box2d{"Box2D"};
    inline constexpr LogChannel sol2{"sol2"};
    inline constexpr LogChannel rmlui{"RmlUi"};
    inline constexpr LogChannel imgui{"ImGui"};
} // namespace hob::log
