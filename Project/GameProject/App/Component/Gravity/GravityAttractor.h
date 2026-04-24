#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Utility/Math/Vector3.h"
#include "GravityBody.h"
#include <vector>

namespace App {

/// @brief 重力発生源の抽象基底クラス
class GravityAttractor : public GameEngine::IObjectComponent {
public:
   virtual bool IsInRange(const GameEngine::Vector3& objectPosition) const = 0;
   virtual GameEngine::Vector3 GetUpVectorFor(const GameEngine::Vector3& objectPosition) const = 0;

   void ApplyTo(GravityBody& gravityBody, const GameEngine::Vector3& objectPosition) const {
      if (!IsEnabled()) { return; }
      if (!IsInRange(objectPosition)) { return; }

      GameEngine::Vector3 upVector = GetUpVectorFor(objectPosition);
      gravityBody.SetTargetUpVector(upVector);
      gravityBody.SetGravity(-upVector * gravityBody.gravityStrength);
   }

public:
   bool enabled = true;
};

} // namespace App
