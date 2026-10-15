#include "sim/match_data.h"

namespace dc::sim {

const MoveData* CharacterData::findMove(std::string_view moveId) const noexcept {
    for (const MoveData& move : moves) {
        if (move.id == moveId) {
            return &move;
        }
    }
    return nullptr;
}

}  // namespace dc::sim
