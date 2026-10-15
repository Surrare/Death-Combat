#include "anim/animation_clip.h"
#include "anim/animation_player.h"
#include "anim/pose.h"
#include "anim/skeleton.h"

#include <algorithm>
#include <cstddef>

namespace dc::anim {

int Skeleton::findJoint(std::string_view name) const noexcept {
    for (std::size_t i = 0; i < joints.size(); ++i) {
        if (joints[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

float AnimationClip::markerTime(std::string_view marker, float fallback) const noexcept {
    for (const ClipMarker& candidate : markers) {
        if (candidate.name == marker) {
            return candidate.time;
        }
    }
    return fallback;
}

float warpMoveTime(int moveFrame, int startupFrames, int totalFrames, float impactTime, float clipDuration) noexcept {
    if (totalFrames <= 0 || clipDuration <= 0.0f) {
        return 0.0f;
    }
    const int frame = std::clamp(moveFrame, 0, totalFrames);
    if (startupFrames <= 0 || frame >= startupFrames) {
        const int tailFrames = std::max(totalFrames - startupFrames, 1);
        const float progress = static_cast<float>(frame - std::max(startupFrames, 0)) / static_cast<float>(tailFrames);
        return impactTime + progress * (clipDuration - impactTime);
    }
    const float progress = static_cast<float>(frame) / static_cast<float>(startupFrames);
    return progress * impactTime;
}

void blendPoses(const Pose& a, const Pose& b, float t, Pose& out) {
    const std::size_t count = std::min(a.local.size(), b.local.size());
    out.local.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        out.local[i].translation = glm::mix(a.local[i].translation, b.local[i].translation, t);
        out.local[i].rotation = glm::slerp(a.local[i].rotation, b.local[i].rotation, t);
        out.local[i].scale = glm::mix(a.local[i].scale, b.local[i].scale, t);
    }
}

void AnimationPlayer::play(const AnimationClip* clip, int blendFrames) noexcept {
    if (clip == current_) {
        return;
    }
    previous_ = current_;
    previousTime_ = time_;
    current_ = clip;
    time_ = 0.0f;
    blendFramesTotal_ = std::max(blendFrames, 0);
    blendFramesLeft_ = blendFramesTotal_;
}

void AnimationPlayer::setTime(float seconds) noexcept {
    time_ = seconds;
}

void AnimationPlayer::tickBlend() noexcept {
    if (blendFramesLeft_ > 0) {
        --blendFramesLeft_;
    }
}

float AnimationPlayer::blendWeight() const noexcept {
    if (blendFramesTotal_ == 0 || previous_ == nullptr) {
        return 1.0f;
    }
    return 1.0f - static_cast<float>(blendFramesLeft_) / static_cast<float>(blendFramesTotal_);
}

}  // namespace dc::anim
