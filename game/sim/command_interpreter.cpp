#include "sim/command_interpreter.h"

namespace dc::sim {

MoveId findCommand(const InputBuffer& /*buffer*/, const CharacterData& /*character*/,
                   std::int32_t /*facing*/) noexcept {
    // TODO(§4.3 шаг 3, §5): пройти character.moves, разобрать command каждого приёма и проверить буфер
    // с учётом facing (toRelativeDirection) и окон из rules.json. Вернуть индекс приёма с наивысшим приоритетом.
    return MoveId::None;
}

}  // namespace dc::sim
