#pragma once

#include "GameEngine/Graphics/Renderer/Pass/IRenderPass.h"
#include "GameEngine/Graphics/Renderer/DrawCommand.h"
#include <vector>
#include <memory>

namespace GameEngine {

/// @brief 半透明オブジェクト描画パス
/// カメラ距離でソートしてから transparentCommands を実行する。
class TransparentPass final : public IRenderPass {
public:
   /// @brief 透過描画コマンドをカメラ距離などで並べ替え、シーン色・深度を保存してから描画する。
   /// @param ctx 今フレームの描画対象・デバイス・描画処理を保持するコンテキスト。
   /// @details 対応するコマンド列が未設定の場合は実行しない。
   void Execute(FrameContext& ctx) override;

   /// @brief デバッグ表示でこのレンダーパスを識別する名前を返す。
   /// @return 「TransparentPass」を表す固定文字列。
   std::string_view GetName() const override { return "TransparentPass"; }

   /// @brief レンダラーが実行順を分類するためのパス種別を返す。
   /// @return 半透明描画を表すTransparent。
   RenderPassType GetPassType() const override { return RenderPassType::Transparent; }

private:
   /// @brief カメラ距離でコマンドをソート（遠い順）
   void Sort(std::vector<std::unique_ptr<IDrawCommand>>& commands) const;
};

} // namespace GameEngine
