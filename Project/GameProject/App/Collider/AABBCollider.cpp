#include "AABBCollider.h"
#include "SphereCollider.h"
#include "Collision.h"

using namespace GameEngine;

AABBCollider::AABBCollider(GameObject* owner, const Vector3& size) {
   owner_ = owner;
   size_ = size;
   type_ = ColliderType::AABB;
}

bool AABBCollider::CheckCollision(Collider* other) const {
   if (other->GetType() == ColliderType::Sphere) {
	  const SphereCollider& s = static_cast<const SphereCollider&>(*other);
	  GameEngine::Collider::AABB aabb = { GetMin(),GetMax() };
	  GameEngine::Collider::Sphere sphere = { s.GetPosition(), s.GetRadius() };
	  return GameEngine::Collision::IsCollision(aabb, sphere);
   } else if (other->GetType() == ColliderType::AABB) {
	  const AABBCollider& a = static_cast<const AABBCollider&>(*other);
	  GameEngine::Collider::AABB aabb1 = { GetMin(),GetMax() };
	  GameEngine::Collider::AABB aabb2 = { a.GetMin(), a.GetMax() };
	  return GameEngine::Collision::IsCollision(aabb1, aabb2);
   }

   return false;
}

Vector3 AABBCollider::GetMax() const { return GetPosition() + size_ * 0.5f; }

Vector3 AABBCollider::GetMin() const { return GetPosition() - size_ * 0.5f; }
