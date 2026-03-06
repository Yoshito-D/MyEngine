#pragma once
#include <vector>
#include <unordered_set>
#include "Collider.h"
#include "CollisionConfig.h"

class CollisionManager {
public:
   explicit CollisionManager(CollisionConfig* config);
   ~CollisionManager() = default;

   void RegisterCollider(Collider* collider);
   void CheckAllCollisions();
   void Clear();

   struct ColliderPairHash {
      size_t operator()(const std::pair<Collider*, Collider*>& p) const noexcept {
         return reinterpret_cast<size_t>(p.first) ^ reinterpret_cast<size_t>(p.second);
      }
   };

private:
   std::vector<Collider*> colliders_;
   CollisionConfig* config_ = nullptr;

   std::unordered_set<std::pair<Collider*, Collider*>, ColliderPairHash> previousCollisions_;
};
