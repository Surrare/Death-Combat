#pragma once

#include "sim/match_data.h"
#include "sim/match_state.h"

#include <string_view>

namespace dc::presentation {

// Визуальный двойник бойца (docs/ARCHITECTURE.md §7.1): модель, AnimationPlayer, материал
// (вспышка удара, кровь), расчленение. Читает FighterState и никогда не пишет в симуляцию.
class FighterView {
public:
    // Какой клип показывать для текущего действия. Для приёма — поле animation из moves.json.
    [[nodiscard]] static std::string_view clipFor(const sim::FighterState& fighter,
                                                  const sim::CharacterData& character) noexcept;

    // TODO(§7.2): модель из AssetManager, AnimationPlayer с кроссфейдом, time-warp по маркеру impact,
    //             отражение модели по facing, скин-палитра в SSBO, вспышка удара и кровь в материале.
    void update(const sim::FighterState& fighter, const sim::CharacterData& character) noexcept;

    [[nodiscard]] std::string_view currentClip() const noexcept { return currentClip_; }

private:
    std::string_view currentClip_{"idle"};
};

}  // namespace dc::presentation
