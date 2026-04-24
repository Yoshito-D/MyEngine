#pragma once
#include "../Core/ICinemachineComponent.h"

namespace GameEngine {

/// @brief パーリンノイズベースのカメラシェイク
class PerlinNoise : public ICinemachineComponent {
public:
    PerlinNoise() = default;
    ~PerlinNoise() override = default;

    void MutateCameraState(CameraState& state, float deltaTime) override;
    CinemachineStage GetStage() const override { return CinemachineStage::Noise; }
    const char* GetComponentName() const override { return "PerlinNoise"; }

    /// @brief シェイクを開始
    /// @param amplitude 振幅
    /// @param frequency 周波数
    /// @param duration 持続時間
    void Shake(float amplitude, float frequency, float duration);

    /// @brief 振幅を設定
    void SetAmplitude(float amplitude) { amplitudeGain_ = amplitude; }
    /// @brief 周波数を設定
    void SetFrequency(float frequency) { frequencyGain_ = frequency; }

    /// @brief シェイク中かどうか
    bool IsShaking() const { return remainingTime_ > 0.0f; }

#ifdef USE_IMGUI
    void DrawInspector() override;
#endif

private:
    float Noise(float t) const;

    float amplitudeGain_ = 0.0f;
    float frequencyGain_ = 1.0f;
    float time_ = 0.0f;
    float remainingTime_ = 0.0f;
    float initialAmplitude_ = 0.0f;
};

} // namespace GameEngine
