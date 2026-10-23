#pragma once

#include "GameEngine/UI/Text/TextTypes.h"
#include <cstddef>
#include <string_view>

namespace GameEngine {
class FontManager;

/// @brief UTF-8文字列を画面描画可能なグリフ列へ変換する
class TextLayout {
public:
   /// @brief テキストを指定スタイルでレイアウトする
   /// @param fontManager グリフとフォントメトリクスの供給元
   /// @param text UTF-8文字列
   /// @param style 表示設定
   /// @return ローカル座標上のレイアウト結果
   static TextLayoutResult Build(FontManager& fontManager, std::string_view text, const TextStyle& style);

private:
   /// @brief 横方向の整列に使う、1行分のグリフ範囲と幅。
   struct LineRange {
      size_t begin = 0;   ///< 行に含まれる最初のグリフの番号。
      size_t end = 0;     ///< 行に含まれる最後のグリフの次の番号。
      float width = 0.0f; ///< 行の横幅（ピクセル）。
   };
};

} // namespace GameEngine
