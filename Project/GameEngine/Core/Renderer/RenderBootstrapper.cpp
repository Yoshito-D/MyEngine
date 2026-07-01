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
#include "LineRenderer.h"
#include "PostProcess/PostProcessManager.h"
#include "Scene/Camera/Camera.h"
#include "Utility/Logger.h"

namespace GameEngine {

void RenderBootstrapper::Initialize(const RenderBootstrapContext& context) const {
   context.offscreenRenderTarget->Initialize(
	  context.device);

   context.shaderManager->Initialize(context.device);
   context.psoManager->Initialize(context.device, context.shaderManager);

   context.modelRenderer->Initialize(context.device, context.psoManager, context.assetManager);
   context.spriteRenderer->Initialize(context.device, context.psoManager);
   context.particleRenderer->Initialize(context.device, context.psoManager);

   if (!context.psoManager->LoadPipelineDefinitions(L"resources/pipelines/pipeline_registry.json", context.offscreenRenderTarget->GetFormat())) {
      Logger::Info("Failed to load pipeline definitions from JSON, using predefined pipelines");
      context.psoManager->CreatePredefinedPipelines(context.offscreenRenderTarget);
   } else {
      Logger::Info("Successfully loaded pipeline definitions from JSON");
   }

   context.lineRenderer->Initialize(context.device->GetDevice(), 100000);
   context.postProcessLineRenderer->Initialize(context.device->GetDevice(), 100000);

   context.uiRenderer->Initialize(context.device, context.psoManager, context.uiCamera, context.spriteRenderer, context.lightManager);

   context.postProcessManager->Initialize(context.device, context.offscreenRenderTarget, context.psoManager);

   if (!context.postProcessManager->LoadEffectsFromJson(L"resources/postprocess/postprocess_registry.json")) {
      Logger::Info("Failed to load post-process effects from JSON, using predefined effects");
      context.postProcessManager->RegisterPredefinedEffects();
   } else {
      Logger::Info("Successfully loaded post-process effects from JSON");
   }

   context.shaderManager->LogRootParameterTablesDebug();
}

}
