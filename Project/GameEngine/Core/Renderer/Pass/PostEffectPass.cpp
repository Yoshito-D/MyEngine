#include "pch.h"
#include "PostEffectPass.h"
#include "FrameContext.h"
#include "CommandDispatch.h"
#include "Graphics/OffscreenRenderTarget.h"
#include "PostProcess/PostProcessManager.h"

namespace GameEngine {

void PostEffectPass::Execute(FrameContext& ctx) {
	if (!offscreenRT_) {
		return;
	}

	// --- 1. ポストプロセスエフェクトチェーンを適用 ---
	if (ctx.postProcessMgr) {
		ctx.postProcessMgr->ApplyEffects(offscreenRT_->GetSRVHandleGPU());
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
