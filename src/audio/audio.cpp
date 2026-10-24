#include "audio/audio.h"

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_ENGINE
#define MA_NO_RESOURCE_MANAGER
#define MINIAUDIO_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
#include <miniaudio/miniaudio.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <vector>

namespace dc {

namespace {
constexpr int RATE = 44100;
constexpr float TAU = 6.2831853f;

struct Noise {
    uint32_t s = 12345;
    float next() { s = s * 1664525u + 1013904223u; return (float)(s >> 8) / 8388608.0f - 1.0f; }
};

using Buffer = std::vector<float>;

// Builds a mono buffer; fn(t, i) returns the raw sample, an envelope is applied by the caller.
template <class F> Buffer make(float seconds, F fn) {
    Buffer b((size_t)(seconds * RATE));
    for (size_t i = 0; i < b.size(); ++i) b[i] = fn((float)i / RATE, i);
    return b;
}
float decay(float t, float rate) { return std::exp(-t * rate); }
float attack(float t, float a) { return std::min(1.0f, t / a); }

Buffer synth(Sfx s) {
    Noise n;
    float lp = 0;   // one-pole low-pass state for noise shaping
    switch (s) {
    case Sfx::UiMove:
        return make(0.05f, [&](float t, size_t) { return std::sin(TAU * 900 * t) * decay(t, 70) * 0.35f; });
    case Sfx::UiConfirm:
        return make(0.16f, [&](float t, size_t) { float f = t < 0.07f ? 660 : 990; return std::sin(TAU * f * t) * decay(t, 18) * 0.4f; });
    case Sfx::UiBack:
        return make(0.12f, [&](float t, size_t) { return std::sin(TAU * 440 * t) * decay(t, 25) * 0.35f; });
    case Sfx::HitLight:
        return make(0.22f, [&](float t, size_t) {
            lp += (n.next() - lp) * 0.5f;
            return (lp * decay(t, 28) * 0.8f + std::sin(TAU * (150 - 400 * t) * t) * decay(t, 22) * 0.7f) * 0.8f; });
    case Sfx::HitHeavy:
        return make(0.45f, [&](float t, size_t) {
            lp += (n.next() - lp) * 0.3f;
            float f = std::max(35.0f, 120 - 220 * t);
            return std::tanh(lp * decay(t, 14) * 1.2f + std::sin(TAU * f * t * 2) * decay(t, 9) * 1.4f) * 0.9f; });
    case Sfx::Block:
        return make(0.3f, [&](float t, size_t) {
            return (std::sin(TAU * 1480 * t) + 0.6f * std::sin(TAU * 2217 * t) + 0.3f * n.next()) * decay(t, 20) * 0.28f; });
    case Sfx::Throw:
        return make(0.35f, [&](float t, size_t) {
            lp += (n.next() - lp) * 0.2f;
            return (lp * decay(t, 16) + std::sin(TAU * 70 * t) * decay(t, 10)) * 0.8f; });
    case Sfx::Whoosh:
        return make(0.28f, [&](float t, size_t) {
            float c = 0.05f + 0.5f * std::sin(3.14159f * t / 0.28f);
            lp += (n.next() - lp) * c;
            return lp * std::sin(3.14159f * t / 0.28f) * 0.6f; });
    case Sfx::Land:
        return make(0.25f, [&](float t, size_t) {
            lp += (n.next() - lp) * 0.15f;
            return (lp * decay(t, 20) + std::sin(TAU * 60 * t) * decay(t, 14)) * 0.7f; });
    case Sfx::Projectile:
        return make(0.45f, [&](float t, size_t) {
            float f = 300 + 900 * t;
            return (std::sin(TAU * f * t) + 0.5f * std::sin(TAU * f * 1.5f * t)) * attack(t, 0.02f) * decay(t, 6) * 0.3f; });
    case Sfx::Clash:
        return make(0.7f, [&](float t, size_t) {
            lp += (n.next() - lp) * 0.6f;
            return std::tanh(lp * decay(t, 6) * 1.5f + std::sin(TAU * 90 * t) * decay(t, 5) * 1.2f + std::sin(TAU * 1760 * t) * decay(t, 9) * 0.3f) * 0.8f; });
    case Sfx::StyleBreak:
        return make(0.9f, [&](float t, size_t) {
            float f = 220 * std::pow(2.0f, 2.0f * t);
            return (std::sin(TAU * f * t) + std::sin(TAU * f * 1.5f * t)) * attack(t, 0.05f) * decay(t, 3.5f) * 0.25f; });
    case Sfx::FlashCancel:
        return make(0.3f, [&](float t, size_t) {
            float f = 1800 - 5000 * t;
            return std::sin(TAU * std::max(200.0f, f) * t) * decay(t, 12) * 0.35f + n.next() * decay(t, 40) * 0.25f; });
    case Sfx::Tech:
        return make(0.1f, [&](float t, size_t) { return (n.next() * 0.4f + std::sin(TAU * 1200 * t)) * decay(t, 50) * 0.35f; });
    case Sfx::HeartHeat:
        return make(1.2f, [&](float t, size_t) {
            float f = 110 * std::pow(2.0f, 1.5f * t);
            lp += (n.next() - lp) * 0.2f;
            return std::tanh((std::sin(TAU * f * t) + std::sin(TAU * f * 1.498f * t)) * attack(t, 0.03f) * decay(t, 2.2f) + lp * decay(t, 6) * 0.6f) * 0.55f; });
    case Sfx::KO:
        return make(1.6f, [&](float t, size_t) {
            lp += (n.next() - lp) * 0.1f;
            float f = std::max(28.0f, 90 - 50 * t);
            return std::tanh(std::sin(TAU * f * t * 1.5f) * decay(t, 2.5f) * 1.6f + lp * decay(t, 5) * 1.4f) * 0.95f; });
    case Sfx::RoundStart:
        return make(1.4f, [&](float t, size_t) {
            float v = 0;
            for (int h = 1; h <= 5; ++h) v += std::sin(TAU * 196 * h * 1.01f * t) / (h * 1.3f);
            return v * attack(t, 0.01f) * decay(t, 2.8f) * 0.35f; });
    case Sfx::Heartbeat:
        return make(0.35f, [&](float t, size_t) {
            float a = std::sin(TAU * 52 * t) * decay(t, 22);
            float t2 = t - 0.16f;
            float b = t2 > 0 ? std::sin(TAU * 46 * t2) * decay(t2, 26) * 0.7f : 0.0f;
            return (a + b) * 1.1f; });
    case Sfx::Engage:
        return make(1.5f, [&](float t, size_t) {
            float v = std::sin(TAU * 82 * t) + 0.7f * std::sin(TAU * 87 * t) + 0.5f * std::sin(TAU * 123 * t);
            lp += (n.next() - lp) * 0.05f;
            return std::tanh((v * 0.7f + lp * 3) * attack(t, 0.02f) * decay(t, 2.0f)) * 0.5f; });
    default: break;
    }
    return {};
}

float osc(float phase) { return std::sin(TAU * phase); }
float saw(float phase) { return 2.0f * (phase - std::floor(phase + 0.5f)); }
float square(float phase) { return phase - std::floor(phase) < 0.5f ? 1.0f : -1.0f; }
float midi(float note) { return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f); }
} // namespace

struct Audio::Impl {
    struct Voice { const Buffer* buf = nullptr; double pos = 0; float vol = 1, pitch = 1; };
    ma_device device;
    bool deviceOpen = false;
    Buffer sounds[(int)Sfx::Count];
    std::mutex mutex;
    std::vector<Voice> voices;
    std::atomic<float> master{1}, music{1}, sfx{1};
    std::atomic<int> mode{0};
    std::atomic<float> intensity{0};

    // music state (touched only from the audio thread)
    float modeGain[4] = {0, 0, 0, 0};
    double musicTime = 0;
    float lpMenu = 0, lpHat = 0;
    Noise noise;
    float hpbPhase = 0;

    float musicSample(float dtS) {
        int target = mode.load();
        for (int m = 0; m < 4; ++m) modeGain[m] += ((m == target ? 1.0f : 0.0f) - modeGain[m]) * dtS * 1.2f;
        double t = musicTime;
        musicTime += dtS;
        float out = 0;

        if (modeGain[1] > 0.001f) {   // MENU: slow minor pad, bass pulse
            static const int roots[4] = {45, 41, 48, 43};      // A, F, C, G
            int bar = (int)(t / 6.0) % 4;
            float r = midi((float)roots[bar]);
            float pad = 0;
            for (int k = 0; k < 3; ++k) {
                float f = r * (k == 0 ? 2.0f : k == 1 ? 2.4f : 3.0f);   // root, minor-ish third, fifth
                float det = 1.0f + 0.003f * (k - 1);
                pad += osc((float)(t * f * det)) * 0.5f + osc((float)(t * f * 2 * det)) * 0.15f;
            }
            float swell = 0.6f + 0.4f * std::sin((float)(t * TAU / 6.0));
            float bass = osc((float)(t * r)) * (0.5f + 0.5f * std::sin((float)(t * TAU * 0.5)));
            out += (pad * 0.11f * swell + bass * 0.18f) * modeGain[1];
        }
        if (modeGain[2] > 0.001f) {   // HUNT: drone, dissonant shimmer, heartbeat that speeds up with the sense level
            float lv = intensity.load();
            float drone = osc((float)(t * 55.0)) * 0.3f + osc((float)(t * 55.7)) * 0.25f + saw((float)(t * 27.5)) * 0.06f;
            float wob = 0.5f + 0.5f * std::sin((float)(t * TAU * (0.1 + 0.2 * lv)));
            float shimmer = osc((float)(t * 440.0 * (1 + 0.5 * wob))) * 0.015f * (0.3f + lv) * wob + osc((float)(t * 466.2)) * 0.012f * lv;
            float bpm = 55 + 90 * lv;
            hpbPhase += dtS * bpm / 60.0f;
            if (hpbPhase >= 1.0f) hpbPhase -= 1.0f;
            float hp = hpbPhase * 60.0f / bpm;   // seconds into the beat
            float beat = osc(52.0f * hp) * decay(hp, 18) + 0.6f * (hp > 0.2f ? osc(46.0f * (hp - 0.2f)) * decay(hp - 0.2f, 20) : 0.0f);
            out += (drone * (0.5f + 0.5f * lv) + shimmer + beat * (0.15f + 0.35f * lv)) * 0.7f * modeGain[2];
        }
        if (modeGain[3] > 0.001f) {   // FIGHT: 140 bpm kick / hat / bass / arpeggio
            const double beatLen = 60.0 / 140.0;
            double bt = t / beatLen;
            double inBeat = (bt - std::floor(bt)) * beatLen;
            float kick = osc((float)(45.0 * inBeat + 4.5 * (1.0 - std::exp(-inBeat * 20.0)))) * decay((float)inBeat, 9) * 0.8f;
            double eighth = std::fmod(t, beatLen * 0.5);
            lpHat += (noise.next() - lpHat) * 0.8f;
            float hat = (noise.next() - lpHat) * decay((float)eighth, 60) * 0.12f * ((int)(bt * 2) % 2 ? 1.0f : 0.6f);
            static const int bassSeq[8] = {33, 33, 36, 33, 31, 31, 34, 31};
            int step = (int)(bt * 2) % 8;
            int bar = (int)(bt / 4) % 4;
            float bf = midi((float)(bassSeq[step] + (bar == 2 ? 3 : bar == 3 ? -2 : 0)));
            float bass = square((float)(t * bf)) * 0.5f * decay((float)eighth, 4) * 0.28f;
            static const int arp[4] = {0, 3, 7, 12};
            float af = midi((float)(57 + arp[(int)(bt * 4) % 4] + (bar == 2 ? 3 : bar == 3 ? -2 : 0)));
            float sixteenth = (float)std::fmod(t, beatLen * 0.25);
            float lead = saw((float)(t * af)) * decay(sixteenth, 12) * 0.07f;
            out += (kick + hat + bass + lead) * 0.8f * modeGain[3];
        }
        return out;
    }

    static void callback(ma_device* dev, void* output, const void*, ma_uint32 frames) {
        Impl* self = (Impl*)dev->pUserData;
        float* out = (float*)output;
        float m = self->master.load(), mu = self->music.load() * 0.5f, sf = self->sfx.load();
        float dtS = 1.0f / RATE;
        std::lock_guard<std::mutex> lock(self->mutex);
        for (ma_uint32 i = 0; i < frames; ++i) {
            float v = self->musicSample(dtS) * mu;
            float sfxSum = 0;
            for (Voice& vc : self->voices) {
                if (!vc.buf) continue;
                size_t p = (size_t)vc.pos;
                if (p + 1 >= vc.buf->size()) { vc.buf = nullptr; continue; }
                float frac = (float)(vc.pos - (double)p);
                sfxSum += ((*vc.buf)[p] * (1 - frac) + (*vc.buf)[p + 1] * frac) * vc.vol;
                vc.pos += vc.pitch;
            }
            float s = std::tanh((v + sfxSum * sf) * m);   // soft limiter
            out[i * 2] = out[i * 2 + 1] = s;
        }
        self->voices.erase(std::remove_if(self->voices.begin(), self->voices.end(), [](const Voice& v) { return !v.buf; }), self->voices.end());
    }
};

Audio::Audio() : impl_(new Impl) {}
Audio::~Audio() {
    if (impl_ && impl_->deviceOpen) ma_device_uninit(&impl_->device);
}

bool Audio::init() {
    for (int i = 0; i < (int)Sfx::Count; ++i) impl_->sounds[i] = synth((Sfx)i);
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.sampleRate = RATE;
    cfg.dataCallback = Impl::callback;
    cfg.pUserData = impl_.get();
    if (ma_device_init(nullptr, &cfg, &impl_->device) != MA_SUCCESS) {
        std::fprintf(stderr, "[audio] no playback device, running silent\n");
        return false;
    }
    impl_->deviceOpen = true;
    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        std::fprintf(stderr, "[audio] failed to start device, running silent\n");
        ma_device_uninit(&impl_->device);
        impl_->deviceOpen = false;
        return false;
    }
    active_ = true;
    return true;
}

void Audio::play(Sfx s, float volume, float pitch) {
    if (!active_) return;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->voices.size() >= 32) return;
    Impl::Voice v; v.buf = &impl_->sounds[(int)s]; v.vol = volume; v.pitch = pitch;
    if (v.buf->empty()) return;
    impl_->voices.push_back(v);
}

void Audio::setMusic(MusicMode m, float intensity) {
    impl_->mode = (int)m;
    impl_->intensity = intensity;
}

void Audio::setVolumes(int master, int music, int sfx) {
    auto curve = [](int v) { float x = (float)std::clamp(v, 0, 10) / 10.0f; return x * x; };
    impl_->master = curve(master); impl_->music = curve(music); impl_->sfx = curve(sfx);
}

} // namespace dc
