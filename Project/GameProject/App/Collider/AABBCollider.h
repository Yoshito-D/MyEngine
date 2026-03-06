#pragma once
#include "Collider.h"

class AABBCollider : public Collider {
public:
   AABBCollider(GameObject* owner, const GameEngine::Vector3& size);

   bool CheckCollision(Collider* other) const override;

   GameEngine::Vector3 GetMax() const;
   GameEngine::Vector3 GetMin() const;

   void SetSize(const GameEngine::Vector3& size)override { size_ = size; }
private:
   GameEngine::Vector3 size_;
};
