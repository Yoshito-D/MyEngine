#pragma once

#include <cstdint>
#include "GameProject/App/Component/Camera/RearFollow/RearCameraTypes.h"

namespace App {

/// @brief プレイヤー後方と速度後方を追従し、着地後の方向復帰を管理する。
class RearCameraDirectionTracker {
public:
   /// @brief Upのデルタ回転を後方にも適用し、惑星切替時のロール反転を防ぐ。
   void TransportWithGravity(const GameEngine::Vector3& oldUp, const GameEngine::Vector3& newUp);

   /// @brief currentBackward を更新する
   /// @details 地上では重力平面上のプレイヤー後方、空中ではプレイヤー速度の反対方向へ追従する。
   /// @param up 正規化済み補間済み重力Up
   /// @param deltaTime フレーム時間
   void UpdateBackwardVector(const RearCameraInput& input, const RearCameraSettings& settings,
      const RearCameraTransitionState& transition, const RearCameraAimState& aim, const RearCameraPlanetState& planet,
      float planetBlend, const GameEngine::Vector3& up, float deltaTime);

   /// @brief 接地時に直前の空中追従速度を地上復帰の始点にする。
   void BeginLanding(bool justLanded);

   /// @brief 設定の読み込み時に補間履歴を既定値へ戻す。
   void Reset(const RearCameraSettings& settings);

   /// @brief この段階の確定済み状態を読み取り専用で返す。
   const RearCameraDirectionState& GetState() const { return state_; }

private:
   RearCameraDirectionState state_; ///< この段階の確定済み状態。
};

} // namespace App
