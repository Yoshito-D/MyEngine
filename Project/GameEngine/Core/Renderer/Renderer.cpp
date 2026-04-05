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
#include "Asset/TextureManager.h"
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

struct ValidationGateConfig {
   uint32_t warningThreshold = 5;
   double fallbackRateThreshold = 0.30;
   double minStageMatchRate = 0.60;
   uint32_t warningIncreaseThreshold = 1;
   double fallbackRateIncreaseThreshold = 0.05;
   double stageMatchRateDecreaseThreshold = 0.05;
   std::string source = "default";
};

ValidationGateConfig LoadValidationGateConfig() {
   ValidationGateConfig config{};
   constexpr const char* kConfigPath = "resources/pipelines/validation_gate_config.json";
   std::ifstream ifs(kConfigPath);
   if (!ifs.is_open()) {
	  return config;
   }

   try {
	  nlohmann::json root;
	  ifs >> root;
	  config.warningThreshold = root.value("warningThreshold", config.warningThreshold);
	  config.fallbackRateThreshold = root.value("fallbackRateThreshold", config.fallbackRateThreshold);
	  config.minStageMatchRate = root.value("minStageMatchRate", config.minStageMatchRate);
	  config.warningIncreaseThreshold = root.value("warningIncreaseThreshold", config.warningIncreaseThreshold);
	  config.fallbackRateIncreaseThreshold = root.value("fallbackRateIncreaseThreshold", config.fallbackRateIncreaseThreshold);
	  config.stageMatchRateDecreaseThreshold = root.value("stageMatchRateDecreaseThreshold", config.stageMatchRateDecreaseThreshold);
	  config.source = kConfigPath;
   }
   catch (...) {
   }

   return config;
}

double ComputeStageMatchRate(const GameEngine::PipelineStageMatchInfo& stageInfo) {
   double sumRate = 0.0;
   int stageCount = 0;

   auto accumulate = [&](const GameEngine::ShaderStageMatchInfo& info) {
	  if (!info.hasReflection || info.resourceCount == 0) {
		 return;
	  }
	  sumRate += static_cast<double>(info.matchedByName) / static_cast<double>(info.resourceCount);
	  ++stageCount;
	  };

   accumulate(stageInfo.vertex);
   accumulate(stageInfo.pixel);

   return (stageCount > 0) ? (sumRate / static_cast<double>(stageCount)) : 1.0;
}

GameEngine::Renderer::SchemaValidationStatus ValidateReportWithSchema(
   const nlohmann::json& report,
   const std::filesystem::path& schemaPath) {
   GameEngine::Renderer::SchemaValidationStatus status{};
   status.schemaFile = schemaPath.string();

   nlohmann::json schema;
   std::ifstream ifs(schemaPath);
   if (!ifs.is_open()) {
	  status.passed = false;
	  status.failedKeys.push_back("schema:file_not_found");
	  return status;
   }

   try {
	  ifs >> schema;
   } catch (...) {
	  status.passed = false;
	  status.failedKeys.push_back("schema:parse_error");
	  return status;
   }

   std::function<void(const nlohmann::json&, const nlohmann::json&, const std::string&)> validateNode;
   validateNode = [&](const nlohmann::json& value, const nlohmann::json& schemaNode, const std::string& path) {
	  if (schemaNode.contains("type") && schemaNode["type"].is_string()) {
		 const std::string type = schemaNode["type"].get<std::string>();
		 const bool typeOk =
			(type == "object" && value.is_object()) ||
			(type == "array" && value.is_array()) ||
			(type == "string" && value.is_string()) ||
			(type == "number" && value.is_number()) ||
			(type == "integer" && value.is_number_integer()) ||
			(type == "boolean" && value.is_boolean());
		 if (!typeOk) {
			status.failedKeys.push_back(path + ":type");
			return;
		 }
	  }

	  if (schemaNode.contains("required") && schemaNode["required"].is_array() && value.is_object()) {
		 for (const auto& req : schemaNode["required"]) {
			if (!req.is_string()) {
			   continue;
			}
			const std::string key = req.get<std::string>();
			if (!value.contains(key)) {
			   status.failedKeys.push_back(path + "." + key);
			}
		 }
	  }

	  if (schemaNode.contains("properties") && schemaNode["properties"].is_object() && value.is_object()) {
		 for (const auto& [key, subSchema] : schemaNode["properties"].items()) {
			if (value.contains(key)) {
			   validateNode(value.at(key), subSchema, path + "." + key);
			}
		 }
	  }
   };

   validateNode(report, schema, "$report");
   status.passed = status.failedKeys.empty();
   return status;
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
   modelRenderer_->Initialize(device_, psoManager_.get());
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

   lineRenderer_->Initialize(device_->GetDevice(), 10000);
   postProcessLineRenderer_->Initialize(device_->GetDevice(), 10000);

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

   if (shaderManager_) {
	  const auto stats = shaderManager_->GetResolveStats();
	  reflectionResolveRequestsAtFrameBegin_ = stats.requests;
	  reflectionResolveHitsAtFrameBegin_ = stats.hits;
	  reflectionResolveMissesAtFrameBegin_ = stats.misses;
	  reflectionStatsAtFrameBegin_ = shaderManager_->GetPipelineResolveStats();
	  frameReflectionStatsByPipeline_.clear();
   }
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

   if (shaderManager_) {
	  const auto stats = shaderManager_->GetResolveStats();
	  frameReflectionResolveRequests_ = stats.requests - reflectionResolveRequestsAtFrameBegin_;
	  frameReflectionResolveHits_ = stats.hits - reflectionResolveHitsAtFrameBegin_;
	  frameReflectionResolveFallbacks_ = stats.misses - reflectionResolveMissesAtFrameBegin_;

	  const auto currentPipelineStats = shaderManager_->GetPipelineResolveStats();
	  for (const auto& [pipelineName, current] : currentPipelineStats) {
		 const auto beginIt = reflectionStatsAtFrameBegin_.find(pipelineName);
		 const ResolveStats begin = (beginIt != reflectionStatsAtFrameBegin_.end()) ? beginIt->second : ResolveStats{};
		 ResolveStats delta{};
		 delta.requests = current.requests - begin.requests;
		 delta.hits = current.hits - begin.hits;
		 delta.misses = current.misses - begin.misses;
		 if (delta.requests > 0) {
			frameReflectionStatsByPipeline_[pipelineName] = delta;
		 }
	  }
   }

   UpdateValidationReport();
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
	  ShowReflectionDebugWindow();

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

#ifdef USE_IMGUI
void Renderer::ShowReflectionDebugWindow() {
   ImGui::Begin("Reflection Binding Debug");
   ImGui::Text("Frame Resolve Requests : %llu", frameReflectionResolveRequests_);
   ImGui::Text("Frame Resolve Hits     : %llu", frameReflectionResolveHits_);
   ImGui::Text("Frame Fallbacks        : %llu", frameReflectionResolveFallbacks_);
   ImGui::Separator();
   ImGui::Text("Quality Gate: %s", latestQualityGatePassed_ ? "PASS" : "FAIL");
   ImGui::Text("Validation Warnings: %u", latestValidationWarningCount_);
   ImGui::Text("Fallback Rate: %.2f%%", latestFallbackRate_ * 100.0);
   if (!latestQualityGateFailReasons_.empty()) {
	  ImGui::Text("Fail Reasons:");
	  for (const auto& reason : latestQualityGateFailReasons_) {
		 ImGui::BulletText("%s", reason.c_str());
	  }
   }
   ImGui::Checkbox("Show Failed Pipelines Only", &showOnlyFailedItems_);

   const double hitRate = (frameReflectionResolveRequests_ > 0)
	  ? (static_cast<double>(frameReflectionResolveHits_) / static_cast<double>(frameReflectionResolveRequests_) * 100.0)
	  : 100.0;
   ImGui::Text("Frame Hit Rate         : %.1f%%", hitRate);

	if (ImGui::BeginTabBar("ReflectionValidationTabs")) {
		if (ImGui::BeginTabItem("Schema Validation")) {
			ImGui::Text("Schema Validation: %s", latestSchemaValidationStatus_.passed ? "PASS" : "FAIL");
			ImGui::Text("Schema File: %s", latestSchemaValidationStatus_.schemaFile.c_str());
			if (!latestSchemaValidationStatus_.failedKeys.empty()) {
				ImGui::Text("Failed Keys:");
				for (const auto& key : latestSchemaValidationStatus_.failedKeys) {
					ImGui::BulletText("%s", key.c_str());
				}
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Regression Diffs")) {
			if (ImGui::BeginTable("ReflectionByPipeline", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
				ImGui::TableSetupColumn("Pipeline");
				ImGui::TableSetupColumn("Requests");
				ImGui::TableSetupColumn("Hits");
				ImGui::TableSetupColumn("Fallbacks");
				ImGui::TableSetupColumn("MismatchWarnings");
				ImGui::TableHeadersRow();

				for (const auto& [pipelineName, stats] : frameReflectionStatsByPipeline_) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(pipelineName.c_str());
					const auto diffIt = latestPipelineDiffs_.find(pipelineName);
					const PipelineDiffMetrics diff = (diffIt != latestPipelineDiffs_.end()) ? diffIt->second : PipelineDiffMetrics{};
					const char* warningTrend = diff.warningDelta > 0 ? "↑" : (diff.warningDelta < 0 ? "↓" : "→");
					const char* fallbackTrend = diff.fallbackRateDelta > 0.0 ? "↑" : (diff.fallbackRateDelta < 0.0 ? "↓" : "→");
					const char* stageTrend = diff.stageMatchRateDelta > 0.0 ? "↑" : (diff.stageMatchRateDelta < 0.0 ? "↓" : "→");

					ImGui::TableSetColumnIndex(1); ImGui::Text("%llu", stats.requests);
					ImGui::TableSetColumnIndex(2); ImGui::Text("%llu %s", stats.hits, stageTrend);
					ImGui::TableSetColumnIndex(3); ImGui::Text("%llu %s", stats.misses, fallbackTrend);
					ImGui::TableSetColumnIndex(4);
					const auto* metadata = psoManager_ ? psoManager_->GetPipelineReflectionMetadata(pipelineName) : nullptr;
					ImGui::Text("%u %s", metadata ? metadata->validationWarningCount : 0, warningTrend);
				}

				ImGui::EndTable();
			}

			if (!latestRegressionFailReasons_.empty()) {
				ImGui::Separator();
				ImGui::Text("Regression Fail Reasons:");
				for (const auto& reason : latestRegressionFailReasons_) {
					ImGui::BulletText("%s", reason.c_str());
				}
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Current Fail Items")) {
			if (latestValidationFailItems_.empty()) {
				ImGui::Text("No fail items.");
			} else {
				for (int i = 0; i < static_cast<int>(latestValidationFailItems_.size()); ++i) {
					const auto& item = latestValidationFailItems_[i];
					const std::string failLabel = item.pipeline + "##fail_" + std::to_string(i);
					if (ImGui::Selectable(failLabel.c_str(), selectedFailItemIndex_ == i)) {
						selectedFailItemIndex_ = i;
					}
				}

				if (selectedFailItemIndex_ >= 0 && selectedFailItemIndex_ < static_cast<int>(latestValidationFailItems_.size())) {
					const auto& selected = latestValidationFailItems_[selectedFailItemIndex_];
					ImGui::Separator();
					ImGui::Text("Pipeline: %s", selected.pipeline.c_str());
					ImGui::TextWrapped("Reason: %s", selected.reason.c_str());
					ImGui::Text("Stage Match Rate: %.1f%%", selected.stageMatchRate * 100.0);
					if (!selected.missingSemantics.empty()) {
						ImGui::Text("Missing Semantics:");
						for (const auto& semantic : selected.missingSemantics) {
							ImGui::BulletText("%s", semantic.c_str());
						}
					}
				}
			}
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

   if (ImGui::Button("Dump Root Tables To Log") && shaderManager_) {
	  shaderManager_->LogRootParameterTablesDebug();
   }
   ImGui::End();
}
#endif

void Renderer::UpdateValidationReport() {
   if (!psoManager_ || !shaderManager_) {
	  return;
   }

   const auto psoSummary = psoManager_->GetValidationSummary();
   const auto validationMetadata = psoManager_->GetAllPipelineReflectionMetadata();
   const auto stageMatchInfos = shaderManager_->GetPipelineStageMatchInfos();
   latestValidationWarningCount_ = psoSummary.totalWarnings;
   latestFallbackRate_ = (frameReflectionResolveRequests_ > 0)
	  ? static_cast<double>(frameReflectionResolveFallbacks_) / static_cast<double>(frameReflectionResolveRequests_)
	  : 0.0;

   latestQualityGateFailReasons_.clear();
   latestValidationFailItems_.clear();
   latestPipelineDiffs_.clear();
  latestRegressionFailReasons_.clear();
   const ValidationGateConfig gateConfig = LoadValidationGateConfig();
   const uint32_t kWarningThreshold = gateConfig.warningThreshold;
   const double kFallbackRateThreshold = gateConfig.fallbackRateThreshold;
   const double kMinStageMatchRate = gateConfig.minStageMatchRate;
   const uint32_t kWarningIncreaseThreshold = gateConfig.warningIncreaseThreshold;
   const double kFallbackRateIncreaseThreshold = gateConfig.fallbackRateIncreaseThreshold;
   const double kStageMatchRateDecreaseThreshold = gateConfig.stageMatchRateDecreaseThreshold;

   std::unordered_map<std::string, uint32_t> currentWarningsByPipeline;
   for (const auto& [pipelineName, metadata] : validationMetadata) {
	  currentWarningsByPipeline[pipelineName] = metadata.validationWarningCount;
   }

   std::unordered_map<std::string, double> currentFallbackRateByPipeline;
   for (const auto& [pipelineName, stats] : frameReflectionStatsByPipeline_) {
	  const double rate = (stats.requests > 0)
		 ? static_cast<double>(stats.misses) / static_cast<double>(stats.requests)
		 : 0.0;
	  currentFallbackRateByPipeline[pipelineName] = rate;
   }

   std::unordered_map<std::string, double> currentStageMatchRateByPipeline;
   for (const auto& [pipelineName, stageInfo] : stageMatchInfos) {
	  currentStageMatchRateByPipeline[pipelineName] = ComputeStageMatchRate(stageInfo);
   }

   nlohmann::json previousReport;
   bool hasPreviousReport = false;
   try {
	  std::ifstream ifs("reports/reflection_validation_report.json");
	  if (ifs.is_open()) {
		 ifs >> previousReport;
		 hasPreviousReport = previousReport.is_object();
	  }
   }
   catch (...) {
	  hasPreviousReport = false;
   }

   if (latestValidationWarningCount_ > kWarningThreshold) {
	  latestQualityGateFailReasons_.push_back(
		 "Validation warnings exceeded threshold (" +
		 std::to_string(latestValidationWarningCount_) + "/" + std::to_string(kWarningThreshold) + ")");
   }
   if (latestFallbackRate_ > kFallbackRateThreshold) {
	  latestQualityGateFailReasons_.push_back(
		 "Fallback rate exceeded threshold (" +
		 std::to_string(static_cast<int>(latestFallbackRate_ * 100.0)) + "%/" +
		 std::to_string(static_cast<int>(kFallbackRateThreshold * 100.0)) + "%)");
   }

   for (const auto& [pipelineName, metadata] : validationMetadata) {
	  double stageMatchRate = 1.0;
	  if (auto it = stageMatchInfos.find(pipelineName); it != stageMatchInfos.end()) {
		 stageMatchRate = ComputeStageMatchRate(it->second);
	  }

	  if (!metadata.missingSemantics.empty() || stageMatchRate < kMinStageMatchRate) {
		 ValidationFailItem item{};
		 item.pipeline = pipelineName;
		 item.missingSemantics = metadata.missingSemantics;
		 item.stageMatchRate = stageMatchRate;
		 item.reason = "pipeline=" + pipelineName +
			", missingSemantics=" + std::to_string(metadata.missingSemantics.size()) +
			", stageMatchRate=" + std::to_string(stageMatchRate);
		 latestValidationFailItems_.push_back(item);

		 latestQualityGateFailReasons_.push_back(
			"Pipeline " + pipelineName + " failed: missingSemantics=" +
			std::to_string(metadata.missingSemantics.size()) +
			", stageMatchRate=" + std::to_string(static_cast<int>(stageMatchRate * 100.0)) + "%");
	  }
   }

   auto getPreviousPipelineWarning = [&](const std::string& pipelineName) -> uint32_t {
	  if (!hasPreviousReport || !previousReport.contains("pso") || !previousReport["pso"].contains("pipelines") || !previousReport["pso"]["pipelines"].is_array()) {
		 return 0;
	  }
	  for (const auto& entry : previousReport["pso"]["pipelines"]) {
		 if (!entry.is_object()) {
			continue;
		 }
		 if (entry.value("name", std::string{}) == pipelineName) {
			return entry.value("validationWarningCount", 0u);
		 }
	  }
	  return 0;
	  };

   auto getPreviousPipelineFallbackRate = [&](const std::string& pipelineName) -> double {
	  if (!hasPreviousReport || !previousReport.contains("renderer") || !previousReport["renderer"].contains("frameByPipeline")) {
		 return 0.0;
	  }
	  const auto& byPipeline = previousReport["renderer"]["frameByPipeline"];
	  if (!byPipeline.contains(pipelineName)) {
		 return 0.0;
	  }
	  return byPipeline[pipelineName].value("fallbackRate", 0.0);
	  };

   auto getPreviousPipelineStageMatchRate = [&](const std::string& pipelineName) -> double {
	  if (!hasPreviousReport || !previousReport.contains("shader") || !previousReport["shader"].contains("stageMatches")) {
		 return 1.0;
	  }
	  const auto& stageMatches = previousReport["shader"]["stageMatches"];
	  if (!stageMatches.contains(pipelineName)) {
		 return 1.0;
	  }
	  return stageMatches[pipelineName].value("averageMatchRate", 1.0);
	  };

   nlohmann::json diffByPipeline = nlohmann::json::object();
   for (const auto& [pipelineName, currentWarningCount] : currentWarningsByPipeline) {
	  const uint32_t previousWarningCount = getPreviousPipelineWarning(pipelineName);
	  const double currentFallbackRate = currentFallbackRateByPipeline.contains(pipelineName) ? currentFallbackRateByPipeline[pipelineName] : 0.0;
	  const double previousFallbackRate = getPreviousPipelineFallbackRate(pipelineName);
	  const double currentStageRate = currentStageMatchRateByPipeline.contains(pipelineName) ? currentStageMatchRateByPipeline[pipelineName] : 1.0;
	  const double previousStageRate = getPreviousPipelineStageMatchRate(pipelineName);

	  PipelineDiffMetrics diff{};
	  diff.warningDelta = static_cast<int>(currentWarningCount) - static_cast<int>(previousWarningCount);
	  diff.fallbackRateDelta = currentFallbackRate - previousFallbackRate;
	  diff.stageMatchRateDelta = currentStageRate - previousStageRate;
	  latestPipelineDiffs_[pipelineName] = diff;

	  diffByPipeline[pipelineName] = {
		  {"warningDelta", diff.warningDelta},
		  {"fallbackRateDelta", diff.fallbackRateDelta},
		  {"stageMatchRateDelta", diff.stageMatchRateDelta}
	  };

	  if (hasPreviousReport) {
		 if (diff.warningDelta > static_cast<int>(kWarningIncreaseThreshold)) {
             const std::string reason = "Pipeline " + pipelineName + " warning regression: +" + std::to_string(diff.warningDelta);
				latestQualityGateFailReasons_.push_back(reason);
				latestRegressionFailReasons_.push_back(reason);
		 }
		 if (diff.fallbackRateDelta > kFallbackRateIncreaseThreshold) {
               const std::string reason = "Pipeline " + pipelineName + " fallback regression: +" + std::to_string(static_cast<int>(diff.fallbackRateDelta * 100.0)) + "%";
				latestQualityGateFailReasons_.push_back(reason);
				latestRegressionFailReasons_.push_back(reason);
		 }
		 if ((-diff.stageMatchRateDelta) > kStageMatchRateDecreaseThreshold) {
               const std::string reason = "Pipeline " + pipelineName + " stage match regression: " + std::to_string(static_cast<int>(diff.stageMatchRateDelta * 100.0)) + "%";
				latestQualityGateFailReasons_.push_back(reason);
				latestRegressionFailReasons_.push_back(reason);
		 }
	  }
   }

   latestQualityGatePassed_ = latestQualityGateFailReasons_.empty();

   nlohmann::json report = nlohmann::json::object();
 report["version"] = "1.1.0";
	report["schemaVersion"] = "1.1.0";
	report["compatibilityPolicy"] = {
		{"backwardCompatibleRange", ">=1.0.0 <2.0.0"},
		{"note", "Minor version upgrades remain backward compatible."}
	};
   report["schema"] = {
     {"requiredTopLevelFields", nlohmann::json::array({"version", "schemaVersion", "compatibilityPolicy", "schema", "schemaValidation", "qualityGate", "diff", "pso", "shader", "renderer"})},
	   {"qualityGateRequiredFields", nlohmann::json::array({"passed", "warningThreshold", "fallbackRateThreshold", "warningCount", "fallbackRate", "failReasons"})}
   };
   report["diff"] = {
		{"previousReportFound", hasPreviousReport},
		{"warningIncreaseThreshold", kWarningIncreaseThreshold},
		{"fallbackRateIncreaseThreshold", kFallbackRateIncreaseThreshold},
		{"stageMatchRateDecreaseThreshold", kStageMatchRateDecreaseThreshold},
		{"byPipeline", diffByPipeline}
   };
   report["qualityGate"] = {
	   {"passed", latestQualityGatePassed_},
	   {"warningThreshold", kWarningThreshold},
	   {"fallbackRateThreshold", kFallbackRateThreshold},
	   {"minStageMatchRate", kMinStageMatchRate},
	   {"warningIncreaseThreshold", kWarningIncreaseThreshold},
	   {"fallbackRateIncreaseThreshold", kFallbackRateIncreaseThreshold},
	   {"stageMatchRateDecreaseThreshold", kStageMatchRateDecreaseThreshold},
	   {"configSource", gateConfig.source},
	   {"warningCount", latestValidationWarningCount_},
	   {"fallbackRate", latestFallbackRate_},
	 {"failReasons", latestQualityGateFailReasons_},
	   {"failItems", [&]() {
		   nlohmann::json items = nlohmann::json::array();
		   for (const auto& item : latestValidationFailItems_) {
			   items.push_back({
				   {"pipeline", item.pipeline},
				   {"reason", item.reason},
				   {"missingSemantics", item.missingSemantics},
				   {"stageMatchRate", item.stageMatchRate}
			   });
		   }
		   return items;
	   }()}
   };

   report["pso"] = psoManager_->BuildValidationReportJson();
   nlohmann::json stageMatchesJson = nlohmann::json::object();
   for (const auto& [pipelineName, stageInfo] : stageMatchInfos) {
	  const double averageMatchRate = ComputeStageMatchRate(stageInfo);
	  stageMatchesJson[pipelineName] = {
		  {"vertex", {
			  {"hasReflection", stageInfo.vertex.hasReflection},
			  {"resourceCount", stageInfo.vertex.resourceCount},
			  {"matchedByName", stageInfo.vertex.matchedByName}
		  }},
		  {"pixel", {
			  {"hasReflection", stageInfo.pixel.hasReflection},
			  {"resourceCount", stageInfo.pixel.resourceCount},
			  {"matchedByName", stageInfo.pixel.matchedByName}
		}},
		  {"averageMatchRate", averageMatchRate}
	  };
   }

   const auto resolveStats = shaderManager_->GetResolveStats();
   nlohmann::json pipelineResolveStatsJson = nlohmann::json::object();
   for (const auto& [pipelineName, stats] : shaderManager_->GetPipelineResolveStats()) {
	  pipelineResolveStatsJson[pipelineName] = {
		  {"requests", stats.requests},
		  {"hits", stats.hits},
		  {"misses", stats.misses}
	  };
   }

   report["shader"] = {
	   {"stageMatches", stageMatchesJson},
	   {"resolveStats", {
		   {"requests", resolveStats.requests},
		   {"hits", resolveStats.hits},
		   {"misses", resolveStats.misses}
	   }},
	   {"pipelineResolveStats", pipelineResolveStatsJson}
   };

   nlohmann::json rendererByPipeline = nlohmann::json::object();
   for (const auto& [pipelineName, stats] : frameReflectionStatsByPipeline_) {
	  const double fallbackRate = (stats.requests > 0)
		 ? static_cast<double>(stats.misses) / static_cast<double>(stats.requests)
		 : 0.0;
	  rendererByPipeline[pipelineName] = {
		  {"requests", stats.requests},
		  {"hits", stats.hits},
	   {"fallbacks", stats.misses},
		  {"fallbackRate", fallbackRate}
	  };
   }
   report["renderer"] = {
	   {"frameResolveRequests", frameReflectionResolveRequests_},
	   {"frameResolveHits", frameReflectionResolveHits_},
	   {"frameFallbacks", frameReflectionResolveFallbacks_},
	   {"frameByPipeline", rendererByPipeline}
   };

	// スキーマ自己検証（暫定値でキーを確保してから検証）
	report["schemaValidation"] = {
		{"passed", true},
		{"schemaFile", "resources/pipelines/reflection_validation_report.schema.json"},
		{"failedKeys", nlohmann::json::array()}
	};
	latestSchemaValidationStatus_ = ValidateReportWithSchema(report, "resources/pipelines/reflection_validation_report.schema.json");
	report["schemaValidation"] = {
		{"passed", latestSchemaValidationStatus_.passed},
		{"schemaFile", latestSchemaValidationStatus_.schemaFile},
		{"failedKeys", latestSchemaValidationStatus_.failedKeys}
	};

   // PSO単体レポート（CI個別参照用）
   psoManager_->SaveValidationReportJson("reports/pso_validation_report.json");

   try {
	  const std::filesystem::path outPath = "reports/reflection_validation_report.json";
	  std::filesystem::create_directories(outPath.parent_path());
	  std::ofstream ofs(outPath);
	  if (ofs.is_open()) {
		 ofs << report.dump(2);
	  }
   }
   catch (...) {
   }
}

} // namespace GameEngine