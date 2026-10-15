#pragma once

#include <spdlog/spdlog.h>

namespace dc::core {

// Лог в консоль и в logs/death-combat.log. Вызвать один раз в начале main.
// Дальше пишем через spdlog::debug / info / warn / error / critical.
void initLogging();

}  // namespace dc::core
