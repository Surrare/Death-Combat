#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace dc::anim {

struct JointTransform {
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

// Ключи одной кости; времена в секундах.
struct JointTrack {
    int joint{-1};
    std::vector<float> translationTimes;
    std::vector<glm::vec3> translations;
    std::vector<float> rotationTimes;
    std::vector<glm::quat> rotations;
    std::vector<float> scaleTimes;
    std::vector<glm::vec3> scales;
};

// Именованная метка клипа. Главная — "impact": момент контакта удара (docs/ARCHITECTURE.md §7.2).
struct ClipMarker {
    std::string name;
    float time{0.0f};
};

struct AnimationClip {
    std::string name;
    float duration{0.0f};
    std::vector<JointTrack> tracks;
    std::vector<ClipMarker> markers;

    [[nodiscard]] float markerTime(std::string_view marker, float fallback) const noexcept;
};

// Время клипа для кадра приёма (time-warp, §7.2): кадры 0..startup ложатся на 0..impact,
// кадры startup..total — на impact..конец клипа. Так визуальный контакт всегда совпадает
// с первым активным кадром, даже если клип из Mixamo длится иначе.
[[nodiscard]] float warpMoveTime(int moveFrame, int startupFrames, int totalFrames, float impactTime,
                                 float clipDuration) noexcept;

}  // namespace dc::anim
