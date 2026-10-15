#pragma once

#include "anim/animation_clip.h"
#include "anim/pose.h"

namespace dc::anim {

// Проигрывает клипы одного бойца: текущий + кроссфейд из предыдущего (docs/ARCHITECTURE.md §7.2).
// Время задаёт симуляция (кадр приёма), а не часы — поэтому здесь нет update(dt).
class AnimationPlayer {
public:
    // Клип не принадлежит плееру: им владеет AssetManager. blendFrames = 0 — переключение без смешивания.
    void play(const AnimationClip* clip, int blendFrames) noexcept;
    void setTime(float seconds) noexcept;
    void tickBlend() noexcept;  // раз за тик симуляции

    [[nodiscard]] const AnimationClip* current() const noexcept { return current_; }
    [[nodiscard]] float blendWeight() const noexcept;  // 1 — только текущий клип

    // TODO(§7.2): sampleClip(previous) + sampleClip(current) + blendPoses по blendWeight().
    void evaluate(const Skeleton& skeleton, Pose& out) const;

private:
    const AnimationClip* current_{nullptr};
    const AnimationClip* previous_{nullptr};
    float time_{0.0f};
    float previousTime_{0.0f};
    int blendFramesTotal_{0};
    int blendFramesLeft_{0};
};

}  // namespace dc::anim
