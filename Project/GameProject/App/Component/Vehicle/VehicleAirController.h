#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "Utility/Math/Quaternion.h"

namespace App {

/// @brief 空中の yaw / pitch 回転と慣性減衰を担うコンポーネント
///
/// - 入力がある間はターゲット角速度へ即追従
/// - 入力を離すと angularDamping に従って指数的に減衰
/// - 着地した瞬間に角速度をリセットする（VehicleMover から呼ばれる）
class VehicleAirController final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "VehicleAirController";
   const char* GetTypeName() const override { return kTypeName; }
   void Update(float deltaTime) override { (void)deltaTime; }

   /// @brief 空中の回転を適用する
   /// @param steerInput  左右入力（-1〜+1）
   /// @param pitchInput  前後入力（-1〜+1）
   /// @param deltaTime   フレーム時間
   void Apply(float steerInput, float pitchInput, float deltaTime);

   /// @brief 着地時に角速度をリセットする
   void ResetAngularVelocity() { angularVelYaw_ = 0.0f; angularVelPitch_ = 0.0f; }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

public:
   /// @brief ロール角速度（deg/sec）
   float rollSpeed     = 270.0f;

   /// @brief ピッチ角速度（deg/sec）
   float pitchSpeed     = 270.0f;

   /// @brief 空中回転の減衰係数（大きいほど慣性が少ない）
   float angularDamping = 2.5f;

private:
   /// @brief 入力に応じて角速度を更新する（入力なし時は指数減衰）
   void UpdateAngularVelocity(float input, float& angVel,
                              float targetVel, float deltaTime) const;

   /// @brief yaw 角速度を回転クォータニオンへ反映する
   void ApplyYawRotation(GameEngine::Quaternion& rot,
                         const GameEngine::Vector3& localUp, float deltaTime) const;

   /// @brief pitch 角速度を回転クォータニオンへ反映する
   void ApplyPitchRotation(GameEngine::Quaternion& rot,
                           const GameEngine::Vector3& localRight, float deltaTime) const;

   /// @brief roll 角速度を回転クォータニオンへ反映する
   void ApplyRollRotation(GameEngine::Quaternion& rot,
	                      const GameEngine::Vector3& localForward, float deltaTime) const;

   float angularVelYaw_   = 0.0f;
   float angularVelPitch_ = 0.0f;
};

} // namespace App
