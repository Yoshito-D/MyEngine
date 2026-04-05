#pragma once
#include "IObjectComponent.h"
#include "Collision/Collider.h"

namespace GameEngine {
class ColliderComponent final : public IObjectComponent {
public:
   enum class ShapeType {
      None,
      Sphere,
      AABB,
      Capsule,
   };

   bool enabled = true;
   ShapeType shapeType = ShapeType::None;
   Collider::Sphere sphere = {};
   Collider::AABB aabb = {};
   Collider::Capsule capsule = {};
};
}
