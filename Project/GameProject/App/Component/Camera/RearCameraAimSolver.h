#pragma once
#include "RearCameraComponent.h"
#include "RearCameraTypes.h"
#include "RearCameraTransition.h"

namespace App {

/// @brief 注視点と特異点に強いカメラ姿勢を計算する。
class RearCameraAimSolver : public RearCameraComponent {
public:
   /// @brief 所有カメラのパイプラインから担当処理を1フレーム進める。
   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;
   /// @brief 担当する処理ステージを返す。
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Aim; }
   /// @brief 同一ステージの依存関係に従う更新順を返す。
   int GetExecutionOrder() const override { return -1000; }
   /// @brief シーン保存とFactory登録に使う名前を返す。
   const char* GetComponentName() const override { return "RearCameraAimSolver"; }

   /// @brief 特異点付近で視線追従速度を回復させるBezier曲線の値を返す。
   float EvaluateLookAtRecoveryCurve(const RearCameraSettings& settings, float input) const;
   /// @brief LookAt 行列を構築してカメラ状態へ書き込み、キャッシュ軸を更新する
   /// @details LookAt の up には補間済み重力Upを使用する。
   ///          cachedRight / cachedUp は外部（UI など）で参照されるため確定させる。
   void ApplyLookAt(
      const RearCameraInput& input,
      const RearCameraSettings& settings,
      const RearCameraTransition& transition,
      const RearCameraDirectionState& direction,
      GameEngine::CameraState& state,
      const GameEngine::Vector3& eye,
      const GameEngine::Vector3& up,
      float deltaTime);
   /// @brief 設定の読み込み時に補間履歴を既定値へ戻す。
   void Reset(const RearCameraSettings& settings) override;
   /// @brief この段階の確定済み状態を読み取り専用で返す。
   const RearCameraAimState& GetState() const { return state_; }

private:
   /// @brief LookAt に使う注視点を計算する
   /// @details 地上ではプレイヤー中心より少し上を狙い、画面内の地面比率を下げる。
   ///          空中ではプレイヤー中心へ戻し、着地予測中だけ接触地点の少し先へ注視点を寄せる。
   GameEngine::Vector3 ComputeLookTarget(
      const RearCameraInput& input,
      const RearCameraSettings& settings,
      const RearCameraTransition& transition,
      const RearCameraDirectionState& direction,
      const GameEngine::Vector3& up) const;
   RearCameraAimState state_;
};

} // namespace App
