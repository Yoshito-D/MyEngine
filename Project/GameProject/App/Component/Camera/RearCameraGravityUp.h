#pragma once
#include "RearCameraComponent.h"
#include "RearCameraTypes.h"

namespace App {
class RearCameraDirectionTracker;

/// @brief 惑星切替と着地予測に対して重力Upを連続的に追従させる。
class RearCameraGravityUp : public RearCameraComponent {
public:
   /// @brief 所有カメラのパイプラインから担当処理を1フレーム進める。
   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;
   /// @brief 担当する処理ステージを返す。
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Body; }
   /// @brief 同一ステージの依存関係に従う更新順を返す。
   int GetExecutionOrder() const override { return -800; }
   /// @brief シーン保存とFactory登録に使う名前を返す。
   const char* GetComponentName() const override { return "RearCameraGravityUp"; }

   /// @brief 補間済みUpを初期方向へ戻す。
   void Reset(const RearCameraSettings& settings) override;
   /// @brief 補間済み重力Upを読み取り専用で返す。
   const RearCameraGravityState& GetState() const { return state_; }
private:
   GameEngine::Vector3 SmoothGravityUp(
      const RearCameraInput& input,
      const RearCameraSettings& settings,
      const RearCameraTransitionState& transition,
      const RearCameraAimState& aim,
      RearCameraDirectionTracker& direction,
      float deltaTime);
   RearCameraGravityState state_;
};

} // namespace App
