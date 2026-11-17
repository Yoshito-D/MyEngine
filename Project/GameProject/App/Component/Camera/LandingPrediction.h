#pragma once
#include "GameEngine/Math/Types/Vector3.h"
#include "GameEngine/Math/Types/Quaternion.h"
#include <vector>

namespace App {
class GravityBody;

/// @brief 着地予測で得た接触姿勢・時刻と表示用の軌道。
struct LandingPrediction {
   GameEngine::Vector3 up = {0.0f, 1.0f, 0.0f};           ///< 着地面の法線に沿った上方向。
   GameEngine::Vector3 backward = {0.0f, 0.0f, -1.0f};    ///< 着地後の追従カメラを配置する後方方向。
   GameEngine::Vector3 contactPoint = {0.0f, 0.0f, 0.0f}; ///< 着地を予測した地表上の接触点。
   float secondsToImpact = 0.0f;                          ///< 予測した地表接触までの残り時間（秒）。
   std::vector<GameEngine::Vector3> trajectoryPoints;     ///< 着地予測でサンプリングした移動軌道の位置列。
};

/// @brief 現在の機体姿勢と重力先を固定し、重力・空中減速・機体の形状から地表への接触姿勢・時刻を予測する。
/// @param airDrag 空中移動に適用する水平減速係数（1/s）。減速しない場合は0を渡す。
bool PredictLanding(const GameEngine::Vector3& position, const GameEngine::Vector3& velocity,
   const GameEngine::Vector3& planetCenter, float surfaceRadius, float landingOffset,
   const GameEngine::Vector3& obbHalfExtents, const GameEngine::Quaternion& playerRotation,
   const GravityBody& gravityBody, float airDrag, float predictionHorizon, const GameEngine::Vector3& fallbackForward,
   const GameEngine::Vector3& fallbackRight, LandingPrediction& outPrediction);

}
