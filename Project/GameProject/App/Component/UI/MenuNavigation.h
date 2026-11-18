#pragma once

#include <cstddef>
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

   /// @brief 先頭から指定した数までの項目で巡回し、選択が変わった場合trueを返す。
   /// @param animation 項目の参照と有効状態を持つ演出コンポーネント。
   /// @param direction 巡回する方向。0では移動しない。
   /// @param optionLimit 先頭から選択候補に含める最大項目数。
   bool Move(const MenuSelectionAnimationComponent& animation, std::int32_t direction, std::size_t optionLimit);

private:
   std::int32_t selected_ = -1; ///< 選択中の項目。
   bool latched_ = false;       ///< 方向入力が中立に戻るまで再入力を抑止する。
};
}
