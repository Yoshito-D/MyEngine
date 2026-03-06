#pragma once
#include <array>
#include "CollisionLayer.h"

class CollisionConfig {
public:
   CollisionConfig();

   // 衝突可否を設定
   void SetCollisionEnabled(CollisionLayer a, CollisionLayer b, bool enable);

   // 衝突可能かどうかを確認
   bool IsCollisionEnabled(CollisionLayer a, CollisionLayer b) const;

private:
   static constexpr int kMaxLayers = static_cast<int>(CollisionLayer::Count);
   std::array<std::array<bool, kMaxLayers>, kMaxLayers> matrix_{};
};
