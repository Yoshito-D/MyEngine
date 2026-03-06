#include "SphereCollider.h"
#include "AABBCollider.h"
#include "OBBCollider.h"
#include "Collision.h"
#include "Utility/MathUtils.h"
#include <algorithm>
#include <cmath>

using namespace GameEngine;

SphereCollider::SphereCollider(GameObject* owner, float r) {
   type_ = ColliderType::Sphere;
   owner_ = owner;
   radius_ = r;
}

bool SphereCollider::CheckCollision(Collider* other) const {
   if (other->GetType() == ColliderType::Sphere) {
	  const SphereCollider& s = static_cast<const SphereCollider&>(*other);
	   GameEngine::Collider::Sphere sphere1 = { GetPosition(), radius_ };
	   GameEngine::Collider::Sphere sphere2 = { s.GetPosition(), s.radius_ };
	  return  GameEngine::Collision::IsCollision(sphere1, sphere2);
   } else if (other->GetType() == ColliderType::AABB) {
	  const AABBCollider& a = static_cast<const AABBCollider&>(*other);
	   GameEngine::Collider::Sphere sphere = { GetPosition(), radius_ };
	   GameEngine::Collider::AABB aabb = { a.GetMin(), a.GetMax() };
	  return  GameEngine::Collision::IsCollision(aabb, sphere);
   } else if (other->GetType() == ColliderType::OBB) {
	  // OBBと球の当たり判定
	  OBBCollider* obb = static_cast<OBBCollider*>(other);
	  
	  Vector3 obbCenter = obb->GetPosition();
	  Vector3 sphereCenter = GetPosition();
	  
	  // OBBのローカル空間に球の中心を変換
	  Vector3 diff = sphereCenter - obbCenter;
	  
	  Vector3 axisX, axisY, axisZ;
	  obb->GetAxes(axisX, axisY, axisZ);
	  Vector3 size = obb->GetSize();
	  
	  // 各軸への投影
	  float projX = diff.Dot(axisX);
	  float projY = diff.Dot(axisY);
	  float projZ = diff.Dot(axisZ);
	  
	  // 最近接点を計算
	  float closestX = std::clamp(projX, -size.x, size.x);
	  float closestY = std::clamp(projY, -size.y, size.y);
	  float closestZ = std::clamp(projZ, -size.z, size.z);
	  
	  // 最近接点をワールド座標に戻す
	  Vector3 closestPoint = obbCenter + axisX * closestX + axisY * closestY + axisZ * closestZ;
	  
	  // 球の中心から最近接点までの距離
	  float distanceSq = (sphereCenter - closestPoint).LengthSquared();
	  float radiusSq = radius_ * radius_;
	  
	  return distanceSq <= radiusSq;
   }

   return false;
}
