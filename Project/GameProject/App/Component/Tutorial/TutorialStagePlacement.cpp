#include "GameProject/App/Component/Tutorial/TutorialStagePlacement.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include <algorithm>
#include <cmath>

namespace App {
bool PlaceTutorialDestination(const GameEngine::Transform& playerPose, const GameEngine::Vector3& sourceCenter,
   float sourceRadius, float targetRadius, float surfaceGap, GameEngine::Transform& targetPose, GameEngine::Transform& goalPose) {
   const auto up = (playerPose.translation - sourceCenter).Normalize();
   auto forward = GameEngine::RotateVector({0.0f, 0.0f, 1.0f}, playerPose.GetActiveQuaternion());
   forward = forward - up * forward.Dot(up);

   if (forward.LengthSquared() < 1.0e-6f) {
      return false;
   }

   forward = forward.Normalize();
   constexpr float kMinimumPlanetGap = 10.0f; ///< 惑星同士を離す最小地表間隔（m）。
   const float minimumCenterDistance = sourceRadius + targetRadius + kMinimumPlanetGap;
   const float minimumForwardDistance =
      std::sqrt(std::max(minimumCenterDistance * minimumCenterDistance - sourceRadius * sourceRadius, 0.0f));
   const float forwardDistance = std::max(targetRadius + surfaceGap, minimumForwardDistance);

   // 長時間説明を読んだ場合や周回後でも、練習開始時の正面に出現させる。
   // 惑星同士に最小地表間隔を確保し、元の地表と重ならないようにする。
   targetPose.translation = sourceCenter + up * sourceRadius + forward * forwardDistance;
   const auto goalNormal = (up * 0.6f - forward * 0.8f).Normalize();
   goalPose.translation = targetPose.translation + goalNormal * targetRadius;
   // 旗の支柱の+Yを地表の外向き法線へ合わせ、地中へ埋めず地表から垂直に立てる。
   goalPose.SetRotationQuaternion(GameEngine::LookRotation(up * 0.8f + forward * 0.6f, goalNormal));
   return true;
}
}
