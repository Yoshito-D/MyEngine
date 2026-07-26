#include "RenderBootstrapper.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/OffscreenRenderTarget.h"
#include "Asset/AssetManager.h"
#include "Window/Window.h"
#include "ShaderManager.h"
#include "PSOManager.h"
#include "ModelRenderer.h"
#include "SpriteRenderer.h"
#include "ParticleRenderer.h"
#include "UIRenderer.h"
#include "Pass/TextRenderer.h"
#include "LineRenderer.h"
#include "PostProcess/PostProcessManager.h"
#include "Scene/Camera/Camera.h"
#include "Utility/Logger.h"

namespace GameEngine {
namespace {
constexpr size_t kLineRendererCapacity = 100000;
}

bool RenderBootstrapper::Initialize(const RenderBootstrapContext& context) const {
   // 後続パスが参照する描画形式を確定するため、オフスクリーンターゲットを最初に生成する。
   context.offscreenRenderTarget->Initialize(
	  context.device);

   if (!context.shaderManager->Initialize(context.device)) {
	  Logger::Error("[RenderBootstrapper] Failed to initialize shaders from JSON registry.");
	  return false;
   }

   context.psoManager->Initialize(context.device, context.shaderManager);

   // 各レンダラーへ共有サービスを先に注入し、PSO定義ロード後すぐ描画可能な状態にする。
   context.modelRenderer->Initialize(context.device, context.psoManager, context.assetManager);
   context.spriteRenderer->Initialize(context.device, context.psoManager);
   context.particleRenderer->Initialize(context.device, context.psoManager);

   if (!context.psoManager->LoadPipelineDefinitions(L"resources/engine/pipelines/pipeline_registry.json", context.offscreenRenderTarget->GetFormat())) {
      Logger::Error("[RenderBootstrapper] Failed to load pipeline definitions from JSON registry.");
	  return false;
   }
   Logger::Info("Successfully loaded pipeline definitions from JSON");

   if (!context.textRenderer->Initialize(context.device, context.psoManager, context.assetManager->GetFontManager())) {
      Logger::Error("[RenderBootstrapper] Failed to initialize the text renderer.");
      return false;
   }

   context.lineRenderer->Initialize(context.device->GetDevice(), kLineRendererCapacity);
   context.postProcessLineRenderer->Initialize(context.device->GetDevice(), kLineRendererCapacity);

   context.uiRenderer->Initialize(context.device, context.psoManager, context.uiCamera, context.spriteRenderer, context.lightManager);

   context.postProcessManager->Initialize(context.device, context.offscreenRenderTarget, context.psoManager);

   // ポストプロセスはPSOと中間ターゲットの両方を必要とするため、共通描画基盤の後にレジストリを読む。
   if (!context.postProcessManager->LoadEffectsFromJson(L"resources/engine/postprocess/postprocess_registry.json")) {
      Logger::Error("[RenderBootstrapper] Failed to load post-process effects from JSON registry.");
	  return false;
   }
   Logger::Info("Successfully loaded post-process effects from JSON");

   context.shaderManager->LogRootParameterTablesDebug();
   return true;
}

}
