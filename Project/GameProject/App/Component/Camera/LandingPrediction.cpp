#include "GameProject/App/Component/Camera/LandingPrediction.h"
#include "GameEngine/Math/Functions/CollisionGeometry.h"
#include "GameProject/App/Component/Camera/RearFollow/RearCameraMath.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include <algorithm>

namespace App {
using RearCameraMath::NormalizeOrFallback;

namespace {
constexpr float kLandingPredictionStepSeconds = 1.0f / 60.0f;
constexpr float kLandingPredictionDirectionEpsilon = 1e-4f;
}

bool PredictLanding(const GameEngine::Vector3& position, const GameEngine::Vector3& velocity,
   const GameEngine::Vector3& planetCenter, float surfaceRadius, float landingOffset,
   const GameEngine::Vector3& obbHalfExtents, const GameEngine::Quaternion& playerRotation,
   const GravityBody& gravityBody, float predictionHorizon, const GameEngine::Vector3& fallbackForward,
   const GameEngine::Vector3& fallbackRight, LandingPrediction& outPrediction) {
   float horizon = std::clamp(predictionHorizon, 0.0f, 5.0f);

   if (horizon <= 1e-4f || surfaceRadius <= 1e-4f) {
      return false;
   }

   GameEngine::Vector3 simulatedPosition = position;
   GameEngine::Vector3 simulatedVelocity = velocity;

   // 軌道表示と接触判定が同じ積分結果を参照できるよう、初期点から記録する。
   outPrediction.trajectoryPoints.clear();
   outPrediction.trajectoryPoints.push_back(position);
   GameEngine::Vector3 initialToSelf = simulatedPosition - planetCenter;
   float initialDistance = initialToSelf.Length();

   if (initialDistance <= 1e-4f) {
      return false;
   }

   GameEngine::Vector3 initialUp = initialToSelf * (1.0f / initialDistance);

   // プレイヤー中心ではなく、回転中のOBB下面が地表へ触れる半径を判定境界にする。
   float initialSnapRadius =
      surfaceRadius + landingOffset + GameEngine::ComputeObbSupportRadius(playerRotation, obbHalfExtents, initialUp);
   float previousClearance = initialDistance - initialSnapRadius;

   if (previousClearance <= 0.0f) {
      return false;
   }

   float elapsed = 0.0f;

   while (elapsed < horizon) {
      float step = std::min(kLandingPredictionStepSeconds, horizon - elapsed);
      GameEngine::Vector3 toSelf = simulatedPosition - planetCenter;
      float distance = toSelf.Length();

      if (distance <= 1e-4f) {
         return false;
      }

      GameEngine::Vector3 surfaceUp = toSelf * (1.0f / distance);
      GameEngine::Vector3 previousVelocity = simulatedVelocity;
      GameEngine::Vector3 previousPosition = simulatedPosition;

      // GravityBody と同じ半陰的オイラー順序で、候補惑星の放射重力だけを短時間先読みする。
      simulatedVelocity = gravityBody.PredictVelocity(simulatedVelocity, surfaceUp, step);
      simulatedPosition += simulatedVelocity * step;
      outPrediction.trajectoryPoints.push_back(simulatedPosition);

      GameEngine::Vector3 nextToSelf = simulatedPosition - planetCenter;
      float nextDistance = nextToSelf.Length();

      if (nextDistance <= 1e-4f) {
         return false;
      }

      GameEngine::Vector3 nextUp = nextToSelf * (1.0f / nextDistance);
      float nextSnapRadius =
         surfaceRadius + landingOffset + GameEngine::ComputeObbSupportRadius(playerRotation, obbHalfExtents, nextUp);
      float nextClearance = nextDistance - nextSnapRadius;

      if (nextClearance <= 0.0f) {
         // 前後フレームの符号変化を線形補間し、固定刻みの内側まで接触時刻を詰める。
         float denominator = previousClearance - nextClearance;
         float crossingAlpha = denominator > 1e-5f ? std::clamp(previousClearance / denominator, 0.0f, 1.0f) : 1.0f;
         GameEngine::Vector3 hitPosition = previousPosition + (simulatedPosition - previousPosition) * crossingAlpha;
         GameEngine::Vector3 hitVelocity = previousVelocity + (simulatedVelocity - previousVelocity) * crossingAlpha;
         GameEngine::Vector3 hitUp = NormalizeOrFallback(hitPosition - planetCenter, nextUp);

         // 地表へ近づく交差だけを着地予測として扱う。
         if (hitVelocity.Dot(hitUp) > 0.0f) {
            return false;
         }

         GameEngine::Vector3 landingForward = hitVelocity - hitUp * hitVelocity.Dot(hitUp);

         if (landingForward.Length() <= kLandingPredictionDirectionEpsilon) {
            landingForward = fallbackForward - hitUp * fallbackForward.Dot(hitUp);
         }

         if (landingForward.Length() <= kLandingPredictionDirectionEpsilon) {
            // 真上・真下を向いた垂直着地では機首投影も退化するため、
            // 表示中のカメラRightから画面上の前方を復元して方位を維持する。
            landingForward = fallbackRight.Cross(hitUp);
         }

         if (landingForward.Length() <= kLandingPredictionDirectionEpsilon) {
            return false;
         }

         landingForward = NormalizeOrFallback(landingForward, fallbackForward);
         outPrediction.up = hitUp;
         outPrediction.backward = -landingForward;
         outPrediction.contactPoint = planetCenter + hitUp * (surfaceRadius + landingOffset);
         outPrediction.secondsToImpact = elapsed + step * crossingAlpha;
         outPrediction.trajectoryPoints.back() = hitPosition;
         return true;
      }

      previousClearance = nextClearance;
      elapsed += step;
   }

   return false;
}
}
