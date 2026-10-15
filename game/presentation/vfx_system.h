#pragma once

#include "sim/events.h"

namespace dc::presentation {

// Визуальные эффекты (docs/ARCHITECTURE.md §7.4).
// TODO(§7.4): GPU-частицы на compute (кровь, искры удара и блока, пыль, угли), кровь на полу через маску R8,
//             вспышка удара, импульс хроматической аберрации; настройка «Кровь» заменяет кровь искрами.
class VfxSystem {
public:
    void onEvent(const sim::GameEvent& /*event*/) {}
};

}  // namespace dc::presentation
