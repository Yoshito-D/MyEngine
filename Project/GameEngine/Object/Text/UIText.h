#pragma once

#include "Core/UI/Text/TextTypes.h"
#include "Object.h"
#include <string>
#include <string_view>
#include <vector>

namespace GameEngine {
class UITextComponent;

/// @brief コンポーネントで操作できるスクリーンUIテキスト
class UIText final : public Object {
public:
   /// @brief UIテキストを作成し、自動描画レジストリへ登録する
   UIText();

   /// @brief 自動描画レジストリから解除する
   ~UIText() override;

   /// @brief UIテキストのオブジェクト種別を取得する
   /// @return ObjectType::UIText
   ObjectType GetObjectType() const override { return ObjectType::UIText; }

   /// @brief 文字列とスタイルを初期化する
   /// @param text UTF-8文字列
   /// @param style 表示設定
   void Create(std::string text, const TextStyle& style);

   /// @brief 表示文字列をUTF-8で設定する
   /// @param text UTF-8文字列
   void SetText(std::string text);

   /// @brief char8_tのUTF-8文字列を設定する
   /// @param text UTF-8文字列
   void SetText(std::u8string_view text);

   /// @brief UIテキストコンポーネントを取得する
   /// @return 常に有効なコンポーネント
   UITextComponent* GetTextComponent();

   /// @brief 登録済みUIテキスト一覧を取得する
   /// @return 自動描画対象一覧
   static const std::vector<UIText*>& GetRegisteredTexts();

   /// @brief UIテキストを自動描画対象から外す
   /// @param text 解除対象
   static void UnregisterText(UIText* text);

   /// @brief 自動描画レジストリをクリアする
   static void ClearRegisteredTexts() { sRegisteredTexts_.clear(); }

private:
   static std::vector<UIText*> sRegisteredTexts_;
};

} // namespace GameEngine
