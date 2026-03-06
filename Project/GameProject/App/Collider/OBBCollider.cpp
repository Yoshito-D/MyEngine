#include "OBBCollider.h"
#include "SphereCollider.h"
#include "../GameObject/GameObject.h"
#include "Utility/MathUtils.h"
#include <algorithm>
#include <cmath>

using namespace GameEngine;

OBBCollider::OBBCollider(GameObject* owner, const Vector3& size)
   : Collider(), size_(size) {
   owner_ = owner;
   type_ = ColliderType::OBB;
}

Quaternion OBBCollider::GetOrientation() const {
   if (owner_ && owner_->GetModel()) {
      return owner_->GetModel()->GetRotationQuaternion();
   }
   return Quaternion::Identity();
}

void OBBCollider::GetAxes(Vector3& axisX, Vector3& axisY, Vector3& axisZ) const {
   Quaternion orientation = GetOrientation();
   
   // クォータニオンから各軸を計算
   axisX = RotateVector(Vector3(1.0f, 0.0f, 0.0f), orientation);
   axisY = RotateVector(Vector3(0.0f, 1.0f, 0.0f), orientation);
   axisZ = RotateVector(Vector3(0.0f, 0.0f, 1.0f), orientation);
}

bool OBBCollider::CheckCollision(Collider* other) const {
   if (!other) return false;
   
   switch (other->GetType()) {
      case ColliderType::Sphere: {
         SphereCollider* sphere = static_cast<SphereCollider*>(other);
         
         // OBBと球の当たり判定
         Vector3 obbCenter = GetPosition();
         Vector3 sphereCenter = sphere->GetPosition();
         
         // OBBのローカル空間に球の中心を変換
         Vector3 diff = sphereCenter - obbCenter;
         
         Vector3 axisX, axisY, axisZ;
         GetAxes(axisX, axisY, axisZ);
         
         // 各軸への投影
         float projX = diff.Dot(axisX);
         float projY = diff.Dot(axisY);
         float projZ = diff.Dot(axisZ);
         
         // 最近接点を計算
         float closestX = std::clamp(projX, -size_.x, size_.x);
         float closestY = std::clamp(projY, -size_.y, size_.y);
         float closestZ = std::clamp(projZ, -size_.z, size_.z);
         
         // 最近接点をワールド座標に戻す
         Vector3 closestPoint = obbCenter + axisX * closestX + axisY * closestY + axisZ * closestZ;
         
         // 球の中心から最近接点までの距離
         float distanceSq = (sphereCenter - closestPoint).LengthSquared();
         float radiusSq = sphere->GetRadius() * sphere->GetRadius();
         
         return distanceSq <= radiusSq;
      }
      
      case ColliderType::OBB: {
         // OBB同士の当たり判定（分離軸定理）
         OBBCollider* otherOBB = static_cast<OBBCollider*>(other);
         
         Vector3 center1 = GetPosition();
         Vector3 center2 = otherOBB->GetPosition();
         
         Vector3 axis1X, axis1Y, axis1Z;
         Vector3 axis2X, axis2Y, axis2Z;
         GetAxes(axis1X, axis1Y, axis1Z);
         otherOBB->GetAxes(axis2X, axis2Y, axis2Z);
         
         Vector3 size1 = size_;
         Vector3 size2 = otherOBB->GetSize();
         
         // 中心間のベクトル
         Vector3 diff = center2 - center1;
         
         // 15個の分離軸をテスト
         Vector3 axes[15] = {
            axis1X, axis1Y, axis1Z,  // OBB1の3軸
            axis2X, axis2Y, axis2Z,  // OBB2の3軸
            axis1X.Cross(axis2X), axis1X.Cross(axis2Y), axis1X.Cross(axis2Z),  // 外積軸
            axis1Y.Cross(axis2X), axis1Y.Cross(axis2Y), axis1Y.Cross(axis2Z),
            axis1Z.Cross(axis2X), axis1Z.Cross(axis2Y), axis1Z.Cross(axis2Z)
         };
         
         for (int i = 0; i < 15; ++i) {
            Vector3 axis = axes[i];
            
            // 外積がゼロベクトルの場合はスキップ
            if (axis.LengthSquared() < 0.0001f) continue;
            
            axis = axis.Normalize();
            
            // 各OBBの投影半径を計算
            float r1 = std::abs(size1.x * axis.Dot(axis1X)) +
                      std::abs(size1.y * axis.Dot(axis1Y)) +
                      std::abs(size1.z * axis.Dot(axis1Z));
            
            float r2 = std::abs(size2.x * axis.Dot(axis2X)) +
                      std::abs(size2.y * axis.Dot(axis2Y)) +
                      std::abs(size2.z * axis.Dot(axis2Z));
            
            // 中心間距離の投影
            float distance = std::abs(diff.Dot(axis));
            
            // 分離軸が見つかった
            if (distance > r1 + r2) {
               return false;
            }
         }
         
         // すべての軸で重なっている
         return true;
      }
      
      default:
         return false;
   }
}
