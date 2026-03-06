#include "PlanetSwitchHelper.h"
#include "SphericalMovementComponent.h"
#include "../GameObject/Planet/Planet.h"
#include "Utility/MathUtils.h"
#include <cmath>
#include <numbers>
#include <algorithm>

using namespace GameEngine;

bool PlanetSwitchHelper::ShouldSwitchPlanet(
   Planet* currentPlanet,
   Planet* newPlanet,
   const Vector3& currentPosition,
   float switchThreshold
) {
   if (!currentPlanet || !newPlanet || currentPlanet == newPlanet) {
      return false;
   }

   Vector3 currentPlanetCenter = currentPlanet->GetWorldPosition();
   float currentDistance = (currentPosition - currentPlanetCenter).Length();
   float currentSurfaceDistance = currentDistance - currentPlanet->GetPlanetRadius();

   Vector3 newPlanetCenter = newPlanet->GetWorldPosition();
   float newDistance = (currentPosition - newPlanetCenter).Length();
   float newSurfaceDistance = newDistance - newPlanet->GetPlanetRadius();

   return (newSurfaceDistance + switchThreshold < currentSurfaceDistance);
}

void PlanetSwitchHelper::SwitchToPlanet(
   SphericalMovementComponent* sphericalMovement,
   Planet* newPlanet,
   const Vector3& currentPosition,
   const Vector3& currentForwardDirection
) {
   if (!sphericalMovement || !newPlanet) return;

   Vector3 newPlanetCenter = newPlanet->GetWorldPosition();
   Vector3 toPlanet = (newPlanetCenter - currentPosition).Normalize();
   Vector3 newHeadDir = toPlanet * -1.0f;

   // 新しい惑星に対してpositionQuaternionを再計算
   Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
   float dot = up.Dot(newHeadDir);

   Quaternion newPositionQuat;
   if (std::abs(dot - 1.0f) < 0.001f) {
      newPositionQuat = Quaternion::Identity();
   } else if (std::abs(dot + 1.0f) < 0.001f) {
      newPositionQuat = MakeRotateAxisAngleQuaternion(Vector3(1.0f, 0.0f, 0.0f), std::numbers::pi_v<float>);
   } else {
      Vector3 axis = up.Cross(newHeadDir).Normalize();
      float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));
      newPositionQuat = MakeRotateAxisAngleQuaternion(axis, angle);
   }

   sphericalMovement->SetPositionQuaternion(newPositionQuat);

   // 新しい座標系で現在の向きを再計算
   Vector3 baseForward = RotateVector(Vector3(0.0f, 0.0f, 1.0f), newPositionQuat);
   Vector3 targetTangent = currentForwardDirection - newHeadDir * newHeadDir.Dot(currentForwardDirection);
   Vector3 baseTangent = baseForward - newHeadDir * newHeadDir.Dot(baseForward);

   if (targetTangent.Length() > 0.001f && baseTangent.Length() > 0.001f) {
      targetTangent = targetTangent.Normalize();
      baseTangent = baseTangent.Normalize();

      float angleDot = std::clamp(baseTangent.Dot(targetTangent), -1.0f, 1.0f);
      float orientAngle = std::acos(angleDot);

      Vector3 cross = baseTangent.Cross(targetTangent);
      float direction = cross.Dot(newHeadDir) > 0.0f ? 1.0f : -1.0f;

      Quaternion newOrientationQuat = MakeRotateAxisAngleQuaternion(Vector3(0.0f, 1.0f, 0.0f), orientAngle * direction);
      sphericalMovement->SetOrientationQuaternion(newOrientationQuat);
   }

   float newDistance = (currentPosition - newPlanetCenter).Length();
   sphericalMovement->SetCurrentRadius(newDistance);
   sphericalMovement->SetCurrentPlanet(newPlanet);
}
