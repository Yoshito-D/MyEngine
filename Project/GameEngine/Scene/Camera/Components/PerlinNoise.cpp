#include "pch.h"
#include "PerlinNoise.h"
#include <cmath>
#include <algorithm>

namespace GameEngine {

void PerlinNoise::MutateCameraState(CameraState& state, float deltaTime) {
    if (remainingTime_ <= 0.0f && amplitudeGain_ <= 0.0f) return;

    time_ += deltaTime * frequencyGain_;

    float currentAmplitude = amplitudeGain_;

    // 持続時間がある場合、減衰
    if (remainingTime_ > 0.0f) {
        remainingTime_ -= deltaTime;
        float ratio = std::max(0.0f, remainingTime_) / (remainingTime_ + deltaTime);
        currentAmplitude = initialAmplitude_ * ratio;

        if (remainingTime_ <= 0.0f) {
            remainingTime_ = 0.0f;
            amplitudeGain_ = 0.0f;
        }
    }

    // ノイズを位置に適用
    Vector3 shake = {
        Noise(time_ * 1.0f) * currentAmplitude,
        Noise(time_ * 1.3f + 100.0f) * currentAmplitude,
        Noise(time_ * 0.7f + 200.0f) * currentAmplitude
    };

    state.transform.translation = state.transform.translation + shake;
}

void PerlinNoise::Shake(float amplitude, float frequency, float duration) {
    initialAmplitude_ = amplitude;
    amplitudeGain_ = amplitude;
    frequencyGain_ = frequency;
    remainingTime_ = duration;
}

float PerlinNoise::Noise(float t) const {
    // 簡易的なサイン波ベースのノイズ
    constexpr float kPi2 = 6.28318f;
    return std::sin(t * kPi2) * 0.5f +
           std::sin(t * kPi2 * 2.0f + 1.0f) * 0.25f +
           std::sin(t * kPi2 * 4.0f + 2.0f) * 0.125f;
}

} // namespace GameEngine
