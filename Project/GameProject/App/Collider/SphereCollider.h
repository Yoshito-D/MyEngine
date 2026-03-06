#pragma once
#include "Collider.h"

class SphereCollider : public Collider {
public:
   SphereCollider(GameObject* owner, float r);

   bool CheckCollision(Collider* other) const override;

   float GetRadius() const { return radius_; }

   void SetRadius(float radius) override { radius_ = radius; }
private:
   float radius_;
};
