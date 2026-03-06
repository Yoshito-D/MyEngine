#include "CollisionConfig.h"

CollisionConfig::CollisionConfig() {
   // デフォルトはすべて true（必要に応じて個別設定）
   for (int i = 0; i < kMaxLayers; ++i) {
      for (int j = 0; j < kMaxLayers; ++j) {
         matrix_[i][j] = true;
      }
   }
}

void CollisionConfig::SetCollisionEnabled(CollisionLayer a, CollisionLayer b, bool enable) {
   int ia = static_cast<int>(a);
   int ib = static_cast<int>(b);
   matrix_[ia][ib] = enable;
   matrix_[ib][ia] = enable;
}

bool CollisionConfig::IsCollisionEnabled(CollisionLayer a, CollisionLayer b) const {
   int ia = static_cast<int>(a);
   int ib = static_cast<int>(b);
   return matrix_[ia][ib];
}
