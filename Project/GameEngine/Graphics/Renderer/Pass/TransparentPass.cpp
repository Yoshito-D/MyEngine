#include "GameEngine/pch.h"
#include "GameEngine/Graphics/Renderer/Pass/TransparentPass.h"
#include "GameEngine/Graphics/Renderer/Pass/FrameContext.h"
#include "GameEngine/Graphics/Renderer/Pass/DrawCommandExecutor.h"
#include "GameEngine/Graphics/Device/OffscreenRenderTarget.h"

namespace GameEngine {

void TransparentPass::Execute(FrameContext& ctx) {
   if (!ctx.renderQueue) return;

   ctx.renderQueue->SortTransparent();

   // 不透明描画完了時点の色と深度を固定し、透明描画中の自己参照を防ぐ。
   if (ctx.offscreenRenderTarget) {
      ctx.offscreenRenderTarget->CaptureSceneTextures();
   }

   DrawCommandExecutor::Execute(ctx.renderQueue->GetCommands(RenderPass::Transparent), ctx);
}

} // namespace GameEngine
