#pragma once
#include "GameEngine/Math/Types/Vector3.h"
#include <cstdint>

namespace GameEngine {
class VirtualCamera;
}

namespace App {
class GravityFollowCamera;
class PlayerRearFollowCamera;
class PlanetLeashCamera;

/// @brief 車両の走行状態に応じたカメラ演出の設定。
struct VehicleCameraEffectsSettings {

   /// @brief 着地時の縦シェイクを有効にするか
   bool enableLandingShake = true;

   /// @brief 着地シェイクの振幅
   float landingShakeAmplitude = 0.15f;

   /// @brief 着地シェイクの周波数
   float landingShakeFrequency = 14.0f;

   /// @brief 着地シェイクの持続時間
   float landingShakeDuration = 0.1f;

   /// @brief ミニターボ発動時の縦・横シェイクを有効にするか
   bool enableMiniTurboShake = true;

   /// @brief ミニターボシェイクの横揺れ振幅
   float miniTurboShakeHorizontalAmplitude = 0.2f;

   /// @brief ミニターボシェイクの縦揺れ振幅
   float miniTurboShakeVerticalAmplitude = 0.15f;

   /// @brief ミニターボシェイクの周波数
   float miniTurboShakeFrequency = 18.0f;

   /// @brief ミニターボシェイクの持続時間（秒）
   float miniTurboShakeDuration = 0.2f;

   /// @brief 横入力中のカメラ接近演出（距離と高さ）を有効にするか
   bool enableDriftCameraApproach = true;

   /// @brief 横入力中に通常のカメラ距離を短縮する割合（0.1で10%）
   float driftCameraDistanceReduction = 0.1f;

   /// @brief 横入力中に通常のカメラ高さへ加える量（重力Up方向）
   float driftCameraHeightOffset = 0.0f;

   /// @brief 横入力中に通常の注視点高さへ加える量（重力Up方向）
   float driftLookTargetHeightOffset = 0.0f;

   /// @brief 横入力の開始・終了時の距離倍率と高さの補間速度
   float driftCameraLerpSpeed = 6.0f;
};

/// @brief ドリフト時の距離補間と、着地・加速シェイクの発火履歴を所有する。
class VehicleCameraEffects {
public:
   /// @brief 補間履歴を初期化し、既存のミニターボを新規イベントとして扱わない。
   void Reset(uint64_t fireCount);

   /// @brief 走行状態を全候補へ入力し、シェイクは選択カメラだけへ適用する。
   void Update(const VehicleCameraEffectsSettings& settings, float deltaTime, bool steering, bool isGrounded,
      uint64_t miniTurboFireCount, const GameEngine::Vector3& gravityUp, GravityFollowCamera* gravityFollowCamera,
      PlayerRearFollowCamera* playerRearFollowCamera, PlanetLeashCamera* planetLeashCamera,
      const GameEngine::VirtualCamera* selectedCamera);

private:
   /// @brief 着地遷移検出用の前フレーム接地状態
   bool wasGrounded_ = true;

   /// @brief カメラが最後に処理したミニターボ発動の連番
   uint64_t lastMiniTurboFireCount_ = 0;

   /// @brief 横入力状態に応じて補間したカメラ距離倍率
   float currentDriftDistanceScale_ = 1.0f;

   /// @brief 横入力状態に応じて補間したカメラ高さの追加量
   float currentDriftCameraHeightOffset_ = 0.0f;

   /// @brief 横入力状態に応じて補間した注視点高さの追加量
   float currentDriftLookTargetHeightOffset_ = 0.0f;
};
}
