#pragma once

#include "Component/IObjectComponent.h"
#include "Core/UI/Text/TextTypes.h"
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>

namespace GameEngine {
class FontManager;

/// @brief UTF-8文字列とUI表示設定を保持するコンポーネント
class UITextComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "UITextComponent";
   static constexpr ComponentDisplayName kDisplayName{ "UIテキスト", "UI Text" };
   static constexpr size_t kShowAllGlyphs = (std::numeric_limits<size_t>::max)();

   /// @brief コンポーネントの型名を取得する
   /// @return 型名
   const char* GetTypeName() const override;

   /// @brief 表示文字列をUTF-8で設定する
   /// @param text UTF-8文字列
   void SetText(std::string text);

   /// @brief char8_tのUTF-8文字列を設定する
   /// @param text UTF-8文字列
   void SetText(std::u8string_view text);

   /// @brief 表示文字列を取得する
   /// @return UTF-8文字列
   const std::string& GetText() const { return text_; }

   /// @brief 文字の表示設定を一括で設定する
   /// @param style 表示設定
   void SetStyle(const TextStyle& style);

   /// @brief 現在の表示設定を取得する
   /// @return 表示設定
   const TextStyle& GetStyle() const { return style_; }

   /// @brief 使用する登録済みフォントIDを設定する
   /// @param fontId FontManagerに登録したID
   void SetFontId(std::string fontId);

   /// @brief 文字サイズをピクセル単位で設定する
   /// @param fontSize 1以上のピクセルサイズ
   void SetFontSize(uint32_t fontSize);

   /// @brief 文字色を設定する
   /// @param color RGBA色
   void SetColor(const Vector4& color);

   /// @brief 不透明度だけを設定する
   /// @param opacity 0から1の不透明度
   void SetOpacity(float opacity);

   /// @brief 先頭から描画する文字数を設定する
   /// @param glyphCount 表示するグリフ数。kShowAllGlyphsで全表示
   void SetVisibleGlyphCount(size_t glyphCount) { visibleGlyphCount_ = glyphCount; }

   /// @brief 先頭から描画する文字数を取得する
   /// @return グリフ数
   size_t GetVisibleGlyphCount() const { return visibleGlyphCount_; }

   /// @brief キャッシュ済みレイアウトを取得する
   /// @param fontManager フォント管理
   /// @return 描画用レイアウト
   const TextLayoutResult& GetLayout(FontManager& fontManager) const;

   /// @brief 文字列更新を検知するためのリビジョンを取得する
   /// @return 更新ごとに増加する値
   uint64_t GetTextRevision() const { return textRevision_; }

   /// @brief 設定をJSON化する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから設定を復元する
   /// @param data 保存済みJSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief エディタのインスペクターを描画する
   void DrawInspector() override;
#endif

private:
   void InvalidateLayout();

   std::string text_;
   TextStyle style_{};
   size_t visibleGlyphCount_ = kShowAllGlyphs;
   uint64_t textRevision_ = 0;
   mutable bool layoutDirty_ = true;
   mutable uint64_t layoutFontRevision_ = 0;
   mutable TextLayoutResult cachedLayout_{};
};

} // namespace GameEngine
