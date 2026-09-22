#pragma once
#include "RearCameraComponent.h"
#include "RearCameraTypes.h"

namespace App {

/// @brief 空中の惑星方向ガイドを管理する。
class RearCameraPlanetGuide : public RearCameraComponent {
public:
   /// @brief 所有カメラのパイプラインから担当処理を1フレーム進める。
   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;
   /// @brief 担当する処理ステージを返す。
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Body; }
   /// @brief 同一ステージの依存関係に従う更新順を返す。
   int GetExecutionOrder() const override { return -700; }
   /// @brief シーン保存とFactory登録に使う名前を返す。
   const char* GetComponentName() const override { return "RearCameraPlanetGuide"; }

   /// @brief 空中時にカメラ方向を近傍惑星側へ寄せるための方向と係数を更新する
   /// @details 注視点はプレイヤー中心のまま保ち、eye 側の回り込み方向だけを補間する。
   ///          補間開始前は現在の後方へ張り付け、重力条件を満たした瞬間の逆振れを防ぐ。
   void UpdatePlanetDirectionGuide(
      const RearCameraInput& input,
      const RearCameraSettings& settings,
      const GameEngine::Vector3& currentBackward,
      float deltaTime);
   /// @brief 離陸後の待機と復帰を反映した惑星ガイド追従速度を計算する
   /// @details 待機中は0を返す。待機終了後はSmoothstepで0から設定値へ戻し、
   ///          地上時は次回離陸へ影響しないよう常に設定値を返す。
   float ComputePlanetDirectionFollowSpeed(const RearCameraInput& input, const RearCameraSettings& settings) const;
   /// @brief 現在の空中惑星方向補間量を返す
   /// @details 基本補間量に対し、速度が惑星中心方向（重力Down方向）へ近いほど係数を上げる。
   ///          開始判定OFF時は係数目標を1にし、方向の立ち上がりだけは滑らかに保つ。
   float ComputePlanetDirectionBlend(
      const RearCameraInput& input,
      const RearCameraSettings& settings,
      const RearCameraTransitionState& transition,
      const GameEngine::Vector3& trackedBackward) const;
   /// @brief 離陸時の待機・復帰タイマーをガイド更新前に進める。
   void AdvanceTakeoffTimer(
      const RearCameraInput& input,
      const RearCameraSettings& settings,
      const RearCameraFrameEvents& events,
      const GameEngine::Vector3& currentBackward,
      float deltaTime);
   /// @brief 設定の読み込み時に補間履歴を既定値へ戻す。
   void Reset(const RearCameraSettings& settings) override;
   /// @brief この段階の確定済み状態を読み取り専用で返す。
   const RearCameraPlanetState& GetState() const { return state_; }

private:
   /// @brief 速度方向と重力Down方向の近さから惑星方向補間の目標係数を計算する
   /// @details 開始閾値と最大閾値の間を smoothstep でならし、bias で効き方を調整する。
   ///          開始閾値未満では惑星方向補間を始めず、最大閾値で最大係数になる。
   float ComputePlanetDirectionGravityFactorTarget(const RearCameraInput& input, const RearCameraSettings& settings) const;
   RearCameraPlanetState state_;
};

} // namespace App
