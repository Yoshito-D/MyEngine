#pragma once

#include "GameEngine/Math/Types/Vector3.h"
#include "GameEngine/Math/Types/Quaternion.h"

#include <cmath>

namespace GameEngine {

/// @brief 3次元オブジェクトの拡大縮小・回転・平行移動をまとめた変換情報。
/// @details 姿勢は正規化済みQuaternionだけを正本として保持し、Euler角は表示時に導出する。
struct Transform {
   /// @brief 現在の姿勢を決定する回転表現。
   enum class RotationSource {
      Euler, //!< rotationのEuler角を使用する
      Quaternion //!< rotationQuaternionを使用する
   };

   Vector3 scale{ 1.0f, 1.0f, 1.0f }; //!< ローカル軸の拡大率
   Vector3 translation{}; //!< 平行移動量
   /// @brief 恒等変換を生成する。
   Transform() = default;
   /// @brief 有限なEuler角を姿勢へ変換して適用する。
   void SetRotationEuler(const Vector3& euler) {
      if (!std::isfinite(euler.x) || !std::isfinite(euler.y) || !std::isfinite(euler.z)) return;
      orientation_ = EulerToQuaternion(euler);
      serializeAsQuaternion_ = false;
   }
   /// @brief 有限かつ非零のQuaternionを正規化して姿勢へ適用する。
   void SetRotationQuaternion(const Quaternion& quaternion) {
      const float squared = quaternion.x * quaternion.x + quaternion.y * quaternion.y + quaternion.z * quaternion.z + quaternion.w * quaternion.w;
      if (!std::isfinite(squared) || squared < 1.0e-12f) return;
      orientation_ = quaternion.Normalize();
      serializeAsQuaternion_ = true;
   }
   /// @brief 保存時にQuaternion表現を優先するか調べる。計算の正本は常にQuaternion。
   bool IsUsingQuaternion() const { return serializeAsQuaternion_; }
   /// @brief 正規化済みの姿勢を読み取る。
   Quaternion GetActiveQuaternion() const { return orientation_; }
   /// @brief 表示用Euler角を姿勢から導出する。
   Vector3 GetActiveEuler() const { return QuaternionToEuler(orientation_); }

private:
   Quaternion orientation_ = Quaternion::Identity();
   bool serializeAsQuaternion_ = false;
   static Quaternion EulerToQuaternion(const Vector3& eulerAngles) {
      return eulerAngles.ToQuaternion().Normalize();
   }

   static Vector3 QuaternionToEuler(const Quaternion& quaternion) {
      return quaternion.Normalize().ToEuler();
   }
};

} // namespace GameEngine
