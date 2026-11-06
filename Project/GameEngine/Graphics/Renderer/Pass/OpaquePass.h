#pragma once

#include "GameEngine/Graphics/Renderer/Pass/IRenderPass.h"

namespace GameEngine {

/// @brief 不透明オブジェクト描画パス
/// FrameContext の opaqueCommands を順番に実行する。
class OpaquePass final : public IRenderPass {
public:
   /// @brief 不透明描画コマンドを登録順に実行して深度の基準を作る。
   /// @param ctx 今フレームの描画対象・デバイス・描画処理を保持するコンテキスト。
   /// @details 対応するコマンド列が未設定の場合は実行しない。
   void Execute(FrameContext& ctx) override;

   /// @brief デバッグ表示でこのレンダーパスを識別する名前を返す。
   /// @return 「OpaquePass」を表す固定文字列。
   std::string_view GetName() const override { return "OpaquePass"; }

   /// @brief レンダラーが実行順を分類するためのパス種別を返す。
   /// @return 不透明描画を表すOpaque。
   RenderPassType GetPassType() const override { return RenderPassType::Opaque; }
};

} // namespace GameEngine
