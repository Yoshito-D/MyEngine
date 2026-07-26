#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "Utility/Math/Quaternion.h"

namespace App {

class GravityBody;

/// @brief 空中の roll / pitch 回転と慣性減衰を担うコンポーネント
///
/// - 入力による加速・反転は即時反映
/// - 入力を弱めるか離すと angularDamping に従って指数的に減衰
/// - 着地した瞬間に角速度をリセットする（VehicleMover から呼ばれる）
class VehicleAirController final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "VehicleAirController";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "車両空中制御", "Vehicle Air Controller" };
   /// @copydoc GameEngine::IObjectComponent::GetTypeName
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 空中の回転を適用する
   /// @param rollInput   左右ロール入力（-1〜+1）
   /// @param pitchInput  前後入力（-1〜+1）
   /// @param deltaTime   フレーム時間
   void Apply(float rollInput, float pitchInput, float deltaTime);

   /// @brief 着地時に角速度をリセットする
   void ResetAngularVelocity() { angularVelRoll_ = 0.0f; angularVelPitch_ = 0.0f; }

#ifdef USE_IMGUI
   /// @copydoc GameEngine::IObjectComponent::DrawInspector
   void DrawInspector() override;
#endif

   /// @copydoc GameEngine::IObjectComponent::Serialize
   nlohmann::json Serialize() const override;
   /// @copydoc GameEngine::IObjectComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

public:
   /// @brief ロール角速度（deg/sec）
   float rollSpeed     = 285.0f;

   /// @brief ピッチ角速度（deg/sec）
   float pitchSpeed     = 285.0f;

   /// @brief 空中回転の減衰係数（大きいほど慣性が少ない）
   float angularDamping = 2.5f;

   /// @brief 空中の水平減速係数（per sec）
   /// 小さい値にするほど空中での減速が緩やかになる
   float airDrag = 0.001f;

private:
   /// @brief アナログ入力を考慮して角速度の加速・反転・慣性減衰を更新する
   void UpdateAngularVelocity(float input, float& angVel,
                              float targetVel, float deltaTime) const;

   /// @brief pitch 角速度を回転クォータニオンへ反映する
   void ApplyPitchRotation(GameEngine::Quaternion& rot,
                           const GameEngine::Vector3& localRight, float deltaTime) const;

   /// @brief roll 角速度を回転クォータニオンへ反映する
   void ApplyRollRotation(GameEngine::Quaternion& rot,
	                      const GameEngine::Vector3& localForward, float deltaTime) const;

   float angularVelRoll_  = 0.0f;
   float angularVelPitch_ = 0.0f;
};

} // namespace App
