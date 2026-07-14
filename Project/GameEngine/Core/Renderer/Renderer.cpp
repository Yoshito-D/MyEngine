#include "pch.h"
#include "Renderer.h"
#include "Graphics/GraphicsDevice.h"
#include "Model/Model.h"
#include "Camera/Camera.h"
#include "Graphics/RootSignature.h"
#include "Graphics/ShaderCompiler.h"
#include "Graphics/Mesh.h"
#include "Graphics/TransformationMatrix.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include "SpotLight.h"
#include "AreaLight.h"
#include "Effect/ParticleSystem.h"
#include "PostProcess/Grayscale.h"
#include "PostProcess/RadialBlur.h"
#include "PostProcess/GaussFilter.h"
#include "PostProcess/Vignette.h"
#include"PostProcess/ChromaticAberration.h"
#include"PostProcess/ShockWave.h"
#include "PostProcess/Pixelation.h"
#include "Utility/MathUtils.h"
#include "Asset/AssetManager.h"
#include "Asset/AnimationAssetManager.h"
#include "Asset/TextureManager.h"
#include "Component/AnimationComponent.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/MaterialComponent.h"
#include "RenderBootstrapper.h"
#include "Object/Skybox/Skybox.h"
#include "Pass/OpaquePass.h"
#include "Pass/TransparentPass.h"
#include "Pass/PostEffectPass.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <tuple>
#include <unordered_set>

#ifdef USE_IMGUI
#include "Editor/Renderer/RendererEditorController.h"
#include "imgui.h"
#endif

namespace {

GameEngine::Vector3 ExtractTranslation(const GameEngine::Matrix4x4& matrix) {
   return GameEngine::Vector3(matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]);
}

std::vector<GameEngine::Object*> CollectSceneObjectsForRender() {
   std::vector<GameEngine::Object*> objects;

   const auto& models = GameEngine::Model::GetRegisteredModels();
   objects.reserve(models.size() + GameEngine::Sprite::GetRegisteredSprites().size());
   for (auto* model : models) {
	  if (model) {
		 objects.push_back(model);
	  }
   }

   const auto& sprites = GameEngine::Sprite::GetRegisteredSprites();
   for (auto* sprite : sprites) {
	  if (sprite) {
		 objects.push_back(sprite);
	  }
   }

   return objects;
}

void ResolveParentRelationForRender(GameEngine::Object* object, const std::vector<GameEngine::Object*>& sceneObjects) {
   if (!object) {
	  return;
   }

   auto* transformComponent = object->GetComponent<GameEngine::TransformComponent>();
   if (!transformComponent) {
	  return;
   }

   if (transformComponent->parentObjectName.empty()) {
	  transformComponent->useParentMatrix = false;
	  transformComponent->parentMatrix = GameEngine::MakeIdentity4x4();
	  return;
   }

   GameEngine::Object* parentObject = nullptr;
   for (auto* candidate : sceneObjects) {
	  if (!candidate || candidate == object) {
		 continue;
	  }
	  if (candidate->GetObjectName() == transformComponent->parentObjectName) {
		 parentObject = candidate;
		 break;
	  }
   }

   if (!parentObject) {
	  transformComponent->useParentMatrix = false;
	  transformComponent->parentMatrix = GameEngine::MakeIdentity4x4();
	  return;
   }

   const auto* parentTransform = parentObject->GetComponent<GameEngine::TransformComponent>();
   if (!parentTransform) {
	  transformComponent->useParentMatrix = false;
	  transformComponent->parentMatrix = GameEngine::MakeIdentity4x4();
	  return;
   }

   transformComponent->useParentMatrix = true;
   transformComponent->parentMatrix = GameEngine::MakeAffineMatrix(parentTransform->transform);
}
}

namespace GameEngine {
Renderer::~Renderer() = default;

void Renderer::Initialize(GraphicsDevice* device, Window* window, CameraManager* cameraManager, LightManager* lightManager, AssetManager* assetManager) {
   device_ = device;
   cameraManager_ = cameraManager;
   lightManager_ = lightManager;
   assetManager_ = assetManager;

#ifdef USE_IMGUI
   imGuiManager_->Initialize(window->GetHwnd(), device_);
   editorController_ = std::make_unique<RendererEditorController>();
   editorController_->Initialize(assetManager_);
#endif

   defaultMaterial_ = std::make_unique<Material>();
   defaultMaterial_->Create(0xFF3399FF, Material::LightingMode::NONE);

   // UI描画専用カメラの初期化
   InitializeUICamera();

   if (!renderBootstrapper_) {
	  renderBootstrapper_ = std::make_unique<RenderBootstrapper>();
   }

   RenderBootstrapContext context{};
   context.device = device_;
   context.window = window;
   context.cameraManager = cameraManager_;
   context.lightManager = lightManager_;
   context.assetManager = assetManager_;
   context.defaultMaterial = defaultMaterial_.get();
   context.offscreenRenderTarget = offscreenRenderTarget_.get();
   context.shaderManager = shaderManager_.get();
   context.psoManager = psoManager_.get();
   context.modelRenderer = modelRenderer_.get();
   context.spriteRenderer = spriteRenderer_.get();
   context.particleRenderer = particleRenderer_.get();
   context.uiRenderer = uiRenderer_.get();
   context.lineRenderer = lineRenderer_.get();
   context.postProcessLineRenderer = postProcessLineRenderer_.get();
   context.uiCamera = uiCamera_.get();
   context.postProcessManager = postProcessManager_.get();

   if (!renderBootstrapper_->Initialize(context)) {
	  Logger::Error("[Renderer] Render bootstrap failed. Rendering passes were not created.");
	  return;
   }

   BuildDefaultPasses();
}

void Renderer::AddPass(std::unique_ptr<IRenderPass> pass) {
   if (pass) {
      renderPasses_.push_back(std::move(pass));
   }
}

void Renderer::ClearPasses() {
   renderPasses_.clear();
}

void Renderer::BuildDefaultPasses() {
   ClearPasses();
   AddPass(std::make_unique<OpaquePass>());
   AddPass(std::make_unique<TransparentPass>());
   AddPass(std::make_unique<PostEffectPass>(offscreenRenderTarget_.get()));

   // FrameContext を組み立てる（静的部分のみ。コマンドキューは BeginFrame でリセット済みのポインタを参照）
   frameCtx_.device          = device_;
   frameCtx_.psoManager      = psoManager_.get();
   frameCtx_.lightManager    = lightManager_;
   frameCtx_.postProcessMgr  = postProcessManager_.get();
   frameCtx_.offscreenRenderTarget = offscreenRenderTarget_.get();
   frameCtx_.defaultMaterial = defaultMaterial_.get();
   frameCtx_.modelRenderer    = modelRenderer_.get();
   frameCtx_.spriteRenderer   = spriteRenderer_.get();
   frameCtx_.particleRenderer = particleRenderer_.get();
   frameCtx_.uiRenderer       = uiRenderer_.get();
   frameCtx_.opaqueCommands      = &opaqueCommands_;
   frameCtx_.transparentCommands = &transparentCommands_;
   frameCtx_.postProcessCommands = &postProcessCommands_;
	frameCtx_.setPipelineFunc = [this](const std::string& name, BlendMode mode) {
	   SetPipeline(name, mode);
	};
	frameCtx_.invalidatePipelineBindingFunc = [this]() {
	   InvalidatePipelineBinding();
	};
}

void Renderer::SyncRenderTargetSizeToDevice() {
   if (!device_ || !offscreenRenderTarget_) {
	  return;
   }

   const uint32_t width = device_->GetBackBufferWidth();
   const uint32_t height = device_->GetBackBufferHeight();
   offscreenRenderTarget_->Resize(width, height);
   SyncUICameraToRenderTarget(width, height);
}

void Renderer::BeginFrame() {
   // ライトの構造化バッファを更新
   if (lightManager_) {
	  lightManager_->UpdateStructureBuffer();
   }

   // 描画コマンドリストをクリア
   opaqueCommands_.clear();
   transparentCommands_.clear();
   postProcessCommands_.clear();

   offscreenRenderTarget_->PreDraw(true);

#ifdef USE_IMGUI
   imGuiManager_->BeginFrame();
#endif

   // 初期パイプライン設定（パイプライン名とブレンドモードをリセット）
   currentPipelineName_ = "";
   currentPipelineBlendMode_ = BlendMode::kBlendModeNone;

   lineRenderer_->Begin();
   postProcessLineRenderer_->Begin();

}

void Renderer::Draw(Model* model, Texture* texture, std::optional<BlendMode> blendMode, bool applyPostProcess) {
   assert(model != nullptr);
   assert(texture != nullptr);

   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   if (!model->GetComponent<ModelAssetComponent>()->GetModelAsset()) return;

   std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> handles = { texture->GetTextureSrvHandleGPU() };

   // 行列を更新
   model->UpdateMatrix(activeCamera);

   // MaterialComponent のブレンドモード優先解決
   BlendMode effectiveBlendMode = currentBlendMode_;
   if (const auto* mc = model->GetComponent<MaterialComponent>()) {
      if (!mc->materials.empty() && mc->materials[0]) {
         if (const auto matBlend = mc->materials[0]->GetBlendMode()) {
            effectiveBlendMode = *matBlend;
         } else {
            effectiveBlendMode = blendMode.value_or(currentBlendMode_);
         }
      } else {
         effectiveBlendMode = blendMode.value_or(currentBlendMode_);
      }
   } else {
      effectiveBlendMode = blendMode.value_or(currentBlendMode_);
   }

   // 描画パスの決定
   RenderPass renderPass = DetermineRenderPass(effectiveBlendMode, applyPostProcess);

   DrawCommand cmd = DrawCommand::CreateModel(model, handles, activeCamera, effectiveBlendMode, renderPass);
   cmd.modelData.environmentTextureSrvHandle = activeEnvironmentTextureSrvHandle_;
   SubmitDrawCommand(cmd);
}

void Renderer::Draw(Model* model, const std::vector<Texture*>& textures, std::optional<BlendMode> blendMode, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);
   assert(model != nullptr);
   assert(!textures.empty());

   if (!model->GetComponent<ModelAssetComponent>()->GetModelAsset()) return;

   // TextureポインタからSRVハンドルに変換
   std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> textureSrvHandles;
   textureSrvHandles.reserve(textures.size());
   for (const auto& texture : textures) {
	  assert(texture != nullptr);
	  textureSrvHandles.push_back(texture->GetTextureSrvHandleGPU());
   }

   // 行列を更新
   model->UpdateMatrix(activeCamera);

   // MaterialComponent のブレンドモード優先解決
   BlendMode effectiveBlendMode = currentBlendMode_;
   if (const auto* mc = model->GetComponent<MaterialComponent>()) {
      if (!mc->materials.empty() && mc->materials[0]) {
         if (const auto matBlend = mc->materials[0]->GetBlendMode()) {
            effectiveBlendMode = *matBlend;
         } else {
            effectiveBlendMode = blendMode.value_or(currentBlendMode_);
         }
      } else {
         effectiveBlendMode = blendMode.value_or(currentBlendMode_);
      }
   } else {
      effectiveBlendMode = blendMode.value_or(currentBlendMode_);
   }

   // 描画パスの決定
   RenderPass renderPass = DetermineRenderPass(effectiveBlendMode, applyPostProcess);

   DrawCommand cmd = DrawCommand::CreateModel(model, textureSrvHandles, activeCamera, effectiveBlendMode, renderPass);
   cmd.modelData.environmentTextureSrvHandle = activeEnvironmentTextureSrvHandle_;
   SubmitDrawCommand(cmd);
}

void Renderer::Draw(Sprite* sprite, Texture* texture, std::optional<BlendMode> blendMode, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);
   assert(sprite != nullptr);
   assert(texture != nullptr);

   sprite->Update(activeCamera, texture);

   // MaterialComponent のブレンドモード優先解決
   BlendMode effectiveBlendMode = currentBlendMode_;
   if (const auto* mc = sprite->GetComponent<MaterialComponent>()) {
      if (!mc->materials.empty() && mc->materials[0]) {
         if (const auto matBlend = mc->materials[0]->GetBlendMode()) {
            effectiveBlendMode = *matBlend;
         } else {
            effectiveBlendMode = blendMode.value_or(currentBlendMode_);
         }
      } else {
         effectiveBlendMode = blendMode.value_or(currentBlendMode_);
      }
   } else {
      effectiveBlendMode = blendMode.value_or(currentBlendMode_);
   }

   // 描画パスの決定
   RenderPass renderPass = DetermineRenderPass(effectiveBlendMode, applyPostProcess);

   DrawCommand cmd = DrawCommand::CreateSprite(sprite, texture, texture->GetTextureSrvHandleGPU(),
	  activeCamera, effectiveBlendMode, renderPass);
   SubmitDrawCommand(cmd);
}

void Renderer::Draw(ParticleSystem* particleSystem) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);
   assert(particleSystem != nullptr);

   // CPU粒子が尽きた直後も、前フレームのGPU粒子をFreeListへ戻すため最後のCompute更新が必要。
   const uint32_t activeCount = particleSystem->GetActiveParticleCount();
   const bool needsGpuCleanup = activeCount == 0 && particleSystem->GetDrawParticleCount() > 0;
   if (activeCount == 0 && !needsGpuCleanup) return;

   particleSystem->UpdateMatrix(activeCamera);

   // パーティクルマテリアルのブレンドモード優先解決（デフォルトは加算）
   BlendMode blendMode = BlendMode::kBlendModeAdd;
   if (const auto* pm = particleSystem->GetMaterial()) {
      if (const auto matBlend = pm->GetBlendMode()) {
         blendMode = *matBlend;
      }
   }
   const auto* particleMaterial = particleSystem->GetMaterial();
   const bool requiresBloomPass = particleMaterial && particleMaterial->GetBrightness() > 1.0f;
   // 発光値はBloom前のHDRシーンへ書く必要がある。明示設定がなくても輝度>1なら自動的に前段へ送る。
   RenderPass renderPass = particleSystem->GetUsePostProcess() || requiresBloomPass
	  ? RenderPass::Transparent
	  : RenderPass::PostProcess;

   // パーティクルは常に遅延描画（透明度があるため）
   DrawCommand cmd = DrawCommand::CreateParticle(particleSystem, activeCamera, blendMode, renderPass);
   SubmitDrawCommand(cmd);
}

void Renderer::DrawUI(Sprite* sprite, Texture* texture,
   Sprite::AnchorPoint anchorPoint, std::optional<BlendMode> blendMode, bool applyPostProcess,
   uint32_t screenWidth, uint32_t screenHeight) {
   assert(sprite != nullptr);
   assert(texture != nullptr);

   // UI専用カメラとライトをセット、テクスチャ座標も更新
   SyncUICameraToRenderTarget(screenWidth, screenHeight);
   sprite->UpdateMatrixForUI(uiCamera_.get(), texture, anchorPoint, screenWidth, screenHeight);

   D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = texture->GetTextureSrvHandleGPU();

   // ブレンドモードの決定（引数で指定されていない場合は現在のモードを使用）
//    BlendMode effectiveBlendMode = blendMode.value_or(currentBlendMode_);

   BlendMode effectiveBlendMode = blendMode.value_or(currentBlendMode_);

   // 描画パスの決定
   RenderPass renderPass = DetermineRenderPass(effectiveBlendMode, applyPostProcess);

   DrawCommand cmd = DrawCommand::CreateUISprite(sprite, texture, textureSrvHandle,
	  anchorPoint, screenWidth, screenHeight,
	  effectiveBlendMode, renderPass);
   SubmitDrawCommand(cmd);
}

void Renderer::DrawLine(const Vector3& start, const Vector3& end, const Vector4& color, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   if (auto* renderer = SelectLineRenderer(applyPostProcess)) {
	  renderer->DrawLine(start, end, color, activeCamera);
   }
}

void Renderer::DrawSkybox(Skybox* skybox) {
   if (!skybox) return;

   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   if (!activeCamera) return;

   Texture* texture = skybox->GetTexture();
   if (!texture) return;

   // ビュー行列から平行移動を除去してVP行列を作成
   Matrix4x4 viewMatrix = activeCamera->GetViewMatrix();
   viewMatrix.m[3][0] = 0.0f;
   viewMatrix.m[3][1] = 0.0f;
   viewMatrix.m[3][2] = 0.0f;
   Matrix4x4 proj = activeCamera->GetProjectionMatrix();
   Matrix4x4 vp = viewMatrix * proj;
   skybox->UpdateTransform(vp);

   auto* skyboxPipeline = psoManager_->GetPipeline("Skybox");
   if (!skyboxPipeline) return;

   auto* cmdList = device_->GetCommandList();
   cmdList->SetGraphicsRootSignature(skyboxPipeline->GetRootSignature());
   cmdList->SetPipelineState(skyboxPipeline->GetPipelineState());

   const auto materialSlot = psoManager_->ResolvePipelineRootParameter("Skybox", "material");
   const auto transformSlot = psoManager_->ResolvePipelineRootParameter("Skybox", "transform");
   const auto textureSlot = psoManager_->ResolvePipelineRootParameter("Skybox", "texture");
   if (!materialSlot || !transformSlot || !textureSlot) {
	  Logger::Error("[Renderer] Failed to resolve Skybox root slots from PSO JSON.");
	  return;
   }

   cmdList->SetGraphicsRootConstantBufferView(materialSlot.value(), skybox->GetMaterialResource()->GetGPUVirtualAddress());
   cmdList->SetGraphicsRootConstantBufferView(transformSlot.value(), skybox->GetTransformResource()->GetGPUVirtualAddress());
   cmdList->SetGraphicsRootDescriptorTable(textureSlot.value(), texture->GetTextureSrvHandleGPU());

   const auto& mesh = skybox->GetMesh();
   D3D12_VERTEX_BUFFER_VIEW vbv = mesh.GetVertexBufferView();
   D3D12_INDEX_BUFFER_VIEW ibv = mesh.GetIndexBufferView();
   cmdList->IASetVertexBuffers(0, 1, &vbv);
   cmdList->IASetIndexBuffer(&ibv);
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawIndexedInstanced(mesh.GetIndexCount(), 1, 0, 0, 0);

   // パイプライン状態をリセット
   currentPipelineName_ = "";
   currentPipelineBlendMode_ = BlendMode::kBlendModeNone;
}

void Renderer::SubmitDrawCommand(const DrawCommand& command) {
   RouteDrawCommand(command);
}

void Renderer::RouteDrawCommand(const DrawCommand& command) {
   auto wrapper = std::make_unique<DrawCommandWrapper>(command);
   switch (command.renderPass) {
	  case RenderPass::Transparent:
		 transparentCommands_.push_back(std::move(wrapper));
		 break;
	  case RenderPass::PostProcess:
		 postProcessCommands_.push_back(std::move(wrapper));
		 break;
	  case RenderPass::Opaque:
		 opaqueCommands_.push_back(std::move(wrapper));
		 break;
   }
}

void Renderer::DrawSpline(const std::vector<Vector3>& controlPoints, const Vector4& color, size_t segmentCount, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   if (auto* renderer = SelectLineRenderer(applyPostProcess)) {
	  renderer->DrawSpline(controlPoints, color, segmentCount, activeCamera);
   }
}

void Renderer::DrawGrid(GridPlane plane, float gridSize, int thickLineInterval, int range, bool enableFade, float fadeDistance, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   if (auto* renderer = SelectLineRenderer(applyPostProcess)) {
	  renderer->DrawGrid(activeCamera, plane, gridSize, thickLineInterval, range, enableFade, fadeDistance);
   }
}

void Renderer::DrawSphere(const Vector3& center, float radius, const Vector4& color, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   if (auto* renderer = SelectLineRenderer(applyPostProcess)) {
	  renderer->DrawSphere(center, radius, color, activeCamera);
   }
}

void Renderer::DrawHemisphere(const Vector3& center, float radius, const Vector3& up, const Vector4& color, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   if (auto* renderer = SelectLineRenderer(applyPostProcess)) {
	  renderer->DrawHemisphere(center, radius, up, color, activeCamera);
   }
}

void Renderer::DrawCone(const Vector3& apex, float radius, float height, const Vector3& direction, const Vector4& color, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   if (auto* renderer = SelectLineRenderer(applyPostProcess)) {
	  renderer->DrawCone(apex, radius, height, direction, color, activeCamera);
   }
}

void Renderer::DrawBox(const Vector3& center, const Vector3& size, const Vector4& color, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   if (auto* renderer = SelectLineRenderer(applyPostProcess)) {
	  renderer->DrawBox(center, size, color, activeCamera);
   }
}

void Renderer::DrawCircle(const Vector3& center, float radius, const Vector3& normal, const Vector4& color, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   if (auto* renderer = SelectLineRenderer(applyPostProcess)) {
	  renderer->DrawCircle(center, radius, normal, color, activeCamera);
   }
}

void Renderer::DrawSkeleton(Model* model, float jointRadius, const Vector4& jointColor, const Vector4& boneColor, bool applyPostProcess) {
   if (!model) {
	  return;
   }

   ModelAsset* modelAsset = model->GetComponent<ModelAssetComponent>()->GetModelAsset();
   if (!modelAsset) {
	  return;
   }

   const Skeleton* bindSkeleton = modelAsset->GetBindSkeleton();
   if (!bindSkeleton || bindSkeleton->joints.empty()) {
	  return;
   }

   Skeleton skeletonPose = *bindSkeleton;

   if (auto* animationComponent = model->GetComponent<AnimationComponent>()) {
	  if (assetManager_ && !animationComponent->animationName.empty()) {
		 auto* animationManager = assetManager_->GetAnimationAssetManager();
		 if (animationManager) {
			auto animationAsset = animationManager->GetAnimation(animationComponent->animationName);
			if (animationAsset) {
			   const AnimationClip* clip = nullptr;
			   if (!animationComponent->clipName.empty()) {
				  clip = animationAsset->GetClip(animationComponent->clipName);
			   }
			   if (!clip) {
				  clip = animationAsset->GetDefaultClip();
			   }

			   if (clip) {
				  ApplyAnimation(skeletonPose, *clip, animationComponent->currentTime);
				  skeletonPose.Update();
			   }
			}
		 }
	  }
   }

   const TransformComponent* transformComponent = model->GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }

   Matrix4x4 modelMatrix = MakeAffineMatrix(transformComponent->transform);
   if (transformComponent->useParentMatrix) {
	  modelMatrix = modelMatrix * transformComponent->parentMatrix;
   }

   std::vector<Vector3> jointPositions;
   jointPositions.resize(skeletonPose.joints.size());

   for (const Joint& joint : skeletonPose.joints) {
	  const Matrix4x4 jointWorldMatrix = joint.skeletonSpaceMatrix * modelMatrix;
	  jointPositions[joint.index] = ExtractTranslation(jointWorldMatrix);
	  DrawSphere(jointPositions[joint.index], jointRadius, jointColor, applyPostProcess);
   }

   for (const Joint& joint : skeletonPose.joints) {
	  if (!joint.parent) {
		 continue;
	  }

	  const int32_t parentIndex = *joint.parent;
	  if (parentIndex < 0 || static_cast<size_t>(parentIndex) >= jointPositions.size()) {
		 continue;
	  }

	  DrawLine(jointPositions[parentIndex], jointPositions[joint.index], boneColor, applyPostProcess);
   }
}

void Renderer::EndFrame() {
#ifdef USE_IMGUI
   if (editorController_) {
	  editorController_->BeginEditorFrame();
	  editorController_->ShowPlayModeToolbar();
	  editorController_->ShowAssetWindow();
	  editorController_->ShowInspectorWindow();
	  editorController_->ShowHierarchyWindow();
   }
#endif
   // ラインレンダラーを終了
   lineRenderer_->End();
   postProcessLineRenderer_->End();

   DrawAutoRegisteredModels();
   DrawAutoRegisteredSprites();
   DrawAutoRegisteredParticles();

   // ラインをパス別にフラッシュ
   FlushLineRenderer(lineRenderer_.get(), RenderPass::Opaque);
   FlushLineRenderer(postProcessLineRenderer_.get(), RenderPass::PostProcess);

   // 次のフレームに備えてクリア
   lineRenderer_->Clear();
   postProcessLineRenderer_->Clear();

   // スカイボックスを不透明パス完了後に描画
   DrawAutoRegisteredSkyboxes();

   // --- レンダーパスを順番に実行 ---
   for (const auto& pass : renderPasses_) {
	  if (pass) {
		 pass->Execute(frameCtx_);
	  }
   }

   // オフスクリーンレンダーターゲットをバックバッファに描画
   device_->PreDraw();

#ifdef USE_IMGUI
   bool isDockSpaceVisible = imGuiManager_->IsDockSpaceVisible();
   imGuiManager_->ShowEngineSettings(isDockSpaceVisible);
   if (isDockSpaceVisible) {
	  imGuiManager_->ShowViewport(
		 offscreenRenderTarget_.get(),
		 isSceneHovered_,
		 [this](float viewportX, float viewportY, float viewportWidth, float viewportHeight) {
			if (editorController_) {
			   editorController_->ShowSceneOverlay(viewportX, viewportY, viewportWidth, viewportHeight);
			}
		 });
	  postProcessManager_->ShowImGuiControls();
   } else {
	  DrawFullscreenTriangle(offscreenRenderTarget_->GetSRVHandleGPU());
   }

   imGuiManager_->EndFrame(device_->GetCommandList());
#else
   DrawFullscreenTriangle(offscreenRenderTarget_->GetSRVHandleGPU());
#endif

   device_->PostDraw();

#ifdef USE_IMGUI
   imGuiManager_->PresentPlatformWindows();
#endif
}

void Renderer::DrawAutoRegisteredModels() {
   auto* textureManager = assetManager_ ? assetManager_->GetTextureManager() : nullptr;
   if (!textureManager) {
	  return;
   }

   const auto sceneObjects = CollectSceneObjectsForRender();

   auto* fallbackTexture = textureManager->GetTexture("white1x1");

   for (auto* model : Model::GetRegisteredModels()) {
	  if (!model) {
		 continue;
	  }

    auto* renderComponent = model->GetComponent<RenderComponent>();
	  if (!renderComponent) {
		 continue;
	  }

	  ResolveParentRelationForRender(model, sceneObjects);

	  if (!renderComponent->IsEnabled() || !renderComponent->autoRender || !renderComponent->visible) {
		 continue;
	  }

	  auto* materialComp = model->GetComponent<MaterialComponent>();

	  // Texture は MaterialComponent からのみ取得
	  Texture* texture = nullptr;
	  if (materialComp && !materialComp->GetTextureNames().empty() && !materialComp->GetTextureNames()[0].empty()) {
		 texture = textureManager->GetTexture(materialComp->GetTextureNames()[0]);
	  }
	  if (texture && texture->GetMetadata().IsCubemap()) {
		 texture = nullptr;
	  }
	  if (!texture) {
		 texture = fallbackTexture;
	  }
	  if (!texture) {
		 continue;
	  }

	  if (materialComp && !materialComp->GetEnvironmentTextureName().empty()) {
		 auto* envTex = textureManager->GetTexture(materialComp->GetEnvironmentTextureName());
		 SetEnvironmentTexture(envTex);
	  } else {
		 SetEnvironmentTexture(textureManager->GetLastCubemapTexture());
	  }

	  Draw(model, texture, std::nullopt, renderComponent->applyPostProcess);
   }
}

void Renderer::DrawAutoRegisteredSprites() {
   auto* textureManager = assetManager_ ? assetManager_->GetTextureManager() : nullptr;
   if (!textureManager) {
	  return;
   }

   auto* fallbackTexture = textureManager->GetTexture("white1x1");

   const auto sceneObjects = CollectSceneObjectsForRender();

   for (auto* sprite : Sprite::GetRegisteredSprites()) {
	  if (!sprite) {
		 continue;
	  }

   auto* renderComponent = sprite->GetComponent<RenderComponent>();
	  if (!renderComponent) {
		 continue;
	  }

	  ResolveParentRelationForRender(sprite, sceneObjects);

	  if (!renderComponent->IsEnabled() || !renderComponent->autoRender || !renderComponent->visible) {
		 continue;
	  }

	  auto* materialComp = sprite->GetComponent<MaterialComponent>();

	  // Texture は MaterialComponent からのみ取得
	  Texture* texture = nullptr;
	  if (materialComp && !materialComp->GetTextureNames().empty() && !materialComp->GetTextureNames()[0].empty()) {
		 texture = textureManager->GetTexture(materialComp->GetTextureNames()[0]);
	  }
	  if (texture && texture->GetMetadata().IsCubemap()) {
		 texture = nullptr;
	  }
	  if (!texture) {
		 texture = fallbackTexture;
	  }
	  if (!texture) {
		 continue;
	  }

	  if (renderComponent->renderSpace == RenderComponent::RenderSpace::Screen) {
		 const uint32_t screenWidth = device_ ? device_->GetBackBufferWidth() : Window::kResolutionWidth;
		 const uint32_t screenHeight = device_ ? device_->GetBackBufferHeight() : Window::kResolutionHeight;
		 DrawUI(sprite, texture, sprite->GetScreenAnchorPoint(), std::nullopt, renderComponent->applyPostProcess, screenWidth, screenHeight);
	  } else {
		 Draw(sprite, texture, std::nullopt, renderComponent->applyPostProcess);
	  }
   }
}

void Renderer::DrawAutoRegisteredSkyboxes() {
   for (auto* skybox : Skybox::GetRegisteredSkyboxes()) {
	  if (!skybox) {
		 continue;
	  }
	  DrawSkybox(skybox);
   }
}

void Renderer::DrawAutoRegisteredParticles() {
   for (auto* particleSystem : ParticleSystem::GetRegisteredParticleSystems()) {
	  if (!particleSystem) {
		 continue;
	  }
	  Draw(particleSystem);
   }
}

void Renderer::ExecuteDrawCommands(const std::vector<std::unique_ptr<IDrawCommand>>& commands) {
   for (const auto& icmd : commands) {
	  const DrawCommand& cmd = icmd->GetDrawCommand();
	  switch (cmd.type) {
		 case DrawCommandType::Model:
			modelRenderer_->DrawModel(cmd.modelData, defaultMaterial_.get(), lightManager_,
			   [this](const std::string& name, BlendMode mode) { SetPipeline(name, mode); });
			break;
		 case DrawCommandType::Sprite:
			if (cmd.isUISprite) {
			   uiRenderer_->DrawUISprite(cmd.uiSpriteData, defaultMaterial_.get(),
				  [this](const std::string& name, BlendMode mode) { SetPipeline(name, mode); });
			} else {
			   spriteRenderer_->DrawSprite(cmd.spriteData, defaultMaterial_.get(), lightManager_,
				  [this](const std::string& name, BlendMode mode) { SetPipeline(name, mode); });
			}
			break;
		 case DrawCommandType::Particle:
			particleRenderer_->DrawParticle(cmd.particleData,
			   [this](const std::string& name, BlendMode mode) { SetPipeline(name, mode); },
			   [this]() { InvalidatePipelineBinding(); });
			break;
		 case DrawCommandType::Line:
			DrawLineInternal(cmd.lineData);
			break;
	  }
   }
}

void Renderer::DrawLineInternal(const LineDrawData& lineData) {
   if (!lineData.drawFunc) return;

   // Line3DパイプラインをPipelineManagerから取得
   auto* linePipeline = psoManager_->GetPipeline("Line3D");
   assert(linePipeline != nullptr);

   device_->GetCommandList()->SetPipelineState(linePipeline->GetPipelineState());
   device_->GetCommandList()->SetGraphicsRootSignature(linePipeline->GetRootSignature());

   // 描画関数を実行（ViewProjection行列を渡す）
   lineData.drawFunc(device_->GetCommandList(), lineData.viewProjectionMatrix);
}

void Renderer::Finalize() {
#ifdef USE_IMGUI
   imGuiManager_->Finalize();
#endif
}

void Renderer::SetBlendMode(BlendMode blendMode) {
   currentBlendMode_ = blendMode;
}

void Renderer::InitializeUICamera() {
   Transform uiCameraTransform = {};
   uiCameraTransform.scale = Vector3(1.0f, 1.0f, 1.0f);
   uiCameraTransform.rotation = Vector3(0.0f, 0.0f, 0.0f);
   uiCameraTransform.translation = Vector3(0.0f, 0.0f, 0.0f);

   uiCamera_->Initialize(uiCameraTransform, Camera::ProjectionType::Orthographic);
   uiCamera_->SetNearClip(0.0f);
   uiCamera_->SetFarClip(10000.0f);
   const uint32_t screenWidth = device_ ? device_->GetBackBufferWidth() : Window::kResolutionWidth;
   const uint32_t screenHeight = device_ ? device_->GetBackBufferHeight() : Window::kResolutionHeight;
   SyncUICameraToRenderTarget(screenWidth, screenHeight);
}

void Renderer::SyncUICameraToRenderTarget(uint32_t screenWidth, uint32_t screenHeight) {
   if (!uiCamera_ || screenWidth == 0 || screenHeight == 0) {
	  return;
   }

   uiCamera_->SetOrthographicSize(static_cast<float>(screenWidth), static_cast<float>(screenHeight));
}

void Renderer::DrawFullscreenTriangle(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {
   auto cmdList = device_->GetCommandList();
   auto* fullscreenPipeline = psoManager_->GetPipeline("FullscreenTriangle");
   assert(fullscreenPipeline != nullptr);
   const auto textureSlot = psoManager_->ResolvePipelineRootParameter("FullscreenTriangle", "texture");
   if (!textureSlot.has_value()) {
	  Logger::Error("[Renderer] Failed to resolve FullscreenTriangle texture root slot from PSO JSON.");
	  return;
   }
   cmdList->SetGraphicsRootSignature(fullscreenPipeline->GetRootSignature());
   cmdList->SetPipelineState(fullscreenPipeline->GetPipelineState());
   cmdList->SetGraphicsRootDescriptorTable(textureSlot.value(), textureSrvHandle);
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);
}

void Renderer::SetPipeline(const std::string& pipelineName, BlendMode blendMode) {
   // 同一パイプラインなら DX12 コマンドリストへの再セットをスキップ
   if (currentPipelineName_ == pipelineName && currentPipelineBlendMode_ == blendMode) {
	  return;
   }

   // キャッシュを確認（起動後に一度解決したポインタを再利用）
   const std::string cacheKey = MakePipelineCacheKey(pipelineName, blendMode);
   auto it = pipelineCache_.find(cacheKey);

   PipelineState* pipelineState = nullptr;

   if (it != pipelineCache_.end() && it->second.resolved) {
	  // キャッシュヒット
	  pipelineState = it->second.pso;
   } else {
	  // キャッシュミス → PSOManager に問い合わせてキャッシュに登録
	  pipelineState = psoManager_->GetPipeline(pipelineName, blendMode);
	  if (!pipelineState) {
		 Logger::Error("Failed to get pipeline: " + pipelineName + " with blend mode: " + std::to_string(static_cast<int>(blendMode)));
		 // フォールバック: ブレンドモードなしで再試行
		 pipelineState = psoManager_->GetPipeline(pipelineName, BlendMode::kBlendModeNone);
		 if (!pipelineState) {
			Logger::Error("Failed to get fallback pipeline: " + pipelineName);
			return;
		 }
	  }
	  // 解決結果を記録（nullptr でも resolved=true として二重検索を防ぐ）
	  pipelineCache_[cacheKey] = PipelineHandle{ pipelineState, true };
   }

   if (!pipelineState) {
	  return;
   }

   device_->GetCommandList()->SetGraphicsRootSignature(pipelineState->GetRootSignature());
   device_->GetCommandList()->SetPipelineState(pipelineState->GetPipelineState());

   currentPipelineName_ = pipelineName;
	currentPipelineBlendMode_ = blendMode;
}

void Renderer::InvalidatePipelineBinding() {
	currentPipelineName_.clear();
	currentPipelineBlendMode_ = BlendMode::kBlendModeNone;
}

void Renderer::SetEnvironmentTexture(Texture* texture) {
   if (texture) {
	  activeEnvironmentTextureSrvHandle_ = texture->GetTextureSrvHandleGPU();
   } else {
	  activeEnvironmentTextureSrvHandle_ = {};
   }
}

LineRenderer* Renderer::SelectLineRenderer(bool applyPostProcess) {
   return applyPostProcess ? lineRenderer_.get() : postProcessLineRenderer_.get();
}

void Renderer::FlushLineRenderer(LineRenderer* renderer, RenderPass renderPass) {
   if (!renderer) {
	  return;
   }

   const auto& cameraLineGroups = renderer->GetCameraLineGroups();
   for (const auto& [camera, lines] : cameraLineGroups) {
	  if (lines.empty() || !camera) {
		 continue;
	  }

	  std::vector<LineRenderer::LineInstance> capturedLines = lines;
	  const size_t lineCount = capturedLines.size();
	  auto* lineRendererPtr = renderer;
	  const auto lineTransformSlot = psoManager_->ResolvePipelineRootParameter("Line3D", "transform");
	  if (!lineTransformSlot.has_value()) {
		 Logger::Error("[Renderer] Failed to resolve Line3D transform root slot from PSO JSON.");
		 continue;
	  }
	  Vector3 center = { 0.0f, 0.0f, 0.0f };
	  for (const auto& line : capturedLines) {
		 center += (line.start + line.end) * 0.5f;
	  }
	  center /= static_cast<float>(lineCount);

	  auto drawFunc = [lineRendererPtr, capturedLines, lineCount, lineTransformSlot = lineTransformSlot.value()](ID3D12GraphicsCommandList* cmdList, const Matrix4x4& viewProjMatrix) {
		 if (capturedLines.empty() || !lineRendererPtr) {
			return;
		 }

		 auto* mappedBuffer = lineRendererPtr->GetMappedInstanceBuffer();
		 if (mappedBuffer) {
			memcpy(mappedBuffer, capturedLines.data(), sizeof(LineRenderer::LineInstance) * lineCount);
		 }

		 Matrix4x4 world = MakeIdentity4x4();
		 lineRendererPtr->UpdateMatrix(world, viewProjMatrix);

		 cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		 lineRendererPtr->Draw(cmdList, lineTransformSlot);
		 cmdList->DrawInstanced(2, static_cast<UINT>(lineCount), 0, 0);
		 };

	  DrawCommand cmd = DrawCommand::CreateLine(drawFunc, camera, renderPass, center);
	  SubmitDrawCommand(cmd);
   }
}

RenderPass Renderer::DetermineRenderPass(BlendMode blendMode, bool applyPostProcess) const {
   if (!applyPostProcess) {
	  return RenderPass::PostProcess;
   }

   return (blendMode == BlendMode::kBlendModeNone) ? RenderPass::Opaque : RenderPass::Transparent;
}

void Renderer::SortTransparentCommands() {
   if (transparentCommands_.size() < 2) {
	  return;
   }

   std::stable_sort(transparentCommands_.begin(), transparentCommands_.end(),
	  [](const std::unique_ptr<IDrawCommand>& lhs, const std::unique_ptr<IDrawCommand>& rhs) {
		 const auto lPos = lhs->GetSortPosition();
		 const auto rPos = rhs->GetSortPosition();
		 Camera* lCam = lhs->GetCamera();
		 Camera* rCam = rhs->GetCamera();

		 const bool lHasPos = lPos.has_value() && lCam;
		 const bool rHasPos = rPos.has_value() && rCam;

		 // 位置情報なし → 位置情報ありより手前（先に描画）
		 if (lHasPos != rHasPos) {
			return lHasPos > rHasPos;
		 }

		 // 両方位置情報あり → カメラ距離の遠い順
		 if (lHasPos && rHasPos) {
			const float lDist = (*lPos - lCam->GetPosition()).LengthSquared();
			const float rDist = (*rPos - rCam->GetPosition()).LengthSquared();
			if (lDist != rDist) {
			   return lDist > rDist;
			}
		 }

		 // 距離が同じまたは位置情報なし → 種別優先度順
		 return lhs->GetTypePriority() > rhs->GetTypePriority();
	  });
}

} // namespace GameEngine
