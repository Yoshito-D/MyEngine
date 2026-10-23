#pragma once

#include <cstdint>
#include "GameProject/App/Component/Camera/RearFollow/RearCameraComponent.h"
#include "GameProject/App/Component/Camera/RearFollow/RearCameraTypes.h"

namespace App {

/// @brief 速度に応じたFOVと距離キックを管理する。
class RearCameraSpeedEffects : public RearCameraComponent {
public:
   /// @brief 所有カメラのパイプラインから担当処理を1フレーム進める。
   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;

   /// @brief 担当する処理ステージを返す。
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Body; }

   /// @brief 同一ステージの依存関係に従う更新順を返す。
   std::int32_t GetExecutionOrder() const override { return -500; }

   /// @brief シーン保存とFactory登録に使う名前を返す。
   const char* GetComponentName() const override { return "RearCameraSpeedEffects"; }

   /// @brief 通常FOVと演出による追加FOVを合成し state.fov へ反映する
   /// @details 通常FOVの編集は直接反映し、速度・空中演出の追加量だけを補間する。
   ///          加えて、boostAlpha を返すことで距離ブースト（ComputeEye）と
   ///          同じ速度スケールを共有できるようにする。
   /// @return boostAlpha [0, 1]（加速の強さ。距離ブーストにも流用）
   float UpdateAccelerationEffect(const RearCameraInput& input, const RearCameraSettings& settings,
      const RearCameraTransitionState& transition, GameEngine::CameraState& state, float deltaTime);

   /// @brief 設定の読み込み時に補間履歴を既定値へ戻す。
   void Reset(const RearCameraSettings& settings) override;

   /// @brief この段階の確定済み状態を読み取り専用で返す。
   const RearCameraSpeedState& GetState() const { return state_; }

   /// @brief 距離演出へ渡す現在の加速割合を返す。
   float GetBoostAlpha() const { return boostAlpha_; }

private:
   float boostAlpha_ = 0.0f;    ///< 距離演出へ渡す現在の加速割合。
   RearCameraSpeedState state_; ///< この段階の確定済み状態。
};

} // namespace App
