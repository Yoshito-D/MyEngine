#pragma once
#include "Utility/Math/Vector3.h"

class Planet;
class TPSCameraController;

/// @brief カメラに基づいた入力を接平面方向に変換するヘルパー
class CameraInputHelper {
public:
   CameraInputHelper() = default;
   ~CameraInputHelper() = default;

   /// @brief カメラの方向と入力から接平面上の移動方向を計算
   /// @param inputX 左右入力（-1.0 ～ 1.0）
   /// @param inputZ 前後入力（-1.0 ～ 1.0）
   /// @param headDirection 頭の方向（惑星の法線方向）
   /// @param cameraForward カメラの前方向
   /// @param cameraRight カメラの右方向
   /// @return 正規化された接平面上の移動方向（入力がない場合はゼロベクトル）
   static GameEngine::Vector3 CalculateTangentMoveDirection(
      float inputX,
      float inputZ,
      const GameEngine::Vector3& headDirection,
      const GameEngine::Vector3& cameraForward,
      const GameEngine::Vector3& cameraRight
   );

   /// @brief カメラコントローラーを使用して接平面上の移動方向を計算
   /// @param inputX 左右入力（-1.0 ～ 1.0）
   /// @param inputZ 前後入力（-1.0 ～ 1.0）
   /// @param headDirection 頭の方向（惑星の法線方向）
   /// @param cameraController カメラコントローラー
   /// @return 正規化された接平面上の移動方向（入力がない場合はゼロベクトル）
   static GameEngine::Vector3 CalculateTangentMoveDirection(
      float inputX,
      float inputZ,
      const GameEngine::Vector3& headDirection,
      TPSCameraController* cameraController
   );

private:
   /// @brief ベクトルを接平面に投影
   /// @param vector 投影するベクトル
   /// @param normal 接平面の法線
   /// @return 投影されたベクトル
   static GameEngine::Vector3 ProjectToTangentPlane(const GameEngine::Vector3& vector, const GameEngine::Vector3& normal);
};
