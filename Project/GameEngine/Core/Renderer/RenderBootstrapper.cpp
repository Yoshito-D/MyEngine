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

bool RenderBootstrapper::Initialize(const RenderBootstrapContext& context) const {
   context.offscreenRenderTarget->Initialize(
	  context.device);

   if (!context.shaderManager->Initialize(context.device)) {
	  Logger::Error("[RenderBootstrapper] Failed to initialize shaders from JSON registry.");
	  return false;
   }

   context.psoManager->Initialize(context.device, context.shaderManager);

   context.modelRenderer->Initialize(context.device, context.psoManager, context.assetManager);
   context.spriteRenderer->Initialize(context.device, context.psoManager);
   context.particleRenderer->Initialize(context.device, context.psoManager);

   if (!context.psoManager->LoadPipelineDefinitions(L"resources/pipelines/pipeline_registry.json", context.offscreenRenderTarget->GetFormat())) {
      Logger::Error("[RenderBootstrapper] Failed to load pipeline definitions from JSON registry.");
	  return false;
   }
   Logger::Info("Successfully loaded pipeline definitions from JSON");

   context.lineRenderer->Initialize(context.device->GetDevice(), 100000);
   context.postProcessLineRenderer->Initialize(context.device->GetDevice(), 100000);

   context.uiRenderer->Initialize(context.device, context.psoManager, context.uiCamera, context.spriteRenderer, context.lightManager);

   context.postProcessManager->Initialize(context.device, context.offscreenRenderTarget, context.psoManager);

   if (!context.postProcessManager->LoadEffectsFromJson(L"resources/postprocess/postprocess_registry.json")) {
      Logger::Error("[RenderBootstrapper] Failed to load post-process effects from JSON registry.");
	  return false;
   }
   Logger::Info("Successfully loaded post-process effects from JSON");

   context.shaderManager->LogRootParameterTablesDebug();
   return true;
}

}
