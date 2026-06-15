#pragma once

#include "Vector3.h"
#include "Quaternion.h"

#include <cmath>

namespace GameEngine {

struct Transform {
   enum class RotationSource {
      Euler,
      Quaternion
   };

   Vector3 scale;
   Vector3 rotation;
   Vector3 translation;
   Quaternion rotationQuaternion;
   RotationSource rotationSource;

   Transform()
      : scale(1.0f, 1.0f, 1.0f)
      , rotation(0.0f, 0.0f, 0.0f)
      , translation(0.0f, 0.0f, 0.0f)
      , rotationQuaternion(Quaternion::Identity())
      , rotationSource(RotationSource::Euler) {
   }

   void SetRotationEuler(const Vector3& euler) {
      rotation = euler;
      rotationQuaternion = EulerToQuaternion(euler);
      rotationSource = RotationSource::Euler;
   }

   void SetRotationQuaternion(const Quaternion& quaternion) {
      rotationQuaternion = quaternion.Normalize();
      rotation = QuaternionToEuler(rotationQuaternion);
      rotationSource = RotationSource::Quaternion;
   }

   bool IsUsingQuaternion() const {
      return rotationSource == RotationSource::Quaternion;
   }

   Quaternion GetActiveQuaternion() const {
      if (rotationSource == RotationSource::Quaternion) {
         return rotationQuaternion.Normalize();
      }
      return EulerToQuaternion(rotation);
   }

   Vector3 GetActiveEuler() const {
      if (rotationSource == RotationSource::Quaternion) {
         return QuaternionToEuler(rotationQuaternion);
      }
      return rotation;
   }

private:
   static Quaternion EulerToQuaternion(const Vector3& eulerAngles) {
      return eulerAngles.ToQuaternion().Normalize();
   }

   static Vector3 QuaternionToEuler(const Quaternion& quaternion) {
      return quaternion.Normalize().ToEuler();
   }
};

} // namespace GameEngine
