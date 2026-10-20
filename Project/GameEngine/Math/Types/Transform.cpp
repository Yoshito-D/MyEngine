#include "GameEngine/pch.h"
#include "GameEngine/Math/Types/Transform.h"

namespace GameEngine {

bool Transform::IsFinite() const {
   return std::isfinite(translation.x) && std::isfinite(translation.y) && std::isfinite(translation.z) &&
          std::isfinite(scale.x) && std::isfinite(scale.y) && std::isfinite(scale.z);
}

void Transform::SetRotationEuler(const Vector3& euler) {
   if (!std::isfinite(euler.x) || !std::isfinite(euler.y) || !std::isfinite(euler.z)) return;

   orientation_ = EulerToQuaternion(euler);
   serializeAsQuaternion_ = false;
}

void Transform::SetRotationQuaternion(const Quaternion& quaternion) {
   const float squared = quaternion.x * quaternion.x + quaternion.y * quaternion.y + quaternion.z * quaternion.z +
                         quaternion.w * quaternion.w;

   if (!std::isfinite(squared) || squared < 1.0e-12f) return;

   orientation_ = quaternion.Normalize();
   serializeAsQuaternion_ = true;
}

} // namespace GameEngine
