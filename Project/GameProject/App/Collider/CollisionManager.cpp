#include "CollisionManager.h"
#include <algorithm>

namespace {
   std::pair<Collider*, Collider*> MakePair(Collider* a, Collider* b) {
      return (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
   }
}

CollisionManager::CollisionManager(CollisionConfig* config)
   : config_(config) {
}

void CollisionManager::RegisterCollider(Collider* collider) {
   if (collider) colliders_.push_back(collider);
}

void CollisionManager::CheckAllCollisions() {
   std::unordered_set<std::pair<Collider*, Collider*>, ColliderPairHash> currentCollisions;

   for (size_t i = 0; i < colliders_.size(); ++i) {
      for (size_t j = i + 1; j < colliders_.size(); ++j) {
         Collider* a = colliders_[i];
         Collider* b = colliders_[j];

		 if (!a || !b) continue;
		 
		 // 無効なコライダーはスキップ
		 if (!a->IsEnabled() || !b->IsEnabled()) continue;
		 
         // マスク判定
         if (!config_->IsCollisionEnabled(a->GetLayer(), b->GetLayer())) continue;

         auto pair = MakePair(a, b);
         bool isColliding = a->CheckCollision(b);

         if (isColliding) {
            currentCollisions.insert(pair);

            if (previousCollisions_.find(pair) == previousCollisions_.end()) {
               // 当たった瞬間
               a->OnCollisionEnter(b);
               b->OnCollisionEnter(a);
            } else {
               // 当たっている間
               a->OnCollisionStay(b);
               b->OnCollisionStay(a);
            }
         } else {
            if (previousCollisions_.find(pair) != previousCollisions_.end()) {
               // 離れた瞬間
               a->OnCollisionExit(b);
               b->OnCollisionExit(a);
            }
         }
      }
   }

   previousCollisions_ = std::move(currentCollisions);
}

void CollisionManager::Clear() {
   colliders_.clear();
}
