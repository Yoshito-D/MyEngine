#pragma once

#include "TextTypes.h"
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
};

} // namespace GameEngine
