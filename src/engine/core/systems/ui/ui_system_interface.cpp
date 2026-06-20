#include "ui_system_interface.h"

#include "engine/core/debug.h"
#include "engine/core/systems/timer.h"

namespace hob {
    UiSystemInterface::UiSystemInterface(const Timer& timer)
        : m_timer(timer) {}

    double UiSystemInterface::GetElapsedTime() {
        return m_timer.get_play_time();
    }

    bool UiSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message) {
        if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT) {
            debug::log_error("[RmlUi] {}", message);
        }
        else {
            debug::log("[RmlUi] {}", message);
        }

        return true;
    }

    void UiSystemInterface::JoinPath(Rml::String& translated_path,
                                     const Rml::String& document_path,
                                     const Rml::String& path) {
        translated_path = path;
    }
} // namespace hob
