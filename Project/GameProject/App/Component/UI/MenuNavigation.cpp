#include "GameProject/App/Component/UI/MenuNavigation.h"
#include "GameProject/App/Component/UI/MenuSelectionAnimationComponent.h"

namespace App {
std::int32_t MenuNavigation::ReadDirection(float axis) {
   const std::int32_t direction = axis > 0.5f ? 1 : (axis < -0.5f ? -1 : 0);
   // スティックが中立に戻るまで、押し続けによる選択の往復を抑止する。
   if (direction == 0) {
      latched_ = false;
      return 0;
   }
   if (latched_) {
      return 0;
   }
   latched_ = true;
   return direction;
}

bool MenuNavigation::Move(const MenuSelectionAnimationComponent& animation, std::int32_t direction) {
   if (direction == 0) {
      return false;
   }
   const auto count = static_cast<std::int32_t>(animation.GetOptionCount());
   if (count == 0) {
      return false;
   }
   const auto previous = selected_;
   auto candidate = selected_ >= 0 && selected_ < count ? selected_ : (direction > 0 ? -1 : 0);
   direction = direction > 0 ? 1 : -1;
   // 端では巡回し、未解決の項目を飛ばす。全項目が欠ける場合も必ず終了する。
   for (std::int32_t attempt = 0; attempt < count; ++attempt) {
      candidate = (candidate + direction + count) % count;
      if (animation.IsOptionAvailable(static_cast<std::size_t>(candidate))) {
         selected_ = candidate;
         return selected_ != previous;
      }
   }
   return false;
}
}
