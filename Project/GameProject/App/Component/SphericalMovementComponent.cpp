#include "SphericalMovementComponent.h"
#include "../GameObject/Planet/Planet.h"
#include "Utility/MathUtils.h"
#include <cmath>
#include <numbers>
#include <algorithm>

using namespace GameEngine;

void SphericalMovementComponent::Initialize(const Vector3& initialPosition, Planet* planet, float radius) {
   currentPlanet_ = planet;
   currentRadius_ = radius;

   if (planet) {
      Vector3 planetCenter = planet->GetWorldPosition();
      Vector3 toPlanet = (planetCenter - initialPosition).Normalize();
      Vector3 headDir = toPlanet * -1.0f;

      // 初期位置用Quaternionを計算
      Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
      float dot = up.Dot(headDir);

      if (std::abs(dot - 1.0f) < 0.001f) {
         positionQuaternion_ = Quaternion::Identity();
      } else if (std::abs(dot + 1.0f) < 0.001f) {
         positionQuaternion_ = MakeRotateAxisAngleQuaternion(Vector3(1.0f, 0.0f, 0.0f), std::numbers::pi_v<float>);
      } else {
         Vector3 axis = up.Cross(headDir).Normalize();
         float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));
         positionQuaternion_ = MakeRotateAxisAngleQuaternion(axis, angle);
      }
   } else {
      positionQuaternion_ = Quaternion::Identity();
   }

   orientationQuaternion_ = Quaternion::Identity();
}

void SphericalMovementComponent::Move(const Vector3& tangentDirection, float moveDistance) {
   if (!currentPlanet_ || tangentDirection.Length() < 0.001f) return;

   Vector3 headDir = GetHeadDirection();

   // 接平面に投影
   Vector3 tangentDir = tangentDirection - headDir * headDir.Dot(tangentDirection);

   if (tangentDir.Length() < 0.001f) {
      return;
   }

   tangentDir = tangentDir.Normalize();

   // 回転角度を計算
   float angleToRotate = moveDistance / currentRadius_;

   // 回転軸を計算
   Vector3 rotationAxis = headDir.Cross(tangentDir).Normalize();

   if (rotationAxis.Length() > 0.001f) {
      // ワールド座標系での回転（左から乗算）
      Quaternion deltaQ = MakeRotateAxisAngleQuaternion(rotationAxis, angleToRotate);
      positionQuaternion_ = (deltaQ * positionQuaternion_).Normalize();
   }
}

void SphericalMovementComponent::UpdateModelPosition(GameEngine::Model* model) {
   if (!model || !currentPlanet_) return;

   // 惑星の現在位置を取得（動いている惑星にも対応）
   Vector3 planetCenter = currentPlanet_->GetWorldPosition();
   
   // ローカル座標系での上方向ベクトル
   Vector3 localUp = Vector3(0.0f, currentRadius_, 0.0f);
   
   // positionQuaternionで回転してワールド座標での位置オフセットを計算
   Vector3 worldPosition = RotateVector(localUp, positionQuaternion_);

   // 惑星の中心位置にオフセットを加えてモデルの最終位置を設定
   model->SetPosition(planetCenter + worldPosition);
}

void SphericalMovementComponent::UpdateOrientation(const Vector3& targetTangentDirection, float turnSpeed, float dt, bool immediate) {
   if (!currentPlanet_ || targetTangentDirection.Length() < 0.001f) return;

   Vector3 headDir = GetHeadDirection();

   // 目標方向を接平面に投影
   Vector3 targetTangent = targetTangentDirection - headDir * headDir.Dot(targetTangentDirection);

   if (targetTangent.Length() > 0.001f) {
      targetTangent = targetTangent.Normalize();

      // positionQuaternionの逆回転で目標方向をローカル座標系に変換
      Quaternion invPositionQuat = positionQuaternion_.Conjugate();
      Vector3 localTargetDir = RotateVector(targetTangent, invPositionQuat);

      // ローカル座標系でXZ平面に投影（Y成分を0にする）
      localTargetDir.y = 0.0f;

      if (localTargetDir.Length() > 0.001f) {
         localTargetDir = localTargetDir.Normalize();

         // ローカルZ軸（前方向）
         Vector3 localForward = Vector3(0.0f, 0.0f, 1.0f);

         // 2つのベクトル間の回転を計算
         float dotProduct = std::clamp(localForward.Dot(localTargetDir), -1.0f, 1.0f);
         float angle = std::acos(dotProduct);

         // 回転軸はY軸（上方向）
         Vector3 cross = localForward.Cross(localTargetDir);
         float direction = (cross.y >= 0.0f) ? 1.0f : -1.0f;

         // 目標Quaternionを計算
         Quaternion targetOrientation = MakeRotateAxisAngleQuaternion(
            Vector3(0.0f, 1.0f, 0.0f),
            angle * direction
         );

         if (immediate) {
            // 即座に向きを変更
            orientationQuaternion_ = targetOrientation;
         } else {
            // 最短経路を選択: Quaternionの内積が負の場合、符号を反転
            float quatDot = orientationQuaternion_.x * targetOrientation.x +
               orientationQuaternion_.y * targetOrientation.y +
               orientationQuaternion_.z * targetOrientation.z +
               orientationQuaternion_.w * targetOrientation.w;

            if (quatDot < 0.0f) {
               // 符号を反転して最短経路にする
               targetOrientation.x = -targetOrientation.x;
               targetOrientation.y = -targetOrientation.y;
               targetOrientation.z = -targetOrientation.z;
               targetOrientation.w = -targetOrientation.w;
               quatDot = -quatDot;
            }

            // 現在のorientationQuaternionから目標へSlerp補間
            if (angle > 0.01f) {
               float maxRotation = turnSpeed * dt;

               // 2つのQuaternion間の角度を計算
               float quatAngle = std::acos(std::clamp(std::abs(quatDot), 0.0f, 1.0f)) * 2.0f;

               if (quatAngle <= maxRotation || quatAngle < 0.001f) {
                  // 目標に到達できる場合は直接設定
                  orientationQuaternion_ = targetOrientation;
               } else {
                  // Slerp補間
                  float t = maxRotation / quatAngle;

                  // 正しいSlerp計算
                  float sinHalfAngle = std::sqrt(1.0f - quatDot * quatDot);

                  if (sinHalfAngle > 0.001f) {
                     // 通常のSlerp
                     float halfAngle = std::acos(quatDot);
                     float ratioA = std::sin((1.0f - t) * halfAngle) / sinHalfAngle;
                     float ratioB = std::sin(t * halfAngle) / sinHalfAngle;

                     orientationQuaternion_.x = orientationQuaternion_.x * ratioA + targetOrientation.x * ratioB;
                     orientationQuaternion_.y = orientationQuaternion_.y * ratioA + targetOrientation.y * ratioB;
                     orientationQuaternion_.z = orientationQuaternion_.z * ratioA + targetOrientation.z * ratioB;
                     orientationQuaternion_.w = orientationQuaternion_.w * ratioA + targetOrientation.w * ratioB;

                     orientationQuaternion_ = orientationQuaternion_.Normalize();
                  } else {
                     // ほぼ同じ向きの場合は線形補間
                     orientationQuaternion_.x = orientationQuaternion_.x * (1.0f - t) + targetOrientation.x * t;
                     orientationQuaternion_.y = orientationQuaternion_.y * (1.0f - t) + targetOrientation.y * t;
                     orientationQuaternion_.z = orientationQuaternion_.z * (1.0f - t) + targetOrientation.z * t;
                     orientationQuaternion_.w = orientationQuaternion_.w * (1.0f - t) + targetOrientation.w * t;

                     orientationQuaternion_ = orientationQuaternion_.Normalize();
                  }
               }
            }
         }
      }
   }
}

void SphericalMovementComponent::ApplyRotationToModel(GameEngine::Model* model) {
   if (!model) return;

   // positionQuaternionとorientationQuaternionを合成してモデルに適用
   Quaternion finalQuaternion = positionQuaternion_ * orientationQuaternion_;
   model->SetRotationQuaternion(finalQuaternion);
}

Vector3 SphericalMovementComponent::GetHeadDirection() const {
   Vector3 localUp = Vector3(0.0f, 1.0f, 0.0f);
   return RotateVector(localUp, positionQuaternion_);
}

Vector3 SphericalMovementComponent::GetForwardDirection() const {
   Vector3 localForward = Vector3(0.0f, 0.0f, 1.0f);
   Quaternion combinedQuat = positionQuaternion_ * orientationQuaternion_;
   return RotateVector(localForward, combinedQuat);
}
