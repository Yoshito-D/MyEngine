#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "Utility/Math/Quaternion.h"

namespace App {

/// @brief 接地中の前進・ステアリング・姿勢再構築を担うコンポーネント
///
/// - 毎フレーム autoSpeed へ向けて速度を回復しながら前進
/// - ステアリング入力で yaw 回転
/// - (gravityUp, flatForward) から姿勢を完全再構築
/// - VehicleLandingAligner が同 Object にあれば補正中は姿勢再構築をスキップする
class VehicleGroundMover final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "VehicleGroundMover";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "車両地上移動", "Vehicle Ground Mover" };
   /// @copydoc GameEngine::IObjectComponent::GetTypeName
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 接地中の移動・姿勢を適用する
   /// @param steerInput  左右入力（-1〜+1）
   /// @param gravityUp   現在の重力Up方向
   /// @param deltaTime   フレーム時間
   void Apply(float steerInput, const GameEngine::Vector3& gravityUp, float deltaTime);

   /// @brief 現在の前進実速度を設定する（着地ブーストから呼ばれる）
   void SetCurrentSpeed(float speed) { currentSpeed_ = speed; }

   /// @brief 加速度を積む（持続的な加速・減速用）
   /// 積まれた加速度は UpdateSpeed 内で v += a * dt として適用され、
   /// 適用後にリセットされる。autoSpeed への回復はその後に行われる。
   void AddAcceleration(float accel);

   /// @brief 速度を即座に加算する（着地ブースト・ペナルティなど瞬間的な速度変化用）
   /// autoSpeed への回復は UpdateSpeed の指数平滑によって行われる。
   void AddVelocityImpulse(float impulse);

   /// @brief 現在の前進実速度を取得する
   float GetCurrentSpeed() const { return currentSpeed_; }

   /// @brief 直近の水平前方ベクトルを取得する
   GameEngine::Vector3 GetFlatForward() const { return flatForward_; }

#ifdef USE_IMGUI
   /// @copydoc GameEngine::IObjectComponent::DrawInspector
   void DrawInspector() override;
#endif

   /// @copydoc GameEngine::IObjectComponent::Serialize
   nlohmann::json Serialize() const override;
   /// @copydoc GameEngine::IObjectComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

public:
   /// @brief 自動前進速度（units/sec）
   float autoSpeed = 16.0f;

   /// @brief ステアリング角速度（deg/sec）
   float steerSpeed = 120.0f;

   /// @brief スピード回復速度（per sec）
   float speedRecovery = 1.0f;

   float maxSpeed = 40.0f; ///< 外部加速を含めた前進速度の上限
private:
   /// @brief 速度を autoSpeed へ向けて回復させる
   void UpdateSpeed(float deltaTime);

   /// @brief ステアリング入力を反映した前方ベクトルを返す
   GameEngine::Vector3 ApplySteering(float steerInput,
	  const GameEngine::Vector3& localForward,
	  const GameEngine::Vector3& gravityUp,
	  float deltaTime) const;

   /// @brief 方向ベクトルを gravityUp 平面に投影して正規化する
   GameEngine::Vector3 ProjectToHorizontalPlane(const GameEngine::Vector3& dir,
	  const GameEngine::Vector3& gravityUp) const;

   /// @brief GravityBody の水平速度を flatForward * currentSpeed_ に更新する
   void ApplyVelocityToGravityBody(const GameEngine::Vector3& flatForward,
	  const GameEngine::Vector3& gravityUp);

   /// @brief (gravityUp, flatForward) から姿勢クォータニオンを再構築して適用する
   void RebuildPosture(const GameEngine::Vector3& flatForward,
	  const GameEngine::Vector3& gravityUp);

   /// @brief right / up / fwd 基底からクォータニオンを生成する
   static GameEngine::Quaternion BasisToQuaternion(const GameEngine::Vector3& right,
	  const GameEngine::Vector3& up,
	  const GameEngine::Vector3& fwd);

   /// @brief 現在の前進実速度（負値は未初期化を示す）
   float currentSpeed_ = -1.0f;

   /// @brief 外部から積まれた加速度（units/sec²）。UpdateSpeed で v += a * dt して毎フレームリセット
   float acceleration_ = 0.0f;

   /// @brief 外部から積まれた瞬間速度変化（units/sec）。UpdateSpeed で直接加算してリセット
   float velocityImpulse_ = 0.0f;

   /// @brief 直近の水平前方ベクトル
   GameEngine::Vector3 flatForward_ = { 0.0f, 0.0f, 1.0f };

   const float kInitialSpeed = 15.0f;
};

} // namespace App
