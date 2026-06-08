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
      const float cy = std::cos(eulerAngles.y * 0.5f);
      const float sy = std::sin(eulerAngles.y * 0.5f);
      const float cp = std::cos(eulerAngles.x * 0.5f);
      const float sp = std::sin(eulerAngles.x * 0.5f);
      const float cr = std::cos(eulerAngles.z * 0.5f);
      const float sr = std::sin(eulerAngles.z * 0.5f);

      Quaternion q;
      q.w = cr * cp * cy + sr * sp * sy;
      q.x = sr * cp * cy - cr * sp * sy;
      q.y = cr * sp * cy + sr * cp * sy;
      q.z = cr * cp * sy - sr * sp * cy;
      return q.Normalize();
   }

   static Vector3 QuaternionToEuler(const Quaternion& quaternion) {
      Quaternion q = quaternion.Normalize();

      const float sinPitch = 2.0f * (q.w * q.x - q.y * q.z);
      float pitch;
      if (std::fabs(sinPitch) >= 1.0f) {
         pitch = std::copysign(3.14159265358979323846f * 0.5f, sinPitch);
      } else {
         pitch = std::asin(sinPitch);
      }

      const float yaw = std::atan2(2.0f * (q.w * q.y + q.z * q.x), 1.0f - 2.0f * (q.x * q.x + q.y * q.y));
      const float roll = std::atan2(2.0f * (q.w * q.z + q.x * q.y), 1.0f - 2.0f * (q.x * q.x + q.z * q.z));

      return Vector3(pitch, yaw, roll);
   }
};

} // namespace GameEngine