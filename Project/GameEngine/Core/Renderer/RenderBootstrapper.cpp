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

namespace {
Logger& log_ = Logger::GetInstance();
}

namespace GameEngine {

void RenderBootstrapper::Initialize(const RenderBootstrapContext& context) const {
   context.offscreenRenderTarget->Initialize(context.device);

   context.shaderManager->Initialize(context.device);
   context.psoManager->Initialize(context.device, context.shaderManager);

   context.modelRenderer->Initialize(context.device, context.psoManager, context.assetManager);
   context.spriteRenderer->Initialize(context.device, context.psoManager);
   context.particleRenderer->Initialize(context.device, context.psoManager);

   if (!context.psoManager->LoadPipelineDefinitions(L"resources/pipelines/pipeline_registry.json", context.offscreenRenderTarget->GetFormat())) {
      log_.Log("Failed to load pipeline definitions from JSON, using predefined pipelines");
      context.psoManager->CreatePredefinedPipelines(context.offscreenRenderTarget);
   } else {
      log_.Log("Successfully loaded pipeline definitions from JSON");
   }

   if (!context.psoManager->LoadPipelineDefinitions(L"resources/pipelines/skinning_pipeline_registry.json", context.offscreenRenderTarget->GetFormat())) {
      log_.Log("Failed to load skinning pipeline definitions from JSON", Logger::LogLevel::Error);
   }

   context.lineRenderer->Initialize(context.device->GetDevice(), 100000);
   context.postProcessLineRenderer->Initialize(context.device->GetDevice(), 100000);

   context.uiRenderer->Initialize(context.device, context.psoManager, context.uiCamera, context.spriteRenderer, context.lightManager);

   context.postProcessManager->Initialize(context.device, context.offscreenRenderTarget, context.psoManager);

   if (!context.postProcessManager->LoadEffectsFromJson(L"resources/postprocess/postprocess_registry.json")) {
      log_.Log("Failed to load post-process effects from JSON, using predefined effects");
      context.postProcessManager->RegisterPredefinedEffects();
   } else {
      log_.Log("Successfully loaded post-process effects from JSON");
   }

   context.shaderManager->LogRootParameterTablesDebug();
}

}
