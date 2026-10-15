#define MINIAUDIO_IMPLEMENTATION
#include "audio/audio_engine.h"

#include "core/file_io.h"

#include <miniaudio.h>
#include <spdlog/spdlog.h>

namespace dc::audio {

struct AudioEngine::Impl {
    ma_engine engine{};
    bool initialized{false};
};

AudioEngine::AudioEngine() : impl_{std::make_unique<Impl>()} {
    if (ma_engine_init(nullptr, &impl_->engine) == MA_SUCCESS) {
        impl_->initialized = true;
        spdlog::info("Звук: miniaudio готов");
    } else {
        spdlog::warn("Звуковое устройство недоступно — игра без звука");
    }
}

AudioEngine::~AudioEngine() {
    if (impl_->initialized) {
        ma_engine_uninit(&impl_->engine);
    }
}

bool AudioEngine::isAvailable() const noexcept {
    return impl_->initialized;
}

void AudioEngine::playOneShot(const std::filesystem::path& file) {
    if (!impl_->initialized) {
        return;
    }
    const std::string path = core::pathToUtf8(file);
    if (ma_engine_play_sound(&impl_->engine, path.c_str(), nullptr) != MA_SUCCESS) {
        spdlog::warn("Не удалось проиграть {}", path);
    }
}

}  // namespace dc::audio
