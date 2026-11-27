#pragma once
#include "GameEngine/Math/Types/Transform.h"

namespace App {
/// @brief プレイヤー前方へ惑星と旗を配置する姿勢を計算する。前方が退化する場合はfalse。
/// @param playerPose 現在のプレイヤー姿勢。
/// @param sourceCenter 現在いる惑星の中心。
/// @param sourceRadius 現在いる惑星の半径。
/// @param surfaceGap プレイヤーから目的地の地表まで確保する間隔。
/// @param targetRadius 目標惑星のワールド半径（m）。
/// @param targetPose 目標惑星のワールド位置を受け取る。
/// @param goalPose 旗のワールド位置と姿勢を受け取る。
bool PlaceTutorialDestination(const GameEngine::Transform& playerPose, const GameEngine::Vector3& sourceCenter,
   float sourceRadius, float targetRadius, float surfaceGap, GameEngine::Transform& targetPose, GameEngine::Transform& goalPose);
}
