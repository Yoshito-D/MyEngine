#include "GameEngine/Math/Functions/CollisionGeometry.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include <cmath>

namespace GameEngine {
float ComputeObbSupportRadius(const Quaternion& rotation, const Vector3& halfExtents, const Vector3& surfaceUp) {
   // OBBの各ローカル軸を法線へ射影し、法線方向に張り出す半径へ合成する。
   const Vector3 axisX = RotateVector({1.0f, 0.0f, 0.0f}, rotation);
   const Vector3 axisY = RotateVector({0.0f, 1.0f, 0.0f}, rotation);
   const Vector3 axisZ = RotateVector({0.0f, 0.0f, 1.0f}, rotation);
   return std::abs(axisX.Dot(surfaceUp)) * std::abs(halfExtents.x) +
          std::abs(axisY.Dot(surfaceUp)) * std::abs(halfExtents.y) +
          std::abs(axisZ.Dot(surfaceUp)) * std::abs(halfExtents.z);
}
}
