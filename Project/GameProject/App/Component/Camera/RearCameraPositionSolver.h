#pragma once
#include "RearCameraComponent.h"
#include "RearCameraTypes.h"

namespace App {

/// @brief ピボット相対位置の追従と制約を管理する。
class RearCameraPositionSolver : public RearCameraComponent {
public:
   /// @brief 所有カメラのパイプラインから担当処理を1フレーム進める。
   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;
   /// @brief 担当する処理ステージを返す。
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Body; }
   /// @brief 同一ステージの依存関係に従う更新順を返す。
   int GetExecutionOrder() const override { return -400; }
   /// @brief シーン保存とFactory登録に使う名前を返す。
   const char* GetComponentName() const override { return "RearCameraPositionSolver"; }

   /// @brief 初回は保存位置を採用し、以降は相対位置を追従させる。
   GameEngine::Vector3 Update(
      const RearCameraInput& input,
      const RearCameraSettings& settings,
      const RearCameraTransitionState& transition,
      const RearCameraDirectionState& direction,
      const RearCameraSpeedState& speed,
      const RearCameraAimState& aim,
      GameEngine::CameraState& state,
      const GameEngine::Vector3& up,
      float boostAlpha,
      float deltaTime);
   /// @brief 設定の読み込み時に補間履歴を既定値へ戻す。
   void Reset(const RearCameraSettings& settings) override;
   /// @brief この段階の確定済み状態を読み取り専用で返す。
   const RearCameraPositionState& GetState() const { return state_; }

private:
   /// @brief eye 位置を計算する（後退距離に加速ブーストを加味）
   /// @details カメラは pivot の後方（currentBackward 方向）に distance 離れた場所に置く。
   ///          加速中は distanceBoostMax * boostAlpha 分さらに後退させることで
   ///          「カメラが引けて世界が広がる」視覚的な加速感を演出する。
   ///          空中では currentAirborneBlend に応じてさらに後退し、周囲を広く映す。
   /// @param up カメラ位置の高さ方向
   /// @param boostAlpha 加速度合い [0,1]
   GameEngine::Vector3 ComputeEye(
      const RearCameraInput& input,
      const RearCameraSettings& settings,
      const RearCameraTransitionState& transition,
      const RearCameraDirectionState& direction,
      const RearCameraSpeedState& speed,
      const GameEngine::Vector3& up,
      float boostAlpha) const;
   /// @brief 目標 eye オフセット（ピボット相対）の方向変化を制限し、距離を補間する
   /// @details 【なぜ相対オフセットで補間するか】
   ///          絶対座標で補間すると、ピボット（プレイヤー）が移動するたびに
   ///          理想 eye も一緒にワールド空間を動く。この場合の補間パスは
   ///          「前フレームの eye（旧ピボット後方）→ 今フレームの eye（新ピボット後方）」
   ///          という直線を通るため、一瞬プレイヤーを突き抜けるルートになり得る。
   ///
   ///          現在の実装では方向を UpdateBackwardVector で一度だけ補間し、
   ///          ここでは通常の方向変化へ遅延を足さず、急変時だけ最大角速度を制限する。
   ///          ピボット相対距離は指数平滑で目標へ近づける。
   ///          補間後はUp方向の高さ下限を適用し、カメラが基準面より下へ沈みすぎることを防ぐ。
   GameEngine::Vector3 SmoothEye(
      const RearCameraInput& input,
      const RearCameraSettings& settings,
      const RearCameraTransitionState& transition,
      const RearCameraAimState& aim,
      const GameEngine::Vector3& targetEye,
      const GameEngine::Vector3& up,
      float deltaTime);
   RearCameraPositionState state_;
};

} // namespace App
