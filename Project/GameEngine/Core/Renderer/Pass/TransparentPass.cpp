#include "pch.h"
#include "TransparentPass.h"
#include "FrameContext.h"
#include "CommandDispatch.h"
#include <algorithm>
#include "Graphics/OffscreenRenderTarget.h"

namespace GameEngine {

void TransparentPass::Sort(std::vector<std::unique_ptr<IDrawCommand>>& commands) const {
	if (commands.size() < 2) {
		return;
	}

	// 通常のアルファ合成を成立させるため奥から手前へ並べる。
	// 距離が同じ要素はstable_sortで登録順を保持し、描画のちらつきを防ぐ。
	std::stable_sort(commands.begin(), commands.end(),
		[](const std::unique_ptr<IDrawCommand>& lhs, const std::unique_ptr<IDrawCommand>& rhs) {
			const auto lPos = lhs->GetSortPosition();
			const auto rPos = rhs->GetSortPosition();
			Camera* lCam = lhs->GetCamera();
			Camera* rCam = rhs->GetCamera();

			const bool lHasPos = lPos.has_value() && lCam;
			const bool rHasPos = rPos.has_value() && rCam;

			// 位置情報を持つ要素を距離ソート対象として先にまとめ、情報なし同士は種別優先度へ委ねる。
			if (lHasPos != rHasPos) {
				return lHasPos > rHasPos;
			}

			if (lHasPos && rHasPos) {
				const float lDist = (*lPos - lCam->GetPosition()).LengthSquared();
				const float rDist = (*rPos - rCam->GetPosition()).LengthSquared();
				if (lDist != rDist) {
					return lDist > rDist;
				}
			}

			return lhs->GetTypePriority() > rhs->GetTypePriority();
		});
}

void TransparentPass::Execute(FrameContext& ctx) {
	if (!ctx.transparentCommands) {
		return;
	}

	Sort(*ctx.transparentCommands);

	// 不透明描画完了時点の色と深度を固定し、透明描画中の自己参照を防ぐ。
	if (ctx.offscreenRenderTarget) {
		ctx.offscreenRenderTarget->CaptureSceneTextures();
	}

	for (const auto& icmd : *ctx.transparentCommands) {
		DispatchDrawCommand(icmd->GetDrawCommand(), ctx);
	}
}

} // namespace GameEngine
