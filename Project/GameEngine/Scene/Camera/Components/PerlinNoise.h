#pragma once

#include "GameEngine/Scene/Camera/Core/ICinemachineComponent.h"
#include "GameEngine/Math/Types/Vector3.h"

namespace GameEngine {

/// @brief パーリンノイズベースのカメラシェイク
class PerlinNoise : public ICinemachineComponent {
public:
   /// @brief 停止状態のノイズコンポーネントを生成する
   PerlinNoise() = default;

   /// @brief カメラコンポーネントを破棄する
   ~PerlinNoise() override = default;

   /// @copydoc ICinemachineComponent::MutateCameraState
   void MutateCameraState(CameraState& state, float deltaTime) override;

   /// @copydoc ICinemachineComponent::GetStage
   CinemachineStage GetStage() const override { return CinemachineStage::Noise; }

   /// @copydoc ICinemachineComponent::GetComponentName
   const char* GetComponentName() const override { return "PerlinNoise"; }

   /// @brief シェイクを開始
   /// @param amplitude 振幅
   /// @param frequency 周波数
   /// @param duration 持続時間
   void Shake(float amplitude, float frequency, float duration);

   /// @brief 指定方向だけにシェイクを開始
   /// @param direction シェイク方向
   /// @param amplitude 振幅
   /// @param frequency 周波数
   /// @param duration 持続時間
   void ShakeDirectional(const Vector3& direction, float amplitude, float frequency, float duration);

   /// @brief 更新時のカメラの縦・横方向へ独立したシェイクを開始する
   /// @param horizontalAmplitude 横揺れの振幅
   /// @param verticalAmplitude 縦揺れの振幅
   /// @param frequency 周波数
   /// @param duration 持続時間（0以下なら停止）
   void ShakeScreenSpace(float horizontalAmplitude, float verticalAmplitude, float frequency, float duration);

   /// @brief 振幅を設定
   void SetAmplitude(float amplitude) { amplitudeGain_ = amplitude; }

   /// @brief 周波数を設定
   void SetFrequency(float frequency) { frequencyGain_ = frequency; }

   /// @brief シェイク中かどうか
   bool IsShaking() const { return remainingTime_ > 0.0f; }

   /// @copydoc ICinemachineComponent::Serialize
   nlohmann::json Serialize() const override;

   /// @copydoc ICinemachineComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @copydoc ICinemachineComponent::DrawInspector
   void DrawInspector() override;
#endif

private:
   float Noise(float t) const;

   float amplitudeGain_ = 0.0f;                  ///< カメラ揺れに掛ける振幅倍率。
   float frequencyGain_ = 1.0f;                  ///< カメラ揺れの時間周波数へ掛ける倍率。
   float time_ = 0.0f;                           ///< クリップ先頭からの時刻（秒）。
   float remainingTime_ = 0.0f;                  ///< 処理が終了するまでの残り時間（秒）。
   float initialAmplitude_ = 0.0f;               ///< 減衰開始時のカメラ揺れの振幅。
   Vector3 shakeDirection_ = {0.0f, 1.0f, 0.0f}; ///< 方向指定のカメラ揺れに使うワールド方向。
   bool useDirectionalShake_ = false;            ///< 揺れを指定方向に制限するかどうか。
   Vector2 screenSpaceWeights_ = {};             ///< 画面の横・縦方向それぞれに加える揺れの重み。
   bool useScreenSpaceShake_ = false;            ///< 画面基準の横・縦方向へ揺れを適用するかどうか。
};

} // namespace GameEngine
