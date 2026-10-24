// Cross-platform audio (miniaudio device backend). All sounds and music are synthesised in code,
// so there are no asset files. If no audio device is available the game simply runs silent.
#pragma once
#include <atomic>
#include <memory>

namespace dc {

enum class Sfx {
    UiMove, UiConfirm, UiBack,
    HitLight, HitHeavy, Block, Throw, Whoosh, Land, Projectile, Clash, StyleBreak, FlashCancel, Tech,
    HeartHeat, KO, RoundStart, Heartbeat, Engage,
    Count
};

enum class MusicMode { Off, Menu, Hunt, Fight };

class Audio {
public:
    Audio();
    ~Audio();
    bool init();                                   // false = silent mode
    void play(Sfx s, float volume = 1.0f, float pitch = 1.0f);
    void setMusic(MusicMode m, float intensity = 0.0f);   // intensity 0..1 (hunt: death sense, fight: unused)
    void setVolumes(int master, int music, int sfx);      // each 0..10
    bool active() const { return active_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool active_ = false;
};

} // namespace dc
