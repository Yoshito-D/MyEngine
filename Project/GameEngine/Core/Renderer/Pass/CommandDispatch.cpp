#include "pch.h"
#include "CommandDispatch.h"
#include "FrameContext.h"
#include "ModelRenderer.h"
#include "SpriteRenderer.h"
#include "ParticleRenderer.h"
#include "UIRenderer.h"
#include "Graphics/GraphicsDevice.h"
#include "PSOManager.h"
#include "Graphics/OffscreenRenderTarget.h"
#include "Effect/ParticleSystem.h"

namespace GameEngine {

void DispatchDrawCommand(const DrawCommand& cmd, FrameContext& ctx) {
   switch (cmd.type) {
	  case DrawCommandType::Model:
		 if (ctx.modelRenderer) {
			ctx.modelRenderer->DrawModel(
			   cmd.modelData,
			   ctx.defaultMaterial,
			   ctx.lightManager,
			   ctx.setPipelineFunc);
		 }
		 break;

	  case DrawCommandType::Sprite:
		 if (cmd.isUISprite) {
			if (ctx.uiRenderer) {
			   ctx.uiRenderer->DrawUISprite(
				  cmd.uiSpriteData,
				  ctx.defaultMaterial,
				  ctx.setPipelineFunc);
			}
		 } else {
			if (ctx.spriteRenderer) {
			   ctx.spriteRenderer->DrawSprite(
				  cmd.spriteData,
				  ctx.defaultMaterial,
				  ctx.lightManager,
				  ctx.setPipelineFunc);
			}
		 }
		 break;

	  case DrawCommandType::Particle:
		 if (ctx.particleRenderer) {
			// 屈折はパス開始時の古いコピーではなく、描画直前までの最新シーン色を参照する。
			// これによりポストプロセス除外オブジェクトの上でもクリアカラーへ抜けない。
			if (ctx.offscreenRenderTarget && cmd.particleData.particleSystem) {
			   const auto* material = cmd.particleData.particleSystem->GetMaterial();
			   if (material && std::fabs(material->GetDistortionStrength()) > 0.0001f) {
				  ctx.offscreenRenderTarget->CaptureSceneColor();
			   }
			}
			ctx.particleRenderer->DrawParticle(
			   cmd.particleData,
			   ctx.setPipelineFunc,
			   ctx.invalidatePipelineBindingFunc,
			   ctx.offscreenRenderTarget ? ctx.offscreenRenderTarget->GetSceneColorSRVHandleGPU() : D3D12_GPU_DESCRIPTOR_HANDLE{},
			   ctx.offscreenRenderTarget ? ctx.offscreenRenderTarget->GetSceneDepthSRVHandleGPU() : D3D12_GPU_DESCRIPTOR_HANDLE{});
		 }
		 break;

	  case DrawCommandType::Line:
		 if (cmd.lineData.drawFunc && ctx.device) {
			auto* linePso = ctx.psoManager ? ctx.psoManager->GetPipeline("Line3D") : nullptr;
			if (linePso) {
			   ctx.device->GetCommandList()->SetPipelineState(linePso->GetPipelineState());
			   ctx.device->GetCommandList()->SetGraphicsRootSignature(linePso->GetRootSignature());
			   cmd.lineData.drawFunc(ctx.device->GetCommandList(), cmd.lineData.viewProjectionMatrix);
			}
		 }
		 break;

	  default:
		 break;
   }
}

} // namespace GameEngine
