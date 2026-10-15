#pragma once

#include <filesystem>
#include <memory>

namespace dc::audio {

// Звук на miniaudio (docs/ARCHITECTURE.md §7.5). Если звукового устройства нет — игра работает молча.
// TODO(§7.5): шины SFX / Voice / Music, банки звуков из sounds.json, панорама по X, приглушение музыки.
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) = delete;
    AudioEngine& operator=(AudioEngine&&) = delete;

    [[nodiscard]] bool isAvailable() const noexcept;
    void playOneShot(const std::filesystem::path& file);

private:
    struct Impl;  // miniaudio.h большой: прячем его в .cpp
    std::unique_ptr<Impl> impl_;
};

}  // namespace dc::audio
