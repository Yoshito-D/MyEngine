#pragma once

#include <cstdint>

namespace App {
class MenuSelectionAnimationComponent;

/// @brief メニューの選択位置と入力の押しっぱなし抑制を管理する。
class MenuNavigation {
public:
   /// @brief 選択位置を変更する。負値は未選択を表す。
   void Select(std::int32_t index) { selected_ = index; }

   /// @brief 現在の選択位置を返す。
   std::int32_t GetSelectedIndex() const { return selected_; }

   /// @brief 次の方向入力を受け付けるようラッチを解除する。
   void ResetInput() { latched_ = false; }

   /// @brief 方向入力が中立から閾値を超えたときだけ-1または1を返す。
   std::int32_t ReadDirection(float axis);

   /// @brief 利用可能な項目へ巡回し、選択が変わった場合trueを返す。
   bool Move(const MenuSelectionAnimationComponent& animation, std::int32_t direction);

private:
   std::int32_t selected_ = -1; ///< 選択中の項目。
   bool latched_ = false;       ///< 方向入力が中立に戻るまで再入力を抑止する。
};
}
