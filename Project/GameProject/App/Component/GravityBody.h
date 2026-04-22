#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "Utility/Math/Quaternion.h"

namespace GameEngine {

/// @brief 重力を受けるオブジェクトのコンポーネント
/// 外部から与えられた重力方向（UpVector）に合わせて姿勢を回転させ、
/// 重力加速度を適用して速度・位置を更新する
class GravityBody final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "GravityBody";
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 更新処理：姿勢の回転補間と物理演算
   void Update(float deltaTime) override;

   /// @brief 目標のUpVectorを設定する（外部の重力発生源から呼ばれる）
   /// @param targetUp 新しい重力方向（正規化されたベクトル）
   void SetTargetUpVector(const Vector3& targetUp);

   /// @brief 重力加速度を設定
   /// @param gravity 重力加速度ベクトル
   void SetGravity(const Vector3& gravity);

   /// @brief 速度を取得
   Vector3 GetVelocity() const { return velocity_; }

   /// @brief 速度を設定
   void SetVelocity(const Vector3& velocity) { velocity_ = velocity; }

   /// @brief 現在のUpVectorを取得
   Vector3 GetCurrentUpVector() const { return currentUpVector_; }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

public:
   // パラメータ
   float rotationSpeed = 5.0f;      ///< Slerp補間の速度（大きいほど速く回転）
   float gravityStrength = 9.8f;    ///< 重力の強さ
   bool useGravity = true;           ///< 重力を適用するか

private:
   /// @brief 現在のUpVectorから目標のUpVectorへの回転を計算し、姿勢を更新
   /// @param deltaTime デルタタイム
   void UpdateRotation(float deltaTime);

   /// @brief 重力加速度を速度に加算し、位置を更新（オイラー積分）
   /// @param deltaTime デルタタイム
   void UpdatePhysics(float deltaTime);

private:
   Vector3 currentUpVector_ = { 0.0f, 1.0f, 0.0f };  ///< 現在のUpVector
   Vector3 targetUpVector_ = { 0.0f, 1.0f, 0.0f };   ///< 目標のUpVector
   Vector3 gravityAcceleration_ = { 0.0f, 0.0f, 0.0f }; ///< 重力加速度ベクトル
   Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };         ///< 速度ベクトル
};

} // namespace GameEngine
