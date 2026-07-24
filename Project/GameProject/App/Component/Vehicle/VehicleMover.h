#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "Utility/Math/Quaternion.h"

namespace App {

/// @brief 車移動の coordinator コンポーネント
///
/// 各サブコンポーネントへ処理を委譲する薄いコーディネーター。
/// 同一 Object に以下のコンポーネントをアタッチして機能を組み合わせる:
///
///   必須:
///     - VehicleGroundMover  … 地上前進・ステアリング・姿勢再構築
///     - VehicleAirController… 空中 roll/pitch 回転（慣性付き）
///
///   オプション（外すと機能が無効化される）:
///     - VehicleLandingAligner … 着地直後の Slerp 姿勢補正
///     - VehicleLandingBoost   … 着地時の速度ブースト
///     - VehicleDrift          … ドリフト処理（将来実装）
class VehicleMover final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "VehicleMover";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "車両移動", "Vehicle Mover" };
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 操舵・空中姿勢入力と接地状態から移動・姿勢を適用する
   /// @param steerInput  地上の左右操舵入力（-1〜+1）
   /// @param rollInput   空中の左右ロール入力（-1〜+1）
   /// @param pitchInput  前後入力（-1〜+1）: 空中のみ有効
   /// @param driftInput  ドリフトボタン入力（VehicleDrift コンポーネントへ転送）
   /// @param isGrounded  接地中なら true
   /// @param gravityUp   現在の重力Up方向
   /// @param deltaTime   フレーム時間
   void ApplyMovement(float steerInput, float rollInput, float pitchInput, bool driftInput, bool isGrounded,
					  const GameEngine::Vector3& gravityUp, float deltaTime);

   /// @brief 直近の移動方向を取得する
   GameEngine::Vector3 GetLastMoveDirection() const { return lastMoveDirection_; }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

private:
   /// @brief 着地遷移時に各サブコンポーネントへ通知する
   void OnLanded(const GameEngine::Quaternion& currentRotation,
				 const GameEngine::Vector3& gravityUp);

   /// @brief VehicleLandingBoost へブースト試行を通知する
   void NotifyLandingBoost(const GameEngine::Vector3& localUp,
						   const GameEngine::Vector3& gravityUp);

   /// @brief VehicleAirController の角速度をリセットする
   void ResetAirAngularVelocity();

   /// @brief VehicleLandingAligner へ補正開始を通知する
   void NotifyLandingAligner(const GameEngine::Quaternion& currentRotation,
							 const GameEngine::Vector3& gravityUp);

   /// @brief gravityUp に合わせた着地目標クォータニオンを生成する
   GameEngine::Quaternion BuildAlignTargetRotation(
	  const GameEngine::Quaternion& currentRotation,
	  const GameEngine::Vector3& gravityUp) const;

   /// @brief 直近の移動方向
   GameEngine::Vector3 lastMoveDirection_ = { 0.0f, 0.0f, 1.0f };

   /// @brief 前フレームの接地状態（着地遷移検出用）
   bool wasGrounded_ = true;
};

} // namespace App
