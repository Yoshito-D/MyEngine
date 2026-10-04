#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include "GameEngine/Math/Types/Quaternion.h"

namespace App {

/// @brief 重力方向に追従して姿勢・速度・位置を更新する物理ボディ
class GravityBody final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "GravityBody";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "重力ボディ", "Gravity Body" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 姿勢補間と重力物理更新を実行する
   void Update(float deltaTime) override;

   /// @brief 目標Up方向を設定する（回転補間に使用）
   void SetTargetUpVector(const GameEngine::Vector3& targetUp);

   /// @brief 目標Upへ即座に姿勢スナップする
   void SnapToUpVector(const GameEngine::Vector3& targetUp);

   /// @brief 地表法線から姿勢目標と、このボディの設定に基づく重力を適用する
   /// @param surfaceUp 地表の外向き法線。ゼロベクトルの場合は変更しない
   void ApplyGravityFromSurface(const GameEngine::Vector3& surfaceUp);

   /// @brief 重力源との接続を解除し、前フレームの加速度を破棄する
   void ReleaseGravity();

   /// @brief 仮の地表法線で重力を積分し、予測速度を返す（実際の状態は変更しない）
   /// @param velocity 予測開始時の速度
   /// @param surfaceUp 候補地表の単位法線
   /// @param deltaTime 予測する時間（秒）
   GameEngine::Vector3 PredictVelocity(const GameEngine::Vector3& velocity,
      const GameEngine::Vector3& surfaceUp, float deltaTime) const;

   /// @brief 現在速度を取得する
   GameEngine::Vector3 GetVelocity() const { return velocity_; }

   /// @brief ジャンプなどの瞬間的な速度変化を加える
   /// @param impulse 加算する速度ベクトル
   void AddVelocityImpulse(const GameEngine::Vector3& impulse);

   /// @brief 重力法線方向の速度を保ちながら、地表に沿った移動速度を適用する
   /// @param surfaceVelocity 地表に沿った速度ベクトル
   /// @param surfaceUp 地表の単位法線
   void ApplySurfaceVelocity(const GameEngine::Vector3& surfaceVelocity,
      const GameEngine::Vector3& surfaceUp);

   /// @brief 現在Upに沿う速度を保ち、地表に沿う速度だけを指数減衰させる
   /// @param drag 減衰係数（per sec）
   /// @param deltaTime フレーム時間（秒）
   void ApplySurfaceDrag(float drag, float deltaTime);

   /// @brief 着地時に法線方向の速度を除去し、地表に沿う速度を保持する
   /// @param surfaceUp 地表の単位法線
   void CancelNormalVelocity(const GameEngine::Vector3& surfaceUp);

   /// @brief 移動を完全に停止する
   void StopMotion();

   /// @brief 現在のUp方向を取得する
   GameEngine::Vector3 GetCurrentUpVector() const { return currentUpVector_; }

   /// @brief 今フレームの重力目標Up方向を取得する
   GameEngine::Vector3 GetTargetUpVector() const { return targetUpVector_; }

   /// @brief 現在のUp方向を強制設定する（空中姿勢変化後の着地補正などに使用）
   void SetCurrentUpVector(const GameEngine::Vector3& up) { currentUpVector_ = up.Normalize(); }

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

private:
   /// @brief Up補間回転速度
   float rotationSpeed = 5.0f;

   /// @brief 重力強度（加速度係数）
   float gravityStrength = 9.8f;

   /// @brief 重力適用フラグ
   bool  useGravity = true;

private:
   /// @brief 現在Upから目標Upへ姿勢を補間する
   void UpdateRotation(float deltaTime);

   /// @brief 重力加速度で速度・位置を更新する
   void UpdatePhysics(float deltaTime);

private:
   /// @brief 現在のUpベクトル
   GameEngine::Vector3 currentUpVector_ = { 0.0f, 1.0f, 0.0f };

   /// @brief 次に向かうUpベクトル
   GameEngine::Vector3 targetUpVector_ = { 0.0f, 1.0f, 0.0f };

   /// @brief 現在の重力加速度
   GameEngine::Vector3 gravityAcceleration_ = { 0.0f, 0.0f, 0.0f };

   /// @brief 現在の速度
   GameEngine::Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
};

} // namespace App
