#pragma once

#include <RmlUi/Core/Log.h>
#include <RmlUi/Core/SystemInterface.h>

namespace hob {
    class Timer;

    class UiSystemInterface : public Rml::SystemInterface {
        const Timer& m_timer;

    public:
        explicit UiSystemInterface(const Timer& timer);

        double GetElapsedTime() override;
        bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
    };
} // namespace hob
