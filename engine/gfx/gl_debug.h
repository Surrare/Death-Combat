#pragma once

namespace dc::gfx {

// Включает GL debug output, если контекст создан с флагом отладки (Debug-сборка, docs/ARCHITECTURE.md §8.1).
// Сообщения HIGH останавливают отладчик, MEDIUM пишутся в лог как предупреждения.
void installDebugOutput();

}  // namespace dc::gfx
