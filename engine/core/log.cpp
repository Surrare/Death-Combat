#include "core/log.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <windows.h>

#include <filesystem>
#include <memory>
#include <vector>

namespace dc::core {

void initLogging() {
    SetConsoleOutputCP(CP_UTF8);  // русские сообщения в консоли без «кракозябр»

    std::filesystem::create_directories("logs");
    std::vector<spdlog::sink_ptr> sinks{
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
        std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/death-combat.log", true),
    };
    auto logger = std::make_shared<spdlog::logger>("dc", sinks.begin(), sinks.end());
    logger->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    logger->flush_on(spdlog::level::info);  // при падении в файле остаётся всё до последней строки
#ifndef NDEBUG
    logger->set_level(spdlog::level::debug);
#endif
    spdlog::set_default_logger(std::move(logger));
}

}  // namespace dc::core
