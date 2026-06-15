#include "pch.h"
#include "PostEffectPass.h"
#include "FrameContext.h"
#include "CommandDispatch.h"
#include "Graphics/OffscreenRenderTarget.h"
#include "Graphics/GraphicsDevice.h"
#include "PostProcess/PostProcessManager.h"

namespace GameEngine {

void PostEffectPass::Execute(FrameContext& ctx) {
	if (!offscreenRT_) {
		return;
	}

	// メイン描画パスの終了：current を RTV→SRV に遷移させてエフェクトの入力として使えるようにする
	offscreenRT_->PostDraw();

	// --- 1. ポストプロセスエフェクトチェーンを適用 ---
	if (ctx.postProcessMgr) {
		if (ctx.device) {
			ctx.device->TransitionDepthStencilToShaderResource();
		}
		ctx.postProcessMgr->ApplyEffects(offscreenRT_->GetSRVHandleGPU());
		if (ctx.device) {
			ctx.device->TransitionDepthStencilToWrite();
		}
	}

	// --- 2. ポストプロセス後コマンド（UI など）を実行 ---
	if (ctx.postProcessCommands && !ctx.postProcessCommands->empty()) {
		offscreenRT_->PreDrawWithoutClear(true);

		for (const auto& icmd : *ctx.postProcessCommands) {
			DispatchDrawCommand(icmd->GetDrawCommand(), ctx);
		}

		offscreenRT_->PostDraw();
	}
}

} // namespace GameEngine
