#include "pch.h"
#include "Renderer.h"
#include "Graphics/GraphicsDevice.h"
#include "Model/Model.h"
#include "Camera/Camera.h"
#include "Graphics/RootSignature.h"
#include "Graphics/ShaderCompiler.h"
#include "Graphics/Mesh.h"
#include "Graphics/TransformationMatrix.h"
#include "Graphics/DirectionalLight.h"
#include "Graphics/PointLight.h"
#include "Graphics/SpotLight.h"
#include "Graphics/AreaLight.h"
#include "Effect/ParticleSystem.h"
#include "PostProcess/Grayscale.h"
#include "PostProcess/RadialBlur.h"
#include "PostProcess/GaussBlur.h"
#include "PostProcess/Vignette.h"
#include"PostProcess/ChromaticAberration.h"
#include"PostProcess/ShockWave.h"
#include "PostProcess/Pixelation.h"
#include "Utility/MathUtils.h"
#include "Asset/AssetManager.h"
#include "Asset/AnimationAssetManager.h"
#include "Asset/TextureManager.h"
#include "Component/AnimationComponent.h"
#include "Component/RenderComponent.h"
#include "RootBindingSlots.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <tuple>
#include <unordered_set>

#ifdef USE_IMGUI
#include "RendererEditorController.h"
#include "externals/imgui/imgui.h"
#endif

namespace {
Logger& log_ = Logger::GetInstance();

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

   auto* transformComponent = object->GetTransformComponent();
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

   const auto* parentTransform = parentObject->GetTransformComponent();
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

   offscreenRenderTarget_->Initialize(device_);

   // シェーダーマネージャーの初期化
   shaderManager_->Initialize(device_);

   // パイプラインマネージャーの初期化
   psoManager_->Initialize(device_, shaderManager_.get());

   // 専門レンダラーの初期化
   modelRenderer_->Initialize(device_, psoManager_.get(), assetManager_);
   spriteRenderer_->Initialize(device_, psoManager_.get());
   particleRenderer_->Initialize(device_, psoManager_.get());
   // UIRendererは後でuiCamera_初期化後に設定

   // パイプライン定義をJSONから読み込み（フォールバック付き）
   if (!psoManager_->LoadPipelineDefinitions(L"resources/pipelines/pipeline_registry.json", offscreenRenderTarget_->GetFormat())) {
	  // JSONロードに失敗した場合は事前定義を使用
	  log_.Log("Failed to load pipeline definitions from JSON, using predefined pipelines");
	  psoManager_->CreatePredefinedPipelines(offscreenRenderTarget_.get());
   } else {
	  log_.Log("Successfully loaded pipeline definitions from JSON");
   }

   // スキニング用定義をJSONから読み込み
   if (!psoManager_->LoadPipelineDefinitions(L"resources/pipelines/skinning_pipeline_registry.json", offscreenRenderTarget_->GetFormat())) {
	  log_.Log("Failed to load skinning pipeline definitions from JSON", Logger::LogLevel::Error);
   }

   lineRenderer_->Initialize(device_->GetDevice(), 100000);
   postProcessLineRenderer_->Initialize(device_->GetDevice(), 100000);

   // UI描画専用カメラの初期化
   InitializeUICamera();

   // UIRendererの初期化（uiCamera_を渡す）
   uiRenderer_->Initialize(device_, psoManager_.get(), uiCamera_.get(), spriteRenderer_.get(), lightManager_);

   // PostProcessManagerを初期化（PipelineManagerを渡す）
   postProcessManager_->Initialize(device_, offscreenRenderTarget_.get(), psoManager_.get());

   // ポストプロセス効果をJSONから読み込み（フォールバック付き）
   if (!postProcessManager_->LoadEffectsFromJson(L"resources/postprocess/postprocess_registry.json")) {
	  // JSONロードに失敗した場合は事前定義を使用
	  log_.Log("Failed to load post-process effects from JSON, using predefined effects");
	  postProcessManager_->RegisterPredefinedEffects();
   } else {
	  log_.Log("Successfully loaded post-process effects from JSON");
   }

   shaderManager_->LogRootParameterTablesDebug();

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

   if (!model->GetModelAsset()) return;

   std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> handles = { texture->GetTextureSrvHandleGPU() };

   // 行列を更新
   model->UpdateMatrix(activeCamera);

   // ブレンドモードの決定（引数で指定されていない場合は現在のモードを使用）
   BlendMode effectiveBlendMode = blendMode.value_or(currentBlendMode_);

   // 描画パスの決定
   RenderPass renderPass = DetermineRenderPass(effectiveBlendMode, applyPostProcess);

   DrawCommand cmd = DrawCommand::CreateModel(model, handles, activeCamera, effectiveBlendMode, renderPass);
   SubmitDrawCommand(cmd);
}

void Renderer::Draw(Model* model, const std::vector<Texture*>& textures, std::optional<BlendMode> blendMode, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);
   assert(model != nullptr);
   assert(!textures.empty());

   if (!model->GetModelAsset()) return;

   // TextureポインタからSRVハンドルに変換
   std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> textureSrvHandles;
   textureSrvHandles.reserve(textures.size());
   for (const auto& texture : textures) {
	  assert(texture != nullptr);
	  textureSrvHandles.push_back(texture->GetTextureSrvHandleGPU());
   }

   // 行列を更新
   model->UpdateMatrix(activeCamera);

   // ブレンドモードの決定（引数で指定されていない場合は現在のモードを使用）
   BlendMode effectiveBlendMode = blendMode.value_or(currentBlendMode_);

   // 描画パスの決定
   RenderPass renderPass = DetermineRenderPass(effectiveBlendMode, applyPostProcess);

   DrawCommand cmd = DrawCommand::CreateModel(model, textureSrvHandles, activeCamera, effectiveBlendMode, renderPass);
   SubmitDrawCommand(cmd);
}

void Renderer::Draw(Sprite* sprite, Texture* texture, std::optional<BlendMode> blendMode, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);
   assert(sprite != nullptr);
   assert(texture != nullptr);

   sprite->Update(activeCamera, texture);

   // ブレンドモードの決定（引数で指定されていない場合は現在のモードを使用）
   BlendMode effectiveBlendMode = blendMode.value_or(currentBlendMode_);

   // 描画パスの決定
   RenderPass renderPass = DetermineRenderPass(effectiveBlendMode, applyPostProcess);

   DrawCommand cmd = DrawCommand::CreateSprite(sprite, texture, texture->GetTextureSrvHandleGPU(),
	  activeCamera, effectiveBlendMode, renderPass);
   SubmitDrawCommand(cmd);
}

void Renderer::Draw(ParticleSystem* particleSystem, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);
   assert(particleSystem != nullptr);

   // アクティブなパーティクルがない場合は描画しない
   uint32_t activeCount = particleSystem->GetActiveParticleCount();
   if (activeCount == 0) return;

   particleSystem->UpdateMatrix(activeCamera);

   // パーティクルは通常加算ブレンドを使用
   BlendMode blendMode = BlendMode::kBlendModeAdd;
   RenderPass renderPass = applyPostProcess ? RenderPass::Transparent : RenderPass::PostProcess;

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

void Renderer::SubmitDrawCommand(const DrawCommand& command) {
   RouteDrawCommand(command);
}

void Renderer::RouteDrawCommand(const DrawCommand& command) {
   switch (command.renderPass) {
	  case RenderPass::Transparent:
		 transparentCommands_.push_back(command);
		 break;
	  case RenderPass::PostProcess:
		 postProcessCommands_.push_back(command);
		 break;
	  case RenderPass::Opaque: {
		 opaqueCommands_.push_back(command);
		 break;
	  }
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

   ModelAsset* modelAsset = model->GetModelAsset();
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

   const TransformComponent* transformComponent = model->GetTransformComponent();
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
   DrawAutoRegisteredModels();
   DrawAutoRegisteredSprites();

   // ラインレンダラーを終了
   lineRenderer_->End();
   postProcessLineRenderer_->End();

   // ラインをパス別にフラッシュ
   FlushLineRenderer(lineRenderer_.get(), RenderPass::Opaque);
   FlushLineRenderer(postProcessLineRenderer_.get(), RenderPass::PostProcess);

   // 次のフレームに備えてクリア
   lineRenderer_->Clear();
   postProcessLineRenderer_->Clear();

   // 不透明オブジェクトを描画（ポストプロセス前）
   ExecuteDrawCommands(opaqueCommands_);

   // 半透明オブジェクトを描画（ポストプロセス前）
   SortTransparentCommands();
   ExecuteDrawCommands(transparentCommands_);

   // オフスクリーンレンダーターゲットの描画を終了
   offscreenRenderTarget_->PostDraw();

   // PostProcessManagerを使用してポストプロセスを適用
   postProcessManager_->ApplyEffects(offscreenRenderTarget_->GetSRVHandleGPU());

   // ポストプロセス後の描画を実行
   if (!postProcessCommands_.empty()) {
	  offscreenRenderTarget_->PreDrawWithoutClear(true);

	  currentPipelineName_ = "";
	  currentPipelineBlendMode_ = BlendMode::kBlendModeNone;

	  ExecuteDrawCommands(postProcessCommands_);
	  offscreenRenderTarget_->PostDraw();
   }

   // バックバッファに描画開始
   device_->PreDraw();

#ifdef USE_IMGUI
   // エンジン設定ウィンドウを表示
   bool isDockSpaceVisible = imGuiManager_->IsDockSpaceVisible();
   imGuiManager_->ShowEngineSettings(isDockSpaceVisible);
   if (isDockSpaceVisible) {
	  // ビューポートを表示
	  imGuiManager_->ShowViewport(offscreenRenderTarget_.get(), isSceneHovered_);

	  if (editorController_) {
		 editorController_->ShowSceneEditorWindow();
		 editorController_->ShowHierarchyWindow();
		 editorController_->ShowInspectorWindow();
	  }

	  // PostProcessManagerのImGuiコントロールを表示
	  postProcessManager_->ShowImGuiControls();

   } else {
	  // UI込みのオフスクリーンレンダーターゲットをバックバッファに描画
	  DrawFullscreenTriangle(offscreenRenderTarget_->GetSRVHandleGPU());
   }

   imGuiManager_->EndFrame(device_->GetCommandList());
#else
   // UI込みのオフスクリーンレンダーターゲットをバックバッファに描画
   DrawFullscreenTriangle(offscreenRenderTarget_->GetSRVHandleGPU());
#endif

   device_->PostDraw();
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

	  auto* renderComponent = model->GetRenderComponent();
	  if (!renderComponent) {
		 continue;
	  }

	  ResolveParentRelationForRender(model, sceneObjects);

	  if (!renderComponent->IsEnabled() || !renderComponent->autoRender || !renderComponent->visible) {
		 continue;
	  }

	  Texture* texture = nullptr;
	  if (!renderComponent->textureName.empty()) {
		 texture = textureManager->GetTexture(renderComponent->textureName);
	  }
	  if (!texture) {
		 texture = fallbackTexture;
	  }
	  if (!texture) {
		 continue;
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

	  auto* renderComponent = sprite->GetRenderComponent();
	  if (!renderComponent) {
		 continue;
	  }

	  ResolveParentRelationForRender(sprite, sceneObjects);

	  if (!renderComponent->IsEnabled() || !renderComponent->autoRender || !renderComponent->visible) {
		 continue;
	  }

	  Texture* texture = nullptr;
	  if (!renderComponent->textureName.empty()) {
		 texture = textureManager->GetTexture(renderComponent->textureName);
	  }
	  if (!texture) {
		 texture = fallbackTexture;
	  }
	  if (!texture) {
		 continue;
	  }

	  Draw(sprite, texture, std::nullopt, renderComponent->applyPostProcess);
   }
}



void Renderer::ExecuteDrawCommands(const std::vector<DrawCommand>& commands) {
   for (const auto& cmd : commands) {
	  switch (cmd.type) {
		 case DrawCommandType::Model:
			modelRenderer_->DrawModel(cmd.modelData, defaultMaterial_.get(), lightManager_,
			   [this](const std::string& name, BlendMode mode) { SetPipeline(name, mode); });
			break;
		 case DrawCommandType::Sprite:
			// isUISpriteフラグでUIスプライトか通常のスプライトか判定
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
			   [this](const std::string& name, BlendMode mode) { SetPipeline(name, mode); });
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
   uiCamera_->Update();
}

void Renderer::DrawFullscreenTriangle(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle) {
   auto cmdList = device_->GetCommandList();
   auto* fullscreenPipeline = psoManager_->GetPipeline("FullscreenTriangle");
   assert(fullscreenPipeline != nullptr);
   const UINT textureSlot = shaderManager_
	  ? shaderManager_->ResolvePipelineRootParameter("FullscreenTriangle", "texture").value_or(RootBindingSlots::FullscreenTriangle::kTexture)
	  : RootBindingSlots::FullscreenTriangle::kTexture;
   cmdList->SetGraphicsRootSignature(fullscreenPipeline->GetRootSignature());
   cmdList->SetPipelineState(fullscreenPipeline->GetPipelineState());
   cmdList->SetGraphicsRootDescriptorTable(textureSlot, textureSrvHandle);
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);
}

void Renderer::SetPipeline(const std::string& pipelineName, BlendMode blendMode) {
   // パイプラインが既に設定されていて、同じものなら何もしない
   if (currentPipelineName_ == pipelineName && currentPipelineBlendMode_ == blendMode) {
	  return;
   }

   auto* pipelineState = psoManager_->GetPipeline(pipelineName, blendMode);
   if (!pipelineState) {
	  log_.Log("Failed to get pipeline: " + pipelineName + " with blend mode: " + std::to_string(static_cast<int>(blendMode)), Logger::LogLevel::Error);
	  // フォールバック: ブレンドモードなしで再試行
	  pipelineState = psoManager_->GetPipeline(pipelineName, BlendMode::kBlendModeNone);
	  if (!pipelineState) {
		 log_.Log("Failed to get fallback pipeline: " + pipelineName, Logger::LogLevel::Error);
		 assert(false && "Pipeline not found");
		 return;
	  }
   }

   device_->GetCommandList()->SetGraphicsRootSignature(pipelineState->GetRootSignature());
   device_->GetCommandList()->SetPipelineState(pipelineState->GetPipelineState());

   currentPipelineName_ = pipelineName;
   currentPipelineBlendMode_ = blendMode;
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
	  const UINT lineTransformSlot = shaderManager_
		 ? shaderManager_->ResolvePipelineRootParameter("Line3D", "transform").value_or(RootBindingSlots::Line3D::kTransform)
		 : RootBindingSlots::Line3D::kTransform;
	  Vector3 center = { 0.0f, 0.0f, 0.0f };
	  for (const auto& line : capturedLines) {
		 center += (line.start + line.end) * 0.5f;
	  }
	  center /= static_cast<float>(lineCount);

	  auto drawFunc = [lineRendererPtr, capturedLines, lineCount, lineTransformSlot](ID3D12GraphicsCommandList* cmdList, const Matrix4x4& viewProjMatrix) {
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

   auto getTypePriority = [](DrawCommandType type) {
	  switch (type) {
		 case DrawCommandType::Model: return 4;
		 case DrawCommandType::Sprite: return 3;
		 case DrawCommandType::Particle: return 2;
		 case DrawCommandType::Line: return 1;
		 default: return 0;
	  }
	  };

   auto getCommandCamera = [](const DrawCommand& cmd) -> Camera* {
	  switch (cmd.type) {
		 case DrawCommandType::Model:
			return cmd.modelData.camera;
		 case DrawCommandType::Sprite:
			return cmd.isUISprite ? nullptr : cmd.spriteData.camera;
		 case DrawCommandType::Particle:
			return cmd.particleData.camera;
		 case DrawCommandType::Line:
			return cmd.lineData.camera;
		 default:
			return nullptr;
	  }
	  };

   auto getCommandPosition = [](const DrawCommand& cmd) -> std::optional<Vector3> {
	  switch (cmd.type) {
		 case DrawCommandType::Model:
			if (cmd.modelData.model) {
			   return cmd.modelData.model->GetPosition();
			}
			break;
		 case DrawCommandType::Sprite:
			if (!cmd.isUISprite && cmd.spriteData.sprite) {
			   return cmd.spriteData.sprite->GetTransform().translation;
			}
			break;
		 case DrawCommandType::Line:
			if (cmd.lineData.sortPosition) {
			   return cmd.lineData.sortPosition;
			}
			break;
		 default:
			break;
	  }

	  return std::nullopt;
	  };

   auto getSortInfo = [&](const DrawCommand& cmd) {
	  auto position = getCommandPosition(cmd);
	  Camera* camera = getCommandCamera(cmd);
	  const int typePriority = getTypePriority(cmd.type);
	  if (!position || !camera) {
		 return std::tuple<int, int, float>{ 0, typePriority, 0.0f };
	  }

	  const Vector3 delta = *position - camera->GetPosition();
	  return std::tuple<int, int, float>{ 1, typePriority, delta.LengthSquared() };
	  };

   std::stable_sort(transparentCommands_.begin(), transparentCommands_.end(),
	  [&](const DrawCommand& lhs, const DrawCommand& rhs) {
		 const auto lhsInfo = getSortInfo(lhs);
		 const auto rhsInfo = getSortInfo(rhs);

		 if (std::get<0>(lhsInfo) != std::get<0>(rhsInfo)) {
			return std::get<0>(lhsInfo) > std::get<0>(rhsInfo);
		 }

		 if (std::get<2>(lhsInfo) != std::get<2>(rhsInfo)) {
			return std::get<2>(lhsInfo) > std::get<2>(rhsInfo);
		 }

		 return std::get<1>(lhsInfo) > std::get<1>(rhsInfo);
	  });
}

} // namespace GameEngine