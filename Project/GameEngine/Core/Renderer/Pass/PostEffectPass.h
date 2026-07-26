#pragma once
#include "IRenderPass.h"
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

	/// @copydoc IRenderPass::Execute
	void Execute(FrameContext& ctx) override;
	/// @copydoc IRenderPass::GetName
	std::string_view GetName() const override { return "PostEffectPass"; }
	/// @copydoc IRenderPass::GetPassType
	RenderPassType GetPassType() const override { return RenderPassType::PostEffect; }

private:
	OffscreenRenderTarget* offscreenRT_ = nullptr;
};

} // namespace GameEngine
