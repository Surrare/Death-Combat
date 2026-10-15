// Death Combat — точка входа. Архитектура: docs/ARCHITECTURE.md.
#include "app/app.h"
#include "core/errors.h"
#include "core/log.h"
#include "platform/message_box.h"

#include <exception>
#include <string>

int main() {
    dc::core::initLogging();
    try {
        dc::game::App app;
        app.run();
    } catch (const dc::core::Error& error) {
        spdlog::critical("{}", error.what());
        dc::platform::showFatalError(error.what());
        return 1;
    } catch (const std::exception& error) {
        spdlog::critical("Непредвиденная ошибка: {}", error.what());
        dc::platform::showFatalError(std::string{"Непредвиденная ошибка: "} + error.what());
        return 1;
    }
    return 0;
}
