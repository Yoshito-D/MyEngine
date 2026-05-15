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

	void Execute(FrameContext& ctx) override;
	std::string_view GetName() const override { return "PostEffectPass"; }
	RenderPassType GetPassType() const override { return RenderPassType::PostEffect; }

private:
	OffscreenRenderTarget* offscreenRT_ = nullptr;
};

} // namespace GameEngine
