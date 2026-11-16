#pragma once

#include "GameEngine/Graphics/Renderer/DrawCommand.h"
#include <array>
#include <span>
#include <vector>

namespace GameEngine {
/// @brief フレーム内の描画要求を値として所有し、描画段階ごとの順序を管理する。
class RenderQueue {
public:
   /// @brief 全描画段階の要求を破棄する。配列の確保済み容量は次フレームに再利用する。
   void Clear();

   /// @brief 完成済みコマンドを対応する描画段階へ移す。不正な段階なら登録せずfalseを返す。
   bool Enqueue(DrawCommand command);

   /// @brief 指定段階の読み取り専用コマンド列を取得する。不正な段階なら空。
   /// @note 同じ段階への追加・ソート・Clearで要素の参照が失効する。
   std::span<const DrawCommand> GetCommands(RenderPass renderPass) const;

   /// @brief 半透明要求を奥から手前へ安定ソートする。位置がない要素は種別優先度に従う。
   void SortTransparent();

private:
   std::array<std::vector<DrawCommand>, static_cast<size_t>(RenderPass::Count)> commands_; ///< 段階別の所有配列。
};
} // namespace GameEngine
