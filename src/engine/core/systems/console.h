#pragma once

#include <format>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <imgui.h>

namespace hob {
    using CommandArgs = std::span<const std::string>;
    using CommandFunc = std::function<void(CommandArgs)>;

    enum class ConsoleVariableType {
        Bool,
        Int,
        Float,
        String
    };

    enum ConsoleVariableFlags : uint32_t {
        None = 0,
        Archive = 1 << 0,
        ReadOnly = 1 << 1,
        Cheat = 1 << 2,
    };

    template<typename T>
    std::string to_cvar_string(const T& value) {
        if constexpr (std::is_same_v<T, bool>) {
            return value ? "1" : "0";
        }
        else if constexpr (std::is_integral_v<T>) {
            return std::format("{}", value);
        }
        else if constexpr (std::is_floating_point_v<T>) {
            std::string s = std::format("{}", value);
            const bool is_integer_like = s.find_first_not_of("-0123456789") == std::string::npos;
            if (is_integer_like) {
                s += ".0";
            }
            return s;
        }
        else if constexpr (std::is_convertible_v<T, std::string_view>) {
            return std::string(std::string_view(value));
        }
        else {
            return std::format("{}", value);
        }
    }

    struct ConsoleCommand {
        std::string name;
        std::string help;
        CommandFunc func;

        std::string to_string(uint32_t indent = 0) const;
    };

    struct ConsoleVariable {
        std::string name;
        std::string help;
        ConsoleVariableType type = ConsoleVariableType::String;
        uint32_t flags = None;

        std::string value;
        std::string default_value;

        std::function<void(const ConsoleVariable&)> on_changed;

        bool bool_value() const;
        int int_value() const;
        float float_value() const;

        std::string to_string_short(uint32_t indent = 0) const;
        std::string to_string_long(uint32_t name_width = 0, uint32_t value_width = 0, uint32_t default_width = 0) const;
    };

    class ConsoleBackend {
        std::unordered_map<std::string, ConsoleCommand> m_commands;
        std::unordered_map<std::string, ConsoleVariable> m_cvars;

    public:
        // Logging sink (frontend sets this)
        std::function<void(std::string_view)> print;
        std::function<void(std::string_view)> print_error;

        ConsoleBackend();

        bool register_command(std::string_view name, std::string_view help, CommandFunc func);
        bool register_cvar(std::string_view name,
                           std::string_view help,
                           std::string_view default_value,
                           ConsoleVariableType type,
                           ConsoleVariableFlags flags = None,
                           std::function<void(const ConsoleVariable&)> on_changed = {});

        const ConsoleCommand* find_command(std::string_view name) const;
        const ConsoleVariable* find_cvar(std::string_view name) const;

        std::vector<std::string_view> get_candidates_for_prefix(std::string_view prefix);

        void execute_line(std::string_view line);

    private:
        void execute_command(const ConsoleCommand& command, CommandArgs args);
        void execute_cvar(ConsoleVariable& cvar, CommandArgs args);

        static std::string key_of(std::string_view s);
        static std::vector<std::string> tokenize(std::string_view line);

        void cmd_help() const;
        void cmd_cmdlist() const;
        void cmd_cvarlist() const;
    };

    class Console {
        static constexpr ImColor LOG_ENTRY_COLOR_WHITE = ImColor(1.0f, 1.0f, 1.0f, 1.0f);
        static constexpr ImColor LOG_ENTRY_COLOR_GRAY = ImColor(0.65f, 0.65f, 0.65f, 1.0f);
        static constexpr ImColor LOG_ENTRY_COLOR_RED = ImColor(1.0f, 0.4f, 0.4f, 1.0f);
        static constexpr ImColor LOG_ENTRY_COLOR_GREEN = ImColor(0.55f, 0.85f, 0.60f, 1.0f);
        static constexpr ImColor LOG_ENTRY_COLOR_BLUE = ImColor(0.55f, 0.70f, 0.95f, 1.0f);
        static constexpr ImColor LOG_ENTRY_COLOR_ORANGE = ImColor(1.0f, 0.78f, 0.58f, 1.0f);
        static constexpr size_t INPUT_BUFFER_SIZE = 256;

        bool m_is_open = false;
        char m_input_buffer[INPUT_BUFFER_SIZE] = {};
        std::vector<std::string> m_log;
        bool m_scroll_to_bottom = true;
        std::vector<std::string> m_history;
        int m_history_index = -1; // -1: new line, [0..history-1] browsing history.

        ConsoleBackend m_backend;

    public:
        Console();

        Console(const Console&) = delete;
        Console& operator=(const Console&) = delete;

        Console(Console&&) = delete;
        Console& operator=(Console&&) = delete;

        bool is_open() const;
        void toggle_open();

        void clear_input_buffer();
        void clear_log();

        template<typename... Args>
        void log(std::format_string<Args...> fmt, Args&&... args) {
            m_log.emplace_back(std::format(fmt, std::forward<Args>(args)...));
            m_scroll_to_bottom = true;
        }

        template<typename... Args>
        void log_error(std::format_string<Args...> fmt, Args&&... args) {
            log("[error] {}", std::format(fmt, std::forward<Args>(args)...));
        }

        void draw();

        bool register_command(std::string_view name, std::string_view help, CommandFunc func);
        bool register_cvar(std::string_view name,
                           std::string_view help,
                           std::string_view default_value,
                           ConsoleVariableType type,
                           ConsoleVariableFlags flags = None,
                           std::function<void(const ConsoleVariable&)> on_changed = {});

        const ConsoleCommand* find_command(std::string_view name) const;
        const ConsoleVariable* find_cvar(std::string_view name) const;

    private:
        void execute_line(std::string_view line);

        static int text_edit_callback_stub(ImGuiInputTextCallbackData* data);
        int text_edit_callback(ImGuiInputTextCallbackData* data);
    };
} // namespace hob
