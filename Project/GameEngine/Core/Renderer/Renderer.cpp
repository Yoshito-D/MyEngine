#include "pch.h"
#include "Renderer.h"
#include "Graphics/GraphicsDevice.h"
#include "Model/Model.h"
#include "Camera/Camera.h"
#include "Graphics/RootSignature.h"
#include "Graphics/ShaderCompiler.h"
#include "Graphics/Mesh.h"
#include "Graphics/ResourceHelper.h"
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
#include "Asset/Font/FontManager.h"
#include "Component/Model/AnimationComponent.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/MeshComponent.h"
#include "RenderBootstrapper.h"
#include "Object/Skybox/Skybox.h"
#include "Object/Text/UIText.h"
#include "Component/UI/UITextComponent.h"
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

constexpr float kUiCameraFarClip = 10000.0f;

GameEngine::Vector3 ExtractTranslation(const GameEngine::Matrix4x4& matrix) {
   return GameEngine::Vector3(matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]);
}

void ResolveParentRelationForRender(GameEngine::Object* object) {
   if (!object) {
	  return;
   }

   auto* transformComponent = object->GetComponent<GameEngine::TransformComponent>();
   if (!transformComponent) {
	  return;
   }

   if (object->GetParentEntityId().empty() && !transformComponent->parentObjectName.empty()) {
      // 旧シーン形式の名前参照を、同名衝突に強いEntityId参照へ描画時に移行する。
      // 解決できた場合だけ旧フィールドを消し、未解決データは後続フレームへ残す。
	  if (auto* legacyParent = GameEngine::Object::FindByObjectName(transformComponent->parentObjectName)) {
		 object->SetParentEntityId(legacyParent->GetEntityId());
		 transformComponent->parentObjectName.clear();
	  }
   }

   if (object->GetParentEntityId().empty()) {
	  transformComponent->useParentMatrix = false;
	  transformComponent->parentMatrix = GameEngine::MakeIdentity4x4();
	  return;
   }

   if (!GameEngine::Object::FindByEntityId(object->GetParentEntityId())) {
      // 親が削除済みでも前フレームの行列を使い続けないよう、親適用を明示的に解除する。
	  transformComponent->useParentMatrix = false;
	  transformComponent->parentMatrix = GameEngine::MakeIdentity4x4();
	  return;
   }

   transformComponent->useParentMatrix = true;
   transformComponent->parentMatrix = object->GetParentWorldMatrix();
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
   // Bootstrapperが生成・接続する描画サブシステムを一つのコンテキストへ集約する。
   // Renderer側は所有ポインタを維持し、初期化順やJSONロードの成否だけを委譲する。
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
   context.textRenderer = textRenderer_.get();
   context.lineRenderer = lineRenderer_.get();
   context.postProcessLineRenderer = postProcessLineRenderer_.get();
   context.uiCamera = uiCamera_.get();
   context.postProcessManager = postProcessManager_.get();

   if (!renderBootstrapper_->Initialize(context)) {
	  Logger::Error("[Renderer] Render bootstrap failed. Rendering passes were not created.");
	  return;
   }

   sceneTransitionHorizontalConstantBuffer_ =
      ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(SceneTransitionConstants));
   // シーン遷移は横方向ぼかしと縦方向ぼかし＋暗転合成の2パスに分ける。
   // 同じ構造体を別バッファで保持し、各パス固有の方向と合成フラグを固定する。
   sceneTransitionHorizontalConstantBuffer_->Map(
      0,
      nullptr,
      reinterpret_cast<void**>(&sceneTransitionHorizontalConstants_));
   sceneTransitionHorizontalConstants_->blurDirection[0] = 1.0f;
   sceneTransitionHorizontalConstants_->blurDirection[1] = 0.0f;
   sceneTransitionHorizontalConstants_->applyComposite = 0;

   sceneTransitionConstantBuffer_ =
      ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(SceneTransitionConstants));
   sceneTransitionConstantBuffer_->Map(
      0,
      nullptr,
      reinterpret_cast<void**>(&sceneTransitionConstants_));
   sceneTransitionConstants_->blurDirection[0] = 0.0f;
   sceneTransitionConstants_->blurDirection[1] = 1.0f;
   sceneTransitionConstants_->applyComposite = 1;
   SetSceneTransitionOpacity(0.0f);

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
   // 色を書き込む順序そのものが合成結果を決めるため、不透明→透明→ポスト処理の順を固定する。
   AddPass(std::make_unique<OpaquePass>());
   AddPass(std::make_unique<TransparentPass>());
   AddPass(std::make_unique<PostEffectPass>(offscreenRenderTarget_.get()));

   // パス間で共有する依存先とコマンド配列のアドレスを一度だけ束縛する。
   // vectorの中身はBeginFrameで消えるがvector自体のアドレスは変わらない。
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
   frameCtx_.textRenderer     = textRenderer_.get();
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

   // 3D描画面とピクセル座標ベースのUIカメラを同じサイズへ更新し、
   // リサイズ後も合成位置とクリップ空間変換を一致させる。
   const uint32_t width = device_->GetBackBufferWidth();
   const uint32_t height = device_->GetBackBufferHeight();
   offscreenRenderTarget_->Resize(width, height);
   SyncUICameraToRenderTarget(width, height);
}

void Renderer::BeginFrame() {
   // コマンド生成時には最新のGPUハンドルを参照できるよう、ライトを先にGPUレイアウトへ詰める。
   if (lightManager_) {
	  lightManager_->UpdateStructureBuffer();
   }

   // 各パスのキューはフレーム単位。テキストだけは全コマンド確定後に一括アップロードするため、
   // CPU側の頂点・インデックス蓄積もここで開始する。
   opaqueCommands_.clear();
   transparentCommands_.clear();
   postProcessCommands_.clear();
   textRenderer_->BeginFrame();

   // 以降の登録処理と即時描画は、最終バックバッファではなくHDR中間面へ積む。
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

   auto* meshComponent = model->GetComponent<MeshComponent>();
   if (!meshComponent ||
      (meshComponent->GetSourceType() == MeshComponent::SourceType::ModelFile && !meshComponent->GetModelAsset()) ||
      (meshComponent->GetSourceType() == MeshComponent::SourceType::Primitive && !meshComponent->EnsureMesh())) {
      return;
   }

   std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> handles = { texture->GetTextureSrvHandleGPU() };

   // 行列を更新
   model->UpdateMatrix(activeCamera);

   // ブレンド設定はマテリアル固有値を最優先し、呼び出し引数、Renderer既定値の順に解決する。
   // 見た目をアセット側で固定しつつ、一時描画だけ引数で上書きできる規則にしている。
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

   auto* meshComponent = model->GetComponent<MeshComponent>();
   if (!meshComponent ||
      (meshComponent->GetSourceType() == MeshComponent::SourceType::ModelFile && !meshComponent->GetModelAsset()) ||
      (meshComponent->GetSourceType() == MeshComponent::SourceType::Primitive && !meshComponent->EnsureMesh())) {
      return;
   }

   // DrawCommandが実行時にTextureオブジェクトへ再アクセスしないよう、登録時点で
   // メッシュ順にGPU SRVハンドルへ変換して値として保持する。
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

void Renderer::DrawUIText(std::string_view text, const Vector2& position, const TextStyle& style) {
   FontManager* fontManager = assetManager_ ? assetManager_->GetFontManager() : nullptr;
   if (!fontManager || !textRenderer_ || text.empty()) {
      return;
   }

   // 文字列全体を先にレイアウトし、アトラスページごとに分かれた描画データを
   // 通常のコマンドキューへ合流させる。GPUバッファ転送はEndFrameで一括実行する。
   Transform transform{};
   transform.translation = { position.x, position.y, 0.0f };
   const TextLayoutResult layout = fontManager->LayoutText(text, style);
   const auto drawDataList = textRenderer_->QueueText(
      layout,
      style,
      transform,
      UITextComponent::kShowAllGlyphs,
      device_->GetBackBufferWidth(),
      device_->GetBackBufferHeight());
   for (const TextDrawData& textData : drawDataList) {
      SubmitDrawCommand(DrawCommand::CreateText(textData, RenderPass::PostProcess));
   }
}

Vector2 Renderer::MeasureText(std::string_view text, const TextStyle& style) {
   FontManager* fontManager = assetManager_ ? assetManager_->GetFontManager() : nullptr;
   return fontManager ? fontManager->LayoutText(text, style).size : Vector2{ 0.0f, 0.0f };
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

   // カメラ回転には追従するが位置移動では空が近づかないよう、ビュー行列から平行移動を除く。
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

   // JSONのルート定義から意味名でスロットを引き、定義順変更をC++へ波及させない。
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

   // Skyboxはキューを介さずPSOを直接変更するため、RendererのPSOキャッシュを無効化し、
   // 後続コマンドが誤って再設定を省略しないようにする。
   currentPipelineName_ = "";
   currentPipelineBlendMode_ = BlendMode::kBlendModeNone;
}

void Renderer::SubmitDrawCommand(const DrawCommand& command) {
   RouteDrawCommand(command);
}

void Renderer::RouteDrawCommand(const DrawCommand& command) {
   // 登録時にパス別の配列へ分離し、EndFrameでは依存順に連続実行できるようにする。
   // wrapperは各描画種別に共通のソート情報と実データの寿命を提供する。
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

   ModelAsset* modelAsset = model->GetComponent<MeshComponent>()->GetModelAsset();
   if (!modelAsset) {
	  return;
   }

   const Skeleton* bindSkeleton = modelAsset->GetBindSkeleton();
   if (!bindSkeleton || bindSkeleton->joints.empty()) {
	  return;
   }

   // デバッグ描画のためにバインド姿勢を複製し、モデル本体のスキンクラスターを変更せず
   // 現在時刻のアニメーション姿勢だけをローカルに評価する。
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

   // 全関節のモデル変換後位置を先に確定し、親子線の走査で親行列を再計算しない。
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

   // 自動登録オブジェクトをすべてキュー化してから、テキストとラインの共有バッファを確定する。
   // これ以降に追加すると当該フレームのアップロード範囲へ含まれない。
   DrawAutoRegisteredModels();
   DrawAutoRegisteredSprites();
   DrawAutoRegisteredParticles();
   DrawAutoRegisteredTexts();

   if (FontManager* fontManager = assetManager_ ? assetManager_->GetFontManager() : nullptr) {
   }
   textRenderer_->UploadBuffers();

   // ポストエフェクト対象のラインと対象外ラインを別キューへ送り、合成前後の位置を固定する。
   FlushLineRenderer(lineRenderer_.get(), RenderPass::Opaque);
   FlushLineRenderer(postProcessLineRenderer_.get(), RenderPass::PostProcess);

   // 次のフレームに備えてクリア
   lineRenderer_->Clear();
   postProcessLineRenderer_->Clear();

   // スカイボックスを不透明パス完了後に描画
   DrawAutoRegisteredSkyboxes();

   // 各パスは前段の出力状態を前提とするため、BuildDefaultPassesで登録した順を崩さない。
   for (const auto& pass : renderPasses_) {
	  if (pass) {
		 pass->Execute(frameCtx_);
	  }
   }

   // UIまで合成した画像へ適用し、実行画面とエディタのシーンビューを同じ暗転結果にする。
   ApplySceneTransitionOverlay();

   // 中間面の全合成が終わってからバックバッファをRTV化し、最終画像を一度だけ転送する。
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
	  if (postProcessManager_->ShowImGuiControls() && editorController_) {
		 editorController_->MarkActiveSceneDirty();
	  }
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

   // テクスチャ未設定でもマテリアル色を描けるよう白テクスチャを既定値にする。
   auto* fallbackTexture = textureManager->GetTexture("white1x1");

   for (auto* model : Model::GetRegisteredModels()) {
	  if (!model) {
		 continue;
	  }

    auto* renderComponent = model->GetComponent<RenderComponent>();
	  if (!renderComponent) {
		 continue;
	  }

	  ResolveParentRelationForRender(model);

	  if (!renderComponent->IsEnabled() || !renderComponent->autoRender || !renderComponent->visible) {
		 continue;
	  }

	  auto* materialComp = model->GetComponent<MaterialComponent>();

	  // Texture は MaterialComponent からのみ取得
	  Texture* texture = nullptr;
	  if (materialComp && !materialComp->GetTextureNames().empty() && !materialComp->GetTextureNames()[0].empty()) {
		 texture = textureManager->GetTexture(materialComp->GetTextureNames()[0]);
	  }
      // 2DサンプラーへCubemap SRVを渡すのはビュー次元不一致になるため、環境用は通常テクスチャから除外する。
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

   for (auto* sprite : Sprite::GetRegisteredSprites()) {
	  if (!sprite) {
		 continue;
	  }

   auto* renderComponent = sprite->GetComponent<RenderComponent>();
	  if (!renderComponent) {
		 continue;
	  }

	  ResolveParentRelationForRender(sprite);

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

void Renderer::DrawAutoRegisteredTexts() {
   FontManager* fontManager = assetManager_ ? assetManager_->GetFontManager() : nullptr;
   if (!fontManager || !textRenderer_) {
      return;
   }

   // 同じsortingOrder内では登録順を保ち、文字の重なりがフレームごとに揺れないようstable_sortを使う。
   std::vector<UIText*> texts = UIText::GetRegisteredTexts();
   std::stable_sort(texts.begin(), texts.end(), [](const UIText* lhs, const UIText* rhs) {
      const auto* leftText = lhs ? lhs->GetComponent<UITextComponent>() : nullptr;
      const auto* rightText = rhs ? rhs->GetComponent<UITextComponent>() : nullptr;
      const int32_t leftOrder = leftText ? leftText->GetStyle().sortingOrder : 0;
      const int32_t rightOrder = rightText ? rightText->GetStyle().sortingOrder : 0;
      return leftOrder < rightOrder;
   });

   for (UIText* text : texts) {
      if (!text) {
         continue;
      }
      const auto* renderComponent = text->GetComponent<RenderComponent>();
      auto* textComponent = text->GetComponent<UITextComponent>();
      const auto* transformComponent = text->GetComponent<TransformComponent>();
      if (!textComponent || !transformComponent ||
         (renderComponent && (!renderComponent->visible || !renderComponent->autoRender))) {
         continue;
      }

      const TextLayoutResult& layout = textComponent->GetLayout(*fontManager);
      const auto drawDataList = textRenderer_->QueueText(
         layout,
         textComponent->GetStyle(),
         transformComponent->transform,
         textComponent->GetVisibleGlyphCount(),
         device_->GetBackBufferWidth(),
         device_->GetBackBufferHeight());
      for (const TextDrawData& textData : drawDataList) {
         SubmitDrawCommand(DrawCommand::CreateText(textData, RenderPass::PostProcess));
      }
   }
}

void Renderer::ExecuteDrawCommands(const std::vector<std::unique_ptr<IDrawCommand>>& commands) {
   // パス側で並べ替えた共通コマンドを、型ごとの専用レンダラーへここで振り分ける。
   // PSO設定コールバックを共有し、連続する同一PSOの冗長なバインドを抑える。
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
		 case DrawCommandType::Text:
			textRenderer_->DrawUIText(cmd.textData,
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
   if (sceneTransitionHorizontalConstantBuffer_ && sceneTransitionHorizontalConstants_) {
      sceneTransitionHorizontalConstantBuffer_->Unmap(0, nullptr);
   }
   sceneTransitionHorizontalConstants_ = nullptr;
   sceneTransitionHorizontalConstantBuffer_.Reset();
   if (sceneTransitionConstantBuffer_ && sceneTransitionConstants_) {
      sceneTransitionConstantBuffer_->Unmap(0, nullptr);
   }
   sceneTransitionConstants_ = nullptr;
   sceneTransitionConstantBuffer_.Reset();
   if (textRenderer_) {
      textRenderer_->Finalize();
   }
#ifdef USE_IMGUI
   imGuiManager_->Finalize();
#endif
}

void Renderer::SetBlendMode(BlendMode blendMode) {
   currentBlendMode_ = blendMode;
}

void Renderer::SetSceneTransitionOpacity(float opacity) {
   sceneTransitionOpacity_ = std::clamp(opacity, 0.0f, 1.0f);
   if (sceneTransitionHorizontalConstants_) {
      sceneTransitionHorizontalConstants_->opacity = sceneTransitionOpacity_;
   }
   if (sceneTransitionConstants_) {
      sceneTransitionConstants_->opacity = sceneTransitionOpacity_;
   }
}

void Renderer::InitializeUICamera() {
   Transform uiCameraTransform = {};
   uiCameraTransform.scale = Vector3(1.0f, 1.0f, 1.0f);
   uiCameraTransform.rotation = Vector3(0.0f, 0.0f, 0.0f);
   uiCameraTransform.translation = Vector3(0.0f, 0.0f, 0.0f);

   uiCamera_->Initialize(uiCameraTransform, Camera::ProjectionType::Orthographic);
   uiCamera_->SetNearClip(0.0f);
   uiCamera_->SetFarClip(kUiCameraFarClip);
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
   // 頂点バッファを持たずVertexIDから画面全体を覆う三角形を生成するため、3頂点だけ発行する。
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);
}

void Renderer::ApplySceneTransitionOverlay() {
   if (sceneTransitionOpacity_ <= 0.0f ||
      !sceneTransitionHorizontalConstantBuffer_ ||
      !sceneTransitionConstantBuffer_ ||
      !offscreenRenderTarget_) {
      return;
   }

   auto* transitionPipeline = psoManager_->GetPipeline("SceneTransition");
   const auto constantBufferSlot =
      psoManager_->ResolvePipelineRootParameter("SceneTransition", "constantbuffer");
   const auto inputTextureSlot =
      psoManager_->ResolvePipelineRootParameter("SceneTransition", "inputtexture");
   if (!transitionPipeline || !constantBufferSlot.has_value() || !inputTextureSlot.has_value()) {
      Logger::Error("[Renderer] SceneTransition pipeline bindings could not be resolved.");
      return;
   }

   auto* commandList = device_->GetCommandList();
   const auto applyTransitionPass =
      [&](D3D12_GPU_DESCRIPTOR_HANDLE inputTexture, ID3D12Resource* constantBuffer) {
         // 読み込み中の面と書き込み面を分離するため、各方向の処理ごとにping-pongする。
         // PreDrawのクリア後に全画面三角形で入力を再構成し、PostDrawで次段のSRVへ戻す。
         offscreenRenderTarget_->SwapBuffers();
         offscreenRenderTarget_->PreDraw(false);

         commandList->SetGraphicsRootSignature(transitionPipeline->GetRootSignature());
         commandList->SetPipelineState(transitionPipeline->GetPipelineState());
         commandList->SetGraphicsRootConstantBufferView(
            constantBufferSlot.value(),
            constantBuffer->GetGPUVirtualAddress());
         commandList->SetGraphicsRootDescriptorTable(inputTextureSlot.value(), inputTexture);
         commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
         commandList->DrawInstanced(3, 1, 0, 0);

         offscreenRenderTarget_->PostDraw();
      };

   // 隣接画素を横・縦に分けて取得し、間隔を空けた格子状サンプリングを避ける。
   applyTransitionPass(
      offscreenRenderTarget_->GetSRVHandleGPU(),
      sceneTransitionHorizontalConstantBuffer_.Get());
   applyTransitionPass(
      offscreenRenderTarget_->GetSRVHandleGPU(),
      sceneTransitionConstantBuffer_.Get());

   InvalidatePipelineBinding();
}

void Renderer::SetPipeline(const std::string& pipelineName, BlendMode blendMode) {
   // 同一パイプラインなら DX12 コマンドリストへの再セットをスキップ
   if (currentPipelineName_ == pipelineName && currentPipelineBlendMode_ == blendMode) {
	  return;
   }

   // PSOManagerの名前検索を描画ごとに繰り返さないよう、名前とブレンドの組で解決結果を保持する。
   // 実コマンドリストが外部から変更された場合はInvalidatePipelineBindingで状態だけ無効化する。
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

      // キュー実行はこの関数の後なので、Begin/End用の一時配列を値キャプチャし寿命を切り離す。
	  std::vector<LineRenderer::LineInstance> capturedLines = lines;
	  const size_t lineCount = capturedLines.size();
	  auto* lineRendererPtr = renderer;
	  const auto lineTransformSlot = psoManager_->ResolvePipelineRootParameter("Line3D", "transform");
	  if (!lineTransformSlot.has_value()) {
		 Logger::Error("[Renderer] Failed to resolve Line3D transform root slot from PSO JSON.");
		 continue;
	  }
      // 透明キューに送られた場合の距離ソート代表点として、全線分の中点平均を使う。
	  Vector3 center = { 0.0f, 0.0f, 0.0f };
	  for (const auto& line : capturedLines) {
		 center += (line.start + line.end) * 0.5f;
	  }
	  center /= static_cast<float>(lineCount);

	  auto drawFunc = [lineRendererPtr, capturedLines, lineCount, lineTransformSlot = lineTransformSlot.value()](ID3D12GraphicsCommandList* cmdList, const Matrix4x4& viewProjMatrix) {
		 if (capturedLines.empty() || !lineRendererPtr) {
			return;
		 }

		 // 実行順が確定した時点で共有インスタンスバッファへ転送し、直後のDrawInstancedで消費する。
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
      // 「ポストプロセス対象外」はエフェクト適用後に描くキューを意味する。
	  return RenderPass::PostProcess;
   }

   // 深度を書き込む不透明物を先に確定し、ブレンド物だけを距離ソート可能な透明パスへ送る。
   return (blendMode == BlendMode::kBlendModeNone) ? RenderPass::Opaque : RenderPass::Transparent;
}

void Renderer::SortTransparentCommands() {
   if (transparentCommands_.size() < 2) {
	  return;
   }

   // 同距離の登録順を維持しつつ、位置を持つ要素は奥から手前へ描いてアルファ合成を成立させる。
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
