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

namespace {
Logger& log_ = Logger::GetInstance();
}

namespace GameEngine {
void Renderer::Initialize(GraphicsDevice* device, Window* window, CameraManager* cameraManager, LightManager* lightManager, AssetManager* assetManager) {
   device_ = device;
   cameraManager_ = cameraManager;
   lightManager_ = lightManager;
   assetManager_ = assetManager;

#ifdef USE_IMGUI
   imGuiManager_->Initialize(window->GetHwnd(), device_);
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

   // レンズフレアの初期化（UIカメラを使用）
   lensFlare_->Initialize(device_, Window::kWindowWidth, Window::kWindowHeight, uiCamera_.get());

   // テクスチャマネージャーを使ってレンズフレア用のテクスチャを読み込む
   if (assetManager_ && assetManager_->GetTextureManager()) {
	  auto* textureManager = assetManager_->GetTextureManager();

	  // レンズフレア用テクスチャを読み込み
	  const std::vector<std::string> lensFlareTextures = {
		 "resources/lensFlare/lensFlare1.png",
		 "resources/lensFlare/lensFlare2.png",
		 "resources/lensFlare/lensFlare3.png",
		 "resources/lensFlare/lensFlare4.png",
		 "resources/lensFlare/lensFlare5.png",
		 "resources/lensFlare/lensFlare6.png",
		 "resources/lensFlare/lensFlare7.png",
		 "resources/lensFlare/lensFlare8.png",
		 "resources/lensFlare/lensFlare9.png"
	  };

	  for (const auto& texturePath : lensFlareTextures) {
		 // ファイル名のみを抽出（パスの最後の部分）
		 std::string textureName = texturePath.substr(texturePath.find_last_of("/\\") + 1);
		 textureManager->LoadTexture(texturePath, textureName);

		 // テクスチャをレンズフレアに追加
		 Texture* texture = textureManager->GetTexture(textureName);
		 if (texture) {
			lensFlare_->AddTexture(textureName, texture);
			log_.Log("Loaded and added lens flare texture: " + textureName);
		 }
	  }

	  log_.Log("Lens flare initialization completed");
   } else {
	  log_.Log("AssetManager not available, lens flare textures not loaded", Logger::LogLevel::Warning);
   }

   lensFlare_->LoadFromJson("resources/lensFlare/lensflare_config.json");
}

void Renderer::BeginFrame() {
   // ライトの構造化バッファを更新
   if (lightManager_) {
      lightManager_->UpdateStructureBuffer();
   }

   // 描画コマンドリストをクリア（不透明オブジェクトは即時描画なのでクリア不要）
   transparentCommands_.clear();
   postProcessCommands_.clear();

   // レンズフレアのクエリ状態とトラッキングをリセット
   lensFlareQueryActive_ = false;
   lensFlareTrackingEnabled_ = false;
   lensFlareTrackedPositions_.clear();

   offscreenRenderTarget_->PreDraw(true);

#ifdef USE_IMGUI
   imGuiManager_->BeginFrame();
#endif

   // 初期パイプライン設定（パイプライン名とブレンドモードをリセット）
   currentPipelineName_ = "";
   currentPipelineBlendMode_ = BlendMode::kBlendModeNone;
   
   lineRenderer_->Begin();
}

void Renderer::Draw(Model* model, Texture* texture, std::optional<BlendMode> blendMode, bool applyPostProcess) {
   assert(model != nullptr);
   assert(texture != nullptr);

   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   if (!model->GetModelAsset()) return;

   // レンズフレアトラッキングが有効な場合、モデルの位置を記録
   bool isFirstTrackedModel = false;
   if (lensFlareTrackingEnabled_) {
	  isFirstTrackedModel = lensFlareTrackedPositions_.empty();
	  lensFlareTrackedPositions_.push_back(model->GetTransform().translation);

	  // 最初のトラッキング対象モデルの場合、クエリを開始
	  if (isFirstTrackedModel && lensFlare_) {
		 // 重心計算（現時点では1つだけ）
		 lensFlareSourcePos_ = model->GetTransform().translation;

		 auto* cmdList = device_->GetCommandList();
		 cmdList->BeginQuery(lensFlare_->GetQueryHeap(), D3D12_QUERY_TYPE_OCCLUSION, 0);
		 lensFlareQueryActive_ = true;
	  }
   }

   std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> handles = { texture->GetTextureSrvHandleGPU() };

   // 行列を更新
   model->UpdateMatrix(activeCamera);

   // ブレンドモードの決定（引数で指定されていない場合は現在のモードを使用）
   BlendMode effectiveBlendMode = blendMode.value_or(currentBlendMode_);

   // 描画パスの決定
   RenderPass renderPass = applyPostProcess ?
	  (effectiveBlendMode == BlendMode::kBlendModeNone ? RenderPass::Opaque : RenderPass::Transparent) :
	  RenderPass::PostProcess;

   if (renderPass == RenderPass::Opaque) {
	  // 不透明オブジェクトは即時描画
	  ModelDrawData drawData;
	  drawData.model = model;
	  drawData.textures = handles;
	  drawData.camera = activeCamera;
	  drawData.blendMode = effectiveBlendMode;

	  modelRenderer_->DrawModel(drawData, defaultMaterial_.get(), lightManager_,
		  [this](const std::string& name, BlendMode mode) { SetPipeline(name, mode); });
   } else {
	  // 半透明/ポストプロセスは遅延描画
	  DrawCommand cmd = DrawCommand::CreateModel(model, handles, activeCamera, effectiveBlendMode, renderPass);

	  if (renderPass == RenderPass::Transparent) {
		 transparentCommands_.push_back(cmd);
	  } else {
		 postProcessCommands_.push_back(cmd);
	  }
   }
}

void Renderer::Draw(Model* model, const std::vector<Texture*>& textures, std::optional<BlendMode> blendMode, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);
   assert(model != nullptr);
   assert(!textures.empty());

   if (!model->GetModelAsset()) return;

   // レンズフレアトラッキングが有効な場合、モデルの位置を記録
   bool isFirstTrackedModel = false;
   if (lensFlareTrackingEnabled_) {
	  isFirstTrackedModel = lensFlareTrackedPositions_.empty();
	  lensFlareTrackedPositions_.push_back(model->GetTransform().translation);

	  // 最初のトラッキング対象モデルの場合、クエリを開始
	  if (isFirstTrackedModel && lensFlare_) {
		 // 重心計算（現時点では1つだけ）
		 lensFlareSourcePos_ = model->GetTransform().translation;

		 auto* cmdList = device_->GetCommandList();
		 cmdList->BeginQuery(lensFlare_->GetQueryHeap(), D3D12_QUERY_TYPE_OCCLUSION, 0);
		 lensFlareQueryActive_ = true;
	  }
   }

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
   RenderPass renderPass = applyPostProcess ?
	  (effectiveBlendMode == BlendMode::kBlendModeNone ? RenderPass::Opaque : RenderPass::Transparent) :
	  RenderPass::PostProcess;

   if (renderPass == RenderPass::Opaque) {
	  // 不透明オブジェクトは即時描画
	  ModelDrawData drawData;
	  drawData.model = model;
	  drawData.textures = textureSrvHandles;
	  drawData.camera = activeCamera;
	  drawData.blendMode = effectiveBlendMode;

	  modelRenderer_->DrawModel(drawData, defaultMaterial_.get(), lightManager_,
		  [this](const std::string& name, BlendMode mode) { SetPipeline(name, mode); });
   } else {
	  // 半透明/ポストプロセスは遅延描画
	  DrawCommand cmd = DrawCommand::CreateModel(model, textureSrvHandles, activeCamera, effectiveBlendMode, renderPass);

	  if (renderPass == RenderPass::Transparent) {
		 transparentCommands_.push_back(cmd);
	  } else {
		 postProcessCommands_.push_back(cmd);
	  }
   }
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
   RenderPass renderPass = applyPostProcess ?
	  (effectiveBlendMode == BlendMode::kBlendModeNone ? RenderPass::Opaque : RenderPass::Transparent) :
	  RenderPass::PostProcess;

   if (renderPass == RenderPass::Opaque) {
	  // 不透明スプライトは即時描画
	  SpriteDrawData drawData;
	  drawData.sprite = sprite;
	  drawData.texture = texture;
	  drawData.textureSrvHandle = texture->GetTextureSrvHandleGPU();
	  drawData.camera = activeCamera;
	  drawData.blendMode = effectiveBlendMode;

	  spriteRenderer_->DrawSprite(drawData, defaultMaterial_.get(), lightManager_,
		  [this](const std::string& name, BlendMode mode) { SetPipeline(name, mode); });
   } else {
	  // 半透明/ポストプロセスは遅延描画
	  DrawCommand cmd = DrawCommand::CreateSprite(sprite, texture, texture->GetTextureSrvHandleGPU(),
		 activeCamera, effectiveBlendMode, renderPass);

	  if (renderPass == RenderPass::Transparent) {
		 transparentCommands_.push_back(cmd);
	  } else {
		 postProcessCommands_.push_back(cmd);
	  }
   }
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

   if (renderPass == RenderPass::Transparent) {
	  transparentCommands_.push_back(cmd);
   } else {
	  postProcessCommands_.push_back(cmd);
   }
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
   RenderPass renderPass = applyPostProcess ?
	  (effectiveBlendMode == BlendMode::kBlendModeNone ? RenderPass::Opaque : RenderPass::Transparent) :
	  RenderPass::PostProcess;

   if (renderPass == RenderPass::Opaque) {
	  // 不透明UIは即時描画
	  UISpriteDrawData drawData;
	  drawData.sprite = sprite;
	  drawData.texture = texture;
	  drawData.textureSrvHandle = textureSrvHandle;
	  drawData.anchorPoint = anchorPoint;
	  drawData.screenWidth = screenWidth;
	  drawData.screenHeight = screenHeight;
	  drawData.blendMode = effectiveBlendMode;

	  uiRenderer_->DrawUISprite(drawData, defaultMaterial_.get(),
		  [this](const std::string& name, BlendMode mode) { SetPipeline(name, mode); });
   } else {
	  // 半透明/ポストプロセスUIは遅延描画
	  DrawCommand cmd = DrawCommand::CreateUISprite(sprite, texture, textureSrvHandle,
		 anchorPoint, screenWidth, screenHeight,
		 effectiveBlendMode, renderPass);

	  if (renderPass == RenderPass::Transparent) {
		 transparentCommands_.push_back(cmd);
	  } else {
		 postProcessCommands_.push_back(cmd);
	  }
   }
}

void Renderer::DrawLine(const Vector3& start, const Vector3& end, const Vector4& color, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   lineRenderer_->DrawLine(start, end, color, activeCamera);

   // ラインはEndでバッチ化されるため、ここでは追加のみ
   (void)applyPostProcess;  // 将来的にポストプロセス制御を実装する場合の予約パラメータ
}

void Renderer::DrawSpline(const std::vector<Vector3>& controlPoints, const Vector4& color, size_t segmentCount, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   lineRenderer_->DrawSpline(controlPoints, color, segmentCount, activeCamera);
   (void)applyPostProcess;  // 将来的にポストプロセス制御を実装する場合のための予約パラメータ
}

void Renderer::DrawGrid(GridPlane plane, float gridSize, int thickLineInterval, int range, bool enableFade, float fadeDistance, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   lineRenderer_->DrawGrid(activeCamera, plane, gridSize, thickLineInterval, range, enableFade, fadeDistance);
   (void)applyPostProcess;  // 将来的にポストプロセス制御を実装する場合のための予約パラメータ
}

void Renderer::DrawSphere(const Vector3& center, float radius, const Vector4& color, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   lineRenderer_->DrawSphere(center, radius, color, activeCamera);
   (void)applyPostProcess;  // 将来的にポストプロセス制御を実装する場合のための予約パラメータ
}

void Renderer::DrawHemisphere(const Vector3& center, float radius, const Vector3& up, const Vector4& color, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   lineRenderer_->DrawHemisphere(center, radius, up, color, activeCamera);
   (void)applyPostProcess;  // 将来的にポストプロセス制御を実装する場合のための予約パラメータ
}

void Renderer::DrawCone(const Vector3& apex, float radius, float height, const Vector3& direction, const Vector4& color, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   lineRenderer_->DrawCone(apex, radius, height, direction, color, activeCamera);
   (void)applyPostProcess;  // 将来的にポストプロセス制御を実装する場合のための予約パラメータ
}

void Renderer::DrawBox(const Vector3& center, const Vector3& size, const Vector4& color, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   lineRenderer_->DrawBox(center, size, color, activeCamera);
   (void)applyPostProcess;  // 将来的にポストプロセス制御を実装する場合のための予約パラメータ
}

void Renderer::DrawCircle(const Vector3& center, float radius, const Vector3& normal, const Vector4& color, bool applyPostProcess) {
   Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
   assert(activeCamera != nullptr);

   lineRenderer_->DrawCircle(center, radius, normal, color, activeCamera);
   (void)applyPostProcess;  // 将来的にポストプロセス制御を実装する場合のための予約パラメータ
}

void Renderer::EndFrame() {
   // ラインレンダラーを終了
   lineRenderer_->End();

   // カメラごとにライン描画コマンドを作成
   const auto& cameraLineGroups = lineRenderer_->GetCameraLineGroups();
   for (const auto& [camera, lines] : cameraLineGroups) {
	  if (lines.empty() || !camera) continue;

	  // このカメラのラインデータをキャプチャ
	  std::vector<LineRenderer::LineInstance> capturedLines = lines;
	  size_t lineCount = capturedLines.size();
	  Matrix4x4 viewProj = camera->GetViewProjectionMatrix();

	  // LineRendererへの参照をキャプチャ
	  auto* lineRendererPtr = lineRenderer_.get();

	  // 描画関数を作成
	  auto drawFunc = [lineRendererPtr, capturedLines, lineCount](ID3D12GraphicsCommandList* cmdList, const Matrix4x4& viewProjMatrix) {
		 if (capturedLines.empty() || !lineRendererPtr) return;

		 // インスタンスバッファに書き込み
		 auto* mappedBuffer = lineRendererPtr->GetMappedInstanceBuffer();
		 if (mappedBuffer) {
			memcpy(mappedBuffer, capturedLines.data(), sizeof(LineRenderer::LineInstance) * lineCount);
		 }

		 // 行列の更新
		 Matrix4x4 world = MakeIdentity4x4();
		 lineRendererPtr->UpdateMatrix(world, viewProjMatrix);

		 // 描画セットアップ
		 cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

		 // LineRendererのバッファビューを直接使用できないため、
		 // ここで取得する必要がある
		 // 代わりに、LineRendererに描画を委譲
		 lineRendererPtr->Draw(cmdList);

		 // インスタンシング描画
		 cmdList->DrawInstanced(2, static_cast<UINT>(lineCount), 0, 0);
		 };

	  DrawCommand cmd = DrawCommand::CreateLine(drawFunc, camera, RenderPass::Opaque);

	  // ラインは不透明として即時描画
	  DrawLineInternal(cmd.lineData);
   }

   // 次のフレームに備えてクリア
   lineRenderer_->Clear();

#ifdef USE_IMGUI
   /*if (lensFlare_) {
	  lensFlare_->ImGuiEdit();
   }*/
#endif 

   // レンズフレアをオフスクリーンに描画（ポストプロセス前）
   if (lensFlare_) {
	  Camera* activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
	  if (activeCamera) {
		 // オクルージョンクエリ結果を取得
		 lensFlare_->ResolveOcclusionQuery();

		 // 可視性チェック
		 float visibility = lensFlare_->GetVisibilityFactor();
		 if (visibility > 0.0f) {
			// レンズフレアを更新（スプライトの位置とスケールを計算）
			lensFlare_->Update(lensFlareSourcePos_, activeCamera);

			// 各フレア要素をRendererのDrawで描画
			const auto& flareElements = lensFlare_->GetFlareElements();
			for (const auto& element : flareElements) {
			   if (!element.visible) continue;

			   // テクスチャを取得
			   Texture* texture = lensFlare_->GetElementTexture(element);
			   if (!texture) continue;

			   // 加算ブレンドでスプライトを描画
			   DrawUI(element.sprite.get(), texture, Sprite::AnchorPoint::MiddleCenter, BlendMode::kBlendModeAdd, true);
			}
		 }
	  }
   }

   // 半透明オブジェクトを描画（ポストプロセス前）
//    ExecuteDrawCommands(transparentCommands_);
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
   cmdList->SetGraphicsRootSignature(fullscreenPipeline->GetRootSignature());
   cmdList->SetPipelineState(fullscreenPipeline->GetPipelineState());
   cmdList->SetGraphicsRootDescriptorTable(0, textureSrvHandle);
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

void Renderer::BeginLensFlareOcclusionQuery() {
   if (!lensFlare_) return;

   // 前回のクエリがまだアクティブな場合は警告
   if (lensFlareQueryActive_) {
	  log_.Log("Warning: Previous lens flare query was not ended properly", Logger::LogLevel::Warning);
	  return;
   }

   // トラッキングを開始（位置の収集を開始）
   lensFlareTrackingEnabled_ = true;
   lensFlareTrackedPositions_.clear();
}

void Renderer::EndLensFlareOcclusionQuery() {
   if (!lensFlare_ || !lensFlareTrackingEnabled_) return;

   // トラッキングを終了
   lensFlareTrackingEnabled_ = false;

   if (lensFlareTrackedPositions_.empty()) {
	  return;
   }

   // 全ての位置の平均を計算（重心）
   Vector3 centerPosition = Vector3(0.0f, 0.0f, 0.0f);
   for (const auto& pos : lensFlareTrackedPositions_) {
	  centerPosition = centerPosition + pos;
   }
   centerPosition = centerPosition / static_cast<float>(lensFlareTrackedPositions_.size());

   // 光源位置を最終的な重心に更新
   lensFlareSourcePos_ = centerPosition;

   // クエリがアクティブな場合、終了して結果を解決
   if (lensFlareQueryActive_) {
	  auto* cmdList = device_->GetCommandList();

	  // クエリ終了
	  cmdList->EndQuery(lensFlare_->GetQueryHeap(), D3D12_QUERY_TYPE_OCCLUSION, 0);

	  // クエリ結果をバッファに解決
	  cmdList->ResolveQueryData(
		 lensFlare_->GetQueryHeap(),
		 D3D12_QUERY_TYPE_OCCLUSION,
		 0,
		 1,
		 lensFlare_->GetQueryResultBuffer(),
		 0
	  );

	  lensFlareQueryActive_ = false;
   }
}

} // namespace GameEngine