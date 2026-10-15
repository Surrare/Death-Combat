#pragma once

#include "audio/audio_engine.h"
#include "sim/events.h"

namespace dc::presentation {

// Звук боя (docs/ARCHITECTURE.md §7.5): переводит события симуляции в звуки.
// TODO(§7.5): HitEvent → слои «щелчок + тело + хвост» по силе удара; анонсер на RoundStarted / FightStarted /
//             FinishHim; музыкальные состояния (бой, акцент Finish Him, победа).
class FightAudio {
public:
    explicit FightAudio(audio::AudioEngine& engine) noexcept : engine_{&engine} {}

    void onEvent(const sim::GameEvent& /*event*/) {}

private:
    audio::AudioEngine* engine_;  // не владеет: движок звука живёт в App
};

}  // namespace dc::presentation
