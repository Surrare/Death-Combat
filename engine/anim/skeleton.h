#pragma once

#include <glm/glm.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace dc::anim {

inline constexpr int maxJoints = 128;  // размер палитры костей в SSBO (docs/ARCHITECTURE.md §7.2)

struct Joint {
    std::string name;
    int parent{-1};  // -1 — корень; родитель всегда стоит в массиве раньше ребёнка
    glm::mat4 inverseBindMatrix{1.0f};
};

struct Skeleton {
    std::vector<Joint> joints;

    [[nodiscard]] int findJoint(std::string_view name) const noexcept;  // -1, если кости нет
};

}  // namespace dc::anim
