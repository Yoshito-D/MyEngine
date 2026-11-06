#pragma once

#include "GameEngine/Graphics/Renderer/Pass/IRenderPass.h"
#include <d3d12.h>

namespace GameEngine {
class OffscreenRenderTarget;

/// @brief ポストエフェクトパス
/// 1. PostProcessManager でエフェクトチェーンを適用する
/// 2. postProcessCommands（ポストプロセス後描画）を実行する
class PostEffectPass final : public IRenderPass {
public:
   /// @brief コンストラクタ
   /// @param offscreenRT オフスクリーンレンダーターゲット（ping-pong バッファ管理用）
   explicit PostEffectPass(OffscreenRenderTarget* offscreenRT) : offscreenRT_(offscreenRT) {}

   /// @brief ポストエフェクトを適用し、その結果へUIなどの後段描画コマンドを重ねる。
   /// @param ctx 今フレームの描画対象・デバイス・描画処理を保持するコンテキスト。
   /// @details 中間レンダーターゲットが未設定の場合は実行しない。
   void Execute(FrameContext& ctx) override;

   /// @brief デバッグ表示でこのレンダーパスを識別する名前を返す。
   /// @return 「PostEffectPass」を表す固定文字列。
   std::string_view GetName() const override { return "PostEffectPass"; }

   /// @brief レンダラーが実行順を分類するためのパス種別を返す。
   /// @return ポストエフェクトと後段描画を表すPostEffect。
   RenderPassType GetPassType() const override { return RenderPassType::PostEffect; }

private:
   /// @brief ポストエフェクトの入出力に使う中間レンダーターゲットへの参照。
   OffscreenRenderTarget* offscreenRT_ = nullptr;
};

} // namespace GameEngine
