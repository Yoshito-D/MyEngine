#include "GameEngine/pch.h"
#include "GameEngine/Graphics/Renderer/Pass/OpaquePass.h"
#include "GameEngine/Graphics/Renderer/Pass/FrameContext.h"
#include "GameEngine/Graphics/Renderer/Pass/DrawCommandExecutor.h"

namespace GameEngine {

void OpaquePass::Execute(FrameContext& ctx) {
   if (!ctx.renderQueue) return;

   // 登録順を維持して深度バッファを先に構築し、後続の透明描画と深度スナップショットの基準にする。
   DrawCommandExecutor::Execute(ctx.renderQueue->GetCommands(RenderPass::Opaque), ctx);
}

} // namespace GameEngine
