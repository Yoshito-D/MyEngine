#include "GameEngine/pch.h"
#include "GameEngine/Graphics/Renderer/Pass/DrawCommandExecutor.h"
#include "GameEngine/Graphics/Renderer/Pass/FrameContext.h"
#include "GameEngine/Graphics/Renderer/Pass/ModelRenderer.h"
#include "GameEngine/Graphics/Renderer/Pass/SpriteRenderer.h"
#include "GameEngine/Graphics/Renderer/Pass/ParticleRenderer.h"
#include "GameEngine/Graphics/Renderer/Pass/UIRenderer.h"
#include "GameEngine/Graphics/Renderer/Pass/TextRenderer.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include "GameEngine/Graphics/Pipeline/PSOManager.h"
#include "GameEngine/Graphics/Device/OffscreenRenderTarget.h"
#include "GameEngine/Effects/Particles/ParticleSystem.h"
#include <cmath>

namespace GameEngine {
void DrawCommandExecutor::Execute(const DrawCommand& command, FrameContext& context) {
   // パスは並び順とリソース状態を管理し、実データの型に対応するオーバーロードだけを選ぶ。
   std::visit([&context](const auto& data) { ExecuteData(data, context); }, command.GetPayload());
}

void DrawCommandExecutor::Execute(std::span<const DrawCommand> commands, FrameContext& context) {
   for (const auto& command : commands) {
      Execute(command, context);
   }
}

void DrawCommandExecutor::ExecuteData(const ModelDrawData& data, FrameContext& context) {
   if (!context.modelRenderer) return;

   context.modelRenderer->DrawModel(data, context.defaultMaterial, context.lightManager, context.setPipelineFunc);

   // モデル固有PSOやスキニングが直接設定した状態を、Facadeのバインドキャッシュへ持ち越さない。
   if (context.invalidatePipelineBindingFunc) {
      context.invalidatePipelineBindingFunc();
   }
}

void DrawCommandExecutor::ExecuteData(const SpriteDrawData& data, FrameContext& context) {
   if (!context.spriteRenderer) return;

   context.spriteRenderer->DrawSprite(data, context.defaultMaterial, context.lightManager, context.setPipelineFunc);
}

void DrawCommandExecutor::ExecuteData(const UISpriteDrawData& data, FrameContext& context) {
   if (!context.uiRenderer) return;

   context.uiRenderer->DrawUISprite(data, context.defaultMaterial, context.setPipelineFunc);
}

void DrawCommandExecutor::ExecuteData(const TextDrawData& data, FrameContext& context) {
   if (!context.textRenderer) return;

   context.textRenderer->DrawUIText(data, context.setPipelineFunc);
}

void DrawCommandExecutor::ExecuteData(const ParticleDrawData& data, FrameContext& context) {
   if (!context.particleRenderer) return;

   // 屈折はパス開始時のコピーではなく、描画直前の最新シーン色を全描画段階で参照する。
   if (context.offscreenRenderTarget && data.particleSystem) {
      const auto* material = data.particleSystem->GetMaterial();

      if (material && std::fabs(material->GetDistortionStrength()) > 0.0001f) {
         context.offscreenRenderTarget->CaptureSceneColor();
      }
   }

   context.particleRenderer->DrawParticle(data, context.setPipelineFunc, context.invalidatePipelineBindingFunc,
      context.offscreenRenderTarget ? context.offscreenRenderTarget->GetSceneColorSRVHandleGPU()
                                    : D3D12_GPU_DESCRIPTOR_HANDLE{},
      context.offscreenRenderTarget ? context.offscreenRenderTarget->GetSceneDepthSRVHandleGPU()
                                    : D3D12_GPU_DESCRIPTOR_HANDLE{});
}

void DrawCommandExecutor::ExecuteData(const LineDrawData& data, FrameContext& context) {
   if (!data.drawFunc || !context.device) return;

   const auto* pipeline = context.psoManager ? context.psoManager->GetPipeline("Line3D") : nullptr;

   if (!pipeline) return;

   context.device->GetCommandList()->SetPipelineState(pipeline->GetPipelineState());
   context.device->GetCommandList()->SetGraphicsRootSignature(pipeline->GetRootSignature());
   data.drawFunc(context.device->GetCommandList(), data.viewProjectionMatrix);

   // ラインの直接設定後も、同名のSpriteなどが古いPSOキャッシュで設定を省略しないようにする。
   if (context.invalidatePipelineBindingFunc) {
      context.invalidatePipelineBindingFunc();
   }
}
} // namespace GameEngine
