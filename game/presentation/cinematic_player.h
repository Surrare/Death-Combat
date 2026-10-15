#pragma once

#include "sim/events.h"

namespace dc::presentation {

// Кинематики Fatality, Fatal Blow, интро и побед (docs/ARCHITECTURE.md §7.6).
// TODO(§7.6): таймлайн из assets/cinematics/*.json — треки камеры (glTF), клипы бойцов, события VFX/SFX,
//             расчленение, слоу-мо, скрытие HUD. Длительность сверяется с durationFrames приёма при загрузке.
class CinematicPlayer {
public:
    void onEvent(const sim::GameEvent& /*event*/) {}
};

}  // namespace dc::presentation
