#pragma once
#include "GameEngine/Math/Types/Transform.h"

namespace App {
/// @brief プレイヤー前方へ惑星と旗を配置する姿勢を計算する。前方が退化する場合はfalse。
/// @param playerPose 現在のプレイヤー姿勢。
/// @param sourceCenter 現在いる惑星の中心。
/// @param sourceRadius 現在いる惑星の半径。
/// @param surfaceGap プレイヤーから目的地の地表まで確保する間隔。
/// @param targetPose 目標惑星の拡縮を入力し、配置位置を受け取る。
/// @param goalPose 旗の拡縮を入力し、配置位置と姿勢を受け取る。
bool PlaceTutorialDestination(const GameEngine::Transform& playerPose, const GameEngine::Vector3& sourceCenter,
   float sourceRadius, float surfaceGap, GameEngine::Transform& targetPose, GameEngine::Transform& goalPose);
}
