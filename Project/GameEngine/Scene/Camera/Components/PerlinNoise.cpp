#include "pch.h"
#include "PerlinNoise.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

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

#ifdef USE_IMGUI
void PerlinNoise::DrawInspector() {
    if (ImGui::Checkbox("Enabled", &isEnabled_)) {}

    ImGui::Text("Shaking: %s", IsShaking() ? "Yes" : "No");

    ImGui::Separator();
    ImGui::Text("Test Shake");

    static float testAmplitude = 0.3f;
    static float testFrequency = 10.0f;
    static float testDuration = 0.5f;

    ImGui::DragFloat("Amplitude", &testAmplitude, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Frequency", &testFrequency, 0.1f, 0.1f, 50.0f);
    ImGui::DragFloat("Duration", &testDuration, 0.01f, 0.1f, 5.0f);

    if (ImGui::Button("Trigger Shake")) {
        Shake(testAmplitude, testFrequency, testDuration);
    }
}
#endif

} // namespace GameEngine
