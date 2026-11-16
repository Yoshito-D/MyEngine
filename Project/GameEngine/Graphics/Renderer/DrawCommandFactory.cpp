#include "GameEngine/pch.h"
#include "GameEngine/Graphics/Renderer/DrawCommandFactory.h"
#include "GameEngine/Graphics/Renderer/RenderQueue.h"
#include "GameEngine/Graphics/Pipeline/PSOManager.h"
#include "GameEngine/Graphics/Resources/Material.h"
#include "GameEngine/Graphics/Resources/Texture.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Object/Component/Rendering/MeshComponent.h"
#include "GameEngine/Object/Component/Rendering/MaterialComponent.h"
#include "GameEngine/Effects/Particles/ParticleSystem.h"
#include "GameEngine/Scene/Camera/Camera.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace GameEngine {
namespace {
RenderPass ChooseRenderPass(BlendMode blendMode, bool applyPostProcess) {
   if (!applyPostProcess) {
      // ポストプロセス対象外の要求は、エフェクト適用後のキューへ送る。
      return RenderPass::PostProcess;
   }

   // 深度を書き込む不透明物を先に確定し、ブレンド物だけを距離ソート可能な透明パスへ送る。
   return blendMode == BlendMode::kBlendModeNone ? RenderPass::Opaque : RenderPass::Transparent;
}
} // namespace

void ModelCommandFactory::Submit(
   const ModelDrawRequest& request, const DrawSubmissionContext& context, const PSOManager& pipelines) {
   if (request.textures.empty() || std::any_of(request.textures.begin(), request.textures.end(),
                                      [](const Texture* texture) { return texture == nullptr; })) {
      Logger::Warning("[ModelCommandFactory] Model draw requires valid textures");
      return;
   }

   auto* mesh = request.model.GetComponent<MeshComponent>();

   if (!mesh || (mesh->GetSourceType() == MeshComponent::SourceType::ModelFile && !mesh->GetModelAsset()) ||
       (mesh->GetSourceType() == MeshComponent::SourceType::Primitive && !mesh->EnsureMesh())) {
      return;
   }

   // 実行時にTextureオブジェクトへ再アクセスしないよう、登録時にメッシュ順のSRVを値として保持する。
   std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> handles;
   handles.reserve(request.textures.size());

   for (const auto* texture : request.textures) {
      handles.push_back(texture->GetTextureSrvHandleGPU());
   }

   request.model.UpdateMatrix(&request.camera);
   const BlendMode requestedBlend = request.blendMode.value_or(context.defaultBlendMode);
   auto* material = request.model.GetComponent<MaterialComponent>();
   const auto* asset = mesh->GetModelAsset();
   const size_t count = asset ? asset->GetMeshData().size() : 1;

   // サブメッシュごとに透過設定が異なるため、共有キューへ渡す前に描画単位へ分割する。
   for (size_t slot = 0; slot < count; ++slot) {
      const auto* value = material ? material->GetMaterial(slot) : nullptr;
      const std::string name = value && !value->GetPipelineName().empty() ? value->GetPipelineName() : "Object3D";
      const std::string effectiveName = pipelines.GetModelPipeline(name) ? name : "Object3D";
      const BlendMode blend = pipelines.ResolveModelBlendMode(
         effectiveName, value ? value->GetBlendMode().value_or(requestedBlend) : requestedBlend);
      ModelDrawData data{&request.model, slot, handles, context.environmentTexture, &request.camera, blend};
      context.queue.Enqueue(DrawCommand(ChooseRenderPass(blend, request.applyPostProcess), std::move(data)));
   }
}

void SpriteCommandFactory::Submit(const SpriteDrawRequest& request, const DrawSubmissionContext& context) {
   request.sprite.Update(&request.camera, &request.texture);
   BlendMode blend = request.blendMode.value_or(context.defaultBlendMode);

   // Spriteの明示的なマテリアル設定は、呼び出し引数とFacadeの既定値より優先する。
   if (const auto* material = request.sprite.GetComponent<MaterialComponent>()) {
      if (const auto* value = material->GetMaterial(); value && value->GetBlendMode()) {
         blend = *value->GetBlendMode();
      }
   }

   SpriteDrawData data{
      &request.sprite, &request.texture, request.texture.GetTextureSrvHandleGPU(), &request.camera, blend};
   context.queue.Enqueue(DrawCommand(ChooseRenderPass(blend, request.applyPostProcess), std::move(data)));
}

void ParticleCommandFactory::Submit(const ParticleDrawRequest& request, const DrawSubmissionContext& context) {
   auto& particle = request.particleSystem;
   const std::uint32_t activeCount = particle.GetActiveParticleCount();
   const bool needsGpuCleanup = activeCount == 0 && particle.GetDrawParticleCount() > 0;

   // CPU粒子が尽きた直後も、前フレームのGPU粒子をFreeListへ戻すため最後のCompute更新が必要。
   if (activeCount == 0 && !needsGpuCleanup) return;

   particle.UpdateMatrix(&request.camera);
   const auto* material = particle.GetMaterial();
   const bool requiresBloomPass = material && material->GetBrightness() > 1.0f;

   // 発光値はBloom前のHDRシーンへ書く。粒子のブレンド自体は実行時にParticleRendererが解決する。
   const RenderPass renderPass =
      particle.GetUsePostProcess() || requiresBloomPass ? RenderPass::Transparent : RenderPass::PostProcess;
   context.queue.Enqueue(DrawCommand(renderPass, ParticleDrawData{&particle, &request.camera}));
}

void UICommandFactory::Submit(const UIDrawRequest& request, const DrawSubmissionContext& context) {
   if (request.screenWidth == 0 || request.screenHeight == 0 || !std::isfinite(request.viewportSize.x) ||
       !std::isfinite(request.viewportSize.y) || request.viewportSize.x <= 0.0f || request.viewportSize.y <= 0.0f) {
      Logger::Warning("[UICommandFactory] UI draw requires positive viewport and reference dimensions");
      return;
   }

   // 基準レイアウト以外も現在の出力比率へ一様フィットさせ、後続のUIへ投影変更を持ち越さない。
   const bool usesReferenceLayout =
      request.screenWidth == Window::kUiReferenceWidth && request.screenHeight == Window::kUiReferenceHeight;

   if (!usesReferenceLayout) {
      const float scale = std::min(request.viewportSize.x / static_cast<float>(request.screenWidth),
         request.viewportSize.y / static_cast<float>(request.screenHeight));
      request.camera.SetOrthographicSize(request.viewportSize.x / scale, request.viewportSize.y / scale);
   }

   request.sprite.UpdateMatrixForUI(
      &request.camera, &request.texture, request.anchorPoint, request.screenWidth, request.screenHeight);

   if (!usesReferenceLayout) {
      request.camera.SetOrthographicSize(request.viewportSize.x, request.viewportSize.y);
   }

   const BlendMode blend = request.blendMode.value_or(context.defaultBlendMode);
   UISpriteDrawData data{&request.sprite, &request.texture, request.texture.GetTextureSrvHandleGPU(), blend};
   context.queue.Enqueue(DrawCommand(ChooseRenderPass(blend, request.applyPostProcess), std::move(data)));
}

DrawCommand LineCommandFactory::Create(std::function<void(ID3D12GraphicsCommandList*, const Matrix4x4&)> drawFunc,
   Camera* camera, RenderPass renderPass, std::optional<Vector3> sortPosition) {
   LineDrawData data;
   data.drawFunc = std::move(drawFunc);
   data.camera = camera;
   data.sortPosition = sortPosition;

   if (camera) {
      // 実行時までカメラ行列が変化しても、登録時に意図した視点で線を描けるよう値を保持する。
      data.viewProjectionMatrix = camera->GetViewProjectionMatrix();
   }

   return DrawCommand(renderPass, std::move(data));
}
} // namespace GameEngine
