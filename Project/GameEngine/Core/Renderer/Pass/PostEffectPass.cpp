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

	// 深度を参照するOutline等の間だけDSVをSRV状態へ変更する。
	// エフェクト終了後はUI等が同じ深度バッファへ書ける状態へ戻す。
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

	// エフェクト済みの色を消さずにUI等を重ねる。直前の色・深度コピーは、
	// この区間の屈折パーティクルが自己参照しないための入力となる。
	// --- 2. ポストプロセス後コマンド（UI など）を実行 ---
	if (ctx.postProcessCommands && !ctx.postProcessCommands->empty()) {
		offscreenRT_->CaptureSceneTextures();
		offscreenRT_->PreDrawWithoutClear(true);

		for (const auto& icmd : *ctx.postProcessCommands) {
			DispatchDrawCommand(icmd->GetDrawCommand(), ctx);
		}

		offscreenRT_->PostDraw();
	}
}

} // namespace GameEngine
