#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Utility/Math/Vector3.h"
#include "GravityBody.h"
#include <vector>

namespace App {

/// @brief 重力場を提供する抽象基底コンポーネント
class GravityAttractor : public GameEngine::IObjectComponent {
public:
   /// @brief 指定座標が重力影響範囲内かどうかを返す
   virtual bool IsInRange(const GameEngine::Vector3& objectPosition) const = 0;

   /// @brief 指定座標に対する重力Up（地表法線相当）を返す
   virtual GameEngine::Vector3 GetUpVectorFor(const GameEngine::Vector3& objectPosition) const = 0;

   /// @brief GravityBody に重力方向と加速度を適用する
   void ApplyTo(GravityBody& gravityBody, const GameEngine::Vector3& objectPosition) const {
      // 非有効時は何もしない
      if (!IsEnabled()) { return; }

      // 影響範囲外なら適用しない
      if (!IsInRange(objectPosition)) { return; }

      // Up方向を基準に姿勢目標と重力加速度を設定
      GameEngine::Vector3 upVector = GetUpVectorFor(objectPosition);
      gravityBody.SetTargetUpVector(upVector);
      gravityBody.SetGravity(-upVector * gravityBody.gravityStrength);
   }

public:
   /// @brief コンポーネント有効フラグ（将来拡張用）
   bool enabled = true;
};

} // namespace App
