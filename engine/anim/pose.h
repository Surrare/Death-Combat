#pragma once

#include "anim/animation_clip.h"
#include "anim/skeleton.h"

#include <vector>

namespace dc::anim {

// Локальные трансформации всех костей в один момент времени.
struct Pose {
    std::vector<JointTransform> local;
};

// Смешивание двух поз: lerp для позиции и масштаба, slerp для вращения. t = 0 → a, t = 1 → b.
void blendPoses(const Pose& a, const Pose& b, float t, Pose& out);

// TODO(§7.2): выборка ключей клипа в момент time (бинарный поиск по временам + интерполяция).
void sampleClip(const AnimationClip& clip, float time, Pose& out);

// TODO(§7.2): поза → матрицы скин-палитры (world * inverseBind) для SSBO, не больше maxJoints.
void computeSkinningMatrices(const Skeleton& skeleton, const Pose& pose, std::vector<glm::mat4>& out);

}  // namespace dc::anim
