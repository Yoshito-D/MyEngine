#pragma once
#include "RearCameraComponent.h"
#include "RearCameraTypes.h"

namespace App {

/// @brief 速度に応じたFOVと距離キックを管理する。
class RearCameraSpeedEffects : public RearCameraComponent {
public:
   /// @brief 所有カメラのパイプラインから担当処理を1フレーム進める。
   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;
   /// @brief 担当する処理ステージを返す。
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Body; }
   /// @brief 同一ステージの依存関係に従う更新順を返す。
   int GetExecutionOrder() const override { return -500; }
   /// @brief シーン保存とFactory登録に使う名前を返す。
   const char* GetComponentName() const override { return "RearCameraSpeedEffects"; }

   /// @brief プレイヤー速度に応じた FOV ブーストを補間し state.fov へ反映する
   /// @details GravityFollowCamera と同じ考え方で FOV を広げて速度感を演出する。
   ///          加えて、boostAlpha を返すことで距離ブースト（ComputeEye）と
   ///          同じ速度スケールを共有できるようにする。
   /// @return boostAlpha [0, 1]（加速の強さ。距離ブーストにも流用）
   float UpdateAccelerationEffect(
      const RearCameraInput& input,
      const RearCameraSettings& settings,
      const RearCameraTransitionState& transition,
      GameEngine::CameraState& state,
      float deltaTime);
   /// @brief 設定の読み込み時に補間履歴を既定値へ戻す。
   void Reset(const RearCameraSettings& settings) override;
   /// @brief この段階の確定済み状態を読み取り専用で返す。
   const RearCameraSpeedState& GetState() const { return state_; }

   /// @brief 距離演出へ渡す現在の加速割合を返す。
   float GetBoostAlpha() const { return boostAlpha_; }

private:
   float boostAlpha_ = 0.0f;
   RearCameraSpeedState state_;
};

} // namespace App
