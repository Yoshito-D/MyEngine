#include "CameraInputHelper.h"
#include "../Camera/TPSCameraController.h"

using namespace GameEngine;

Vector3 CameraInputHelper::ProjectToTangentPlane(const Vector3& vector, const Vector3& normal) {
   Vector3 tangent = vector - normal * normal.Dot(vector);
   
   if (tangent.Length() > 0.001f) {
      return tangent.Normalize();
   }
   
   return Vector3(0.0f, 0.0f, 0.0f);
}

Vector3 CameraInputHelper::CalculateTangentMoveDirection(
   float inputX,
   float inputZ,
   const Vector3& headDirection,
   const Vector3& cameraForward,
   const Vector3& cameraRight
) {
   if (inputX == 0.0f && inputZ == 0.0f) {
      return Vector3(0.0f, 0.0f, 0.0f);
   }

   // カメラの前方向と右方向を接平面に投影
   Vector3 tangentForward = ProjectToTangentPlane(cameraForward, headDirection);
   Vector3 tangentRight = ProjectToTangentPlane(cameraRight, headDirection);

   // フォールバック処理
   if (tangentForward.Length() < 0.001f) {
      tangentForward = ProjectToTangentPlane(cameraRight, headDirection);
      if (tangentForward.Length() < 0.001f) {
         return Vector3(0.0f, 0.0f, 0.0f);
      }
   }

   if (tangentRight.Length() < 0.001f) {
      tangentRight = headDirection.Cross(tangentForward).Normalize();
   }

   // 移動方向を計算（カメラの前方向がZ、右方向がX）
   Vector3 moveDir = (tangentForward * inputZ + tangentRight * inputX);
   
   if (moveDir.Length() > 0.001f) {
      return moveDir.Normalize();
   }

   return Vector3(0.0f, 0.0f, 0.0f);
}

Vector3 CameraInputHelper::CalculateTangentMoveDirection(
   float inputX,
   float inputZ,
   const Vector3& headDirection,
   TPSCameraController* cameraController
) {
   if (!cameraController) {
      // カメラコントローラーがない場合はワールド座標系を使用
      return CalculateTangentMoveDirection(
         inputX,
         inputZ,
         headDirection,
         Vector3(0.0f, 0.0f, 1.0f),  // デフォルトの前方向
         Vector3(1.0f, 0.0f, 0.0f)   // デフォルトの右方向
      );
   }

   Vector3 cameraForward = cameraController->GetCameraForward();
   Vector3 cameraRight = cameraController->GetCameraRight();

   return CalculateTangentMoveDirection(inputX, inputZ, headDirection, cameraForward, cameraRight);
}
