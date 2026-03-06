#include "pch.h"
#include "EngineContext.h"
#include "ModelAsset.h"
#include "BaseScene.h"
#include <numbers>

namespace {
GameEngine::GraphicsDevice* sGraphicsDevice_ = nullptr;
GameEngine::Input* sInput_ = nullptr;
GameEngine::Audio* sAudio_ = nullptr;
GameEngine::Renderer* sRenderer_ = nullptr;
GameEngine::AssetManager* sAssetManager_ = nullptr;
GameEngine::TimeProfiler* sTimeProfiler_ = nullptr;
GameEngine::CameraManager* sCameraManager_ = nullptr;
GameEngine::LightManager* sLightManager_ = nullptr;
GameEngine::JsonDataManager* sJsonDataManager_ = nullptr;
}

namespace GameEngine {

void EngineContextInitializer::Initialize(GraphicsDevice* graphicsDevice, Input* input, Audio* audio, Renderer* renderer, AssetManager* assetManager, TimeProfiler* timeProfiler, CameraManager* cameraManager,
   LightManager* lightManager, JsonDataManager* jsonDataManager) {
   sGraphicsDevice_ = graphicsDevice;
   sInput_ = input;
   sAudio_ = audio;
   sRenderer_ = renderer;
   sAssetManager_ = assetManager;
   sTimeProfiler_ = timeProfiler;
   sCameraManager_ = cameraManager;
   sLightManager_ = lightManager;
   sJsonDataManager_ = jsonDataManager;
}

void EngineContextInitializer::SetGraphicsDevice(GraphicsDevice* graphicsDevice) {
   sGraphicsDevice_ = graphicsDevice;
}

void EngineContextInitializer::SetInput(Input* input) {
   sInput_ = input;
}

void EngineContextInitializer::SetAudio(Audio* audio) {
   sAudio_ = audio;
}

void EngineContextInitializer::SetRenderer(Renderer* renderer) {
   sRenderer_ = renderer;
}

void EngineContextInitializer::SetAssetManager(AssetManager* assetManager) {
   sAssetManager_ = assetManager;
}

void EngineContextInitializer::SetTimeProfiler(TimeProfiler* timeProfiler) {
   sTimeProfiler_ = timeProfiler;
}

void EngineContextInitializer::SetCameraManager(CameraManager* cameraManager) {
   sCameraManager_ = cameraManager;
}

void EngineContextInitializer::SetLightManager(LightManager* lightManager) {
   sLightManager_ = lightManager;
}

void EngineContextInitializer::SetJsonDataManager(JsonDataManager* jsonDataManager) {
   sJsonDataManager_ = jsonDataManager;
}

//================================================================
// Internal helper
//================================================================

JsonDataManager* EngineContext::GetJsonDataManagerInternal_() {
   return sJsonDataManager_;
}

//================================================================
// 入力
//================================================================

bool EngineContext::IsKeyPressed(uint8_t key) {
   if (!sInput_) return false;
   return sInput_->IsKeyPressed(key);
}

bool EngineContext::IsKeyPressed(KeyCode key) {
   if (!sInput_) return false;
   return sInput_->IsKeyPressed(key);
}

bool EngineContext::IsKeyNotPressed(uint8_t key) {
   if (!sInput_) return false;
   return sInput_->IsKeyNotPressed(key);
}

bool EngineContext::IsKeyNotPressed(KeyCode key) {
   if (!sInput_) return false;
   return sInput_->IsKeyNotPressed(key);
}

bool EngineContext::IsKeyTriggered(uint8_t key) {
   if (!sInput_) return false;
   return sInput_->IsKeyTriggered(key);
}

bool EngineContext::IsKeyTriggered(KeyCode key) {
   if (!sInput_) return false;
   return sInput_->IsKeyTriggered(key);
}

bool EngineContext::IsKeyReleased(uint8_t key) {
   if (!sInput_) return false;
   return sInput_->IsKeyReleased(key);
}

bool EngineContext::IsKeyReleased(KeyCode key) {
   if (!sInput_) return false;
   return sInput_->IsKeyReleased(key);
}

bool EngineContext::IsMousePressed(uint8_t button) {
   if (!sInput_) return false;
   return sInput_->IsMousePressed(button);
}

bool EngineContext::IsMousePressed(MouseButton button) {
   if (!sInput_) return false;
   return sInput_->IsMousePressed(button);
}

bool EngineContext::IsMouseNotPressed(uint8_t button) {
   if (!sInput_) return false;
   return sInput_->IsMouseNotPressed(button);
}

bool EngineContext::IsMouseNotPressed(MouseButton button) {
   if (!sInput_) return false;
   return sInput_->IsMouseNotPressed(button);
}

bool EngineContext::IsMouseReleased(uint8_t button) {
   if (!sInput_) return false;
   return sInput_->IsMouseReleased(button);
}

bool EngineContext::IsMouseReleased(MouseButton button) {
   if (!sInput_) return false;
   return sInput_->IsMouseReleased(button);
}

bool EngineContext::IsMouseTriggered(uint8_t button) {
   if (!sInput_) return false;
   return sInput_->IsMouseTriggered(button);
}

bool EngineContext::IsMouseTriggered(MouseButton button) {
   if (!sInput_) return false;
   return sInput_->IsMouseTriggered(button);
}

Vector2 EngineContext::GetMouseScreenPosition() {
   if (!sInput_) return Vector2(0.0f, 0.0f);
   return sInput_->GetMouseScreenPosition();
}

Vector2 EngineContext::GetMouseDelta() {
   if (!sInput_) return Vector2(0.0f, 0.0f);
   return sInput_->GetMouseDelta();
}

int32_t EngineContext::GetMouseWheelDelta() {
   if (!sInput_) return 0;
   return sInput_->GetMouseWheelDelta();
}

bool EngineContext::IsGamePadConnected(uint32_t index) {
   if (!sInput_) return false;
   return sInput_->IsGamePadConnected(index);
}

uint32_t EngineContext::GetConnectedGamePadCount() {
   if (!sInput_) return 0;
   return sInput_->GetConnectedGamePadCount();
}

bool EngineContext::IsGamePadButtonNotPressed(WORD button, uint32_t index) {
   if (!sInput_) return false;
   return sInput_->IsGamePadButtonNotPressed(button, index);
}

bool EngineContext::IsGamePadButtonNotPressed(GamePadButton button, uint32_t index) {
   if (!sInput_) return false;
   return sInput_->IsGamePadButtonNotPressed(static_cast<WORD>(button), index);
}

bool EngineContext::IsGamePadButtonPressed(WORD button, uint32_t index) {
   if (!sInput_) return false;
   return sInput_->IsGamePadButtonPressed(button, index);
}

bool EngineContext::IsGamePadButtonPressed(GamePadButton button, uint32_t index) {
   if (!sInput_) return false;
   return sInput_->IsGamePadButtonPressed(static_cast<WORD>(button), index);
}

bool EngineContext::IsGamePadButtonTriggered(WORD button, uint32_t index) {
   if (!sInput_) return false;
   return sInput_->IsGamePadButtonTriggered(button, index);
}

bool EngineContext::IsGamePadButtonTriggered(GamePadButton button, uint32_t index) {
   if (!sInput_) return false;
   return sInput_->IsGamePadButtonTriggered(static_cast<WORD>(button), index);
}

bool EngineContext::IsGamePadButtonReleased(WORD button, uint32_t index) {
   if (!sInput_) return false;
   return sInput_->IsGamePadButtonReleased(button, index);
}

bool EngineContext::IsGamePadButtonReleased(GamePadButton button, uint32_t index) {
   if (!sInput_) return false;
   return sInput_->IsGamePadButtonReleased(static_cast<WORD>(button), index);
}

Vector2 EngineContext::GetLeftStick(uint32_t index, float deadZone) {
   if (!sInput_) return Vector2(0.0f, 0.0f);
   return sInput_->GetLeftStick(index, deadZone);
}

Vector2 EngineContext::GetRightStick(uint32_t index, float deadZone) {
   if (!sInput_) return Vector2(0.0f, 0.0f);
   return sInput_->GetRightStick(index, deadZone);
}

float EngineContext::GetLeftTrigger(uint32_t index, float deadZone) {
   if (!sInput_) return 0.0f;
   return sInput_->GetLeftTrigger(index, deadZone);
}

float EngineContext::GetRightTrigger(uint32_t index, float deadZone) {
   if (!sInput_) return 0.0f;
   return sInput_->GetRightTrigger(index, deadZone);
}

void EngineContext::SetVibration(uint32_t index, float leftMotor, float rightMotor) {
   if (!sInput_) return;
   return sInput_->SetVibration(index, leftMotor, rightMotor);
}

void EngineContext::ChangeScene(const std::string& name) {
   BaseScene::SetNextSceneName(name);
}

float EngineContext::GetDeltaTime() {
   if (!sTimeProfiler_) return 0.0f;
   return sTimeProfiler_->GetDeltaTime();
}

float EngineContext::GetFPS() {
   if (!sTimeProfiler_) return 0.0f;
   return sTimeProfiler_->GetFPS();
}

float EngineContext::GetFrameTimeMs() {
   if (!sTimeProfiler_) return 0.0f;
   return sTimeProfiler_->GetFrameTimeMs();
}

void EngineContext::LoadModel(const std::string& modelPath, const std::string& modelName) {
   if (!sAssetManager_) return;
   if (!sAssetManager_->GetModelAssetManager()) return;
   sAssetManager_->GetModelAssetManager()->LoadModel(modelPath, modelName);
}

ModelAsset* EngineContext::GetModel(const std::string& modelName) {
   if (!sAssetManager_) return nullptr;
   if (!sAssetManager_->GetModelAssetManager()) return nullptr;
   return sAssetManager_->GetModelAssetManager()->GetModel(modelName);
}

void EngineContext::ClearModelAssets() {
   if (!sAssetManager_) return;
   if (!sAssetManager_->GetModelAssetManager()) return;
   sAssetManager_->GetModelAssetManager()->Clear();
}

void EngineContext::LoadTexture(const std::string& texturePath, const std::string& name) {
   if (!sAssetManager_) return;
   if (!sAssetManager_->GetTextureManager()) return;
   sAssetManager_->GetTextureManager()->LoadTexture(texturePath, name);
}

Texture* EngineContext::GetTexture(const std::string& name) {
   if (!sAssetManager_) return nullptr;
   if (!sAssetManager_->GetTextureManager()) return nullptr;
   return sAssetManager_->GetTextureManager()->GetTexture(name);
}

void EngineContext::ClearTextures() {
   if (!sAssetManager_) return;
   if (!sAssetManager_->GetTextureManager()) return;
   sAssetManager_->GetTextureManager()->Clear();
}

void EngineContext::CreateMaterial(const std::string& name, uint32_t color, int32_t lightingMode, const Matrix4x4& uvTransform) {
   if (!sAssetManager_) return;
   if (!sAssetManager_->GetMaterialManager()) return;
   sAssetManager_->GetMaterialManager()->CreateMaterial(name, color, lightingMode, uvTransform);
}

Material* EngineContext::GetMaterial(const std::string& name) {
   if (!sAssetManager_) return nullptr;
   if (!sAssetManager_->GetMaterialManager()) return nullptr;
   return sAssetManager_->GetMaterialManager()->GetMaterial(name);
}

void EngineContext::ClearMaterials() {
   if (!sAssetManager_) return;
   if (!sAssetManager_->GetMaterialManager()) return;
   sAssetManager_->GetMaterialManager()->Clear();
}

void EngineContext::LoadSound(const std::string& soundPath, const std::string& name) {
   if (!sAssetManager_) return;
   if (!sAssetManager_->GetSoundManager()) return;
   sAssetManager_->GetSoundManager()->LoadSound(soundPath, name);
}

Sound* EngineContext::GetSound(const std::string& name) {
   if (!sAssetManager_) return nullptr;
   if (!sAssetManager_->GetSoundManager()) return nullptr;
   return sAssetManager_->GetSoundManager()->GetSound(name);
}

void EngineContext::ClearSounds() {
   if (!sAssetManager_) return;
   if (!sAssetManager_->GetSoundManager()) return;
   sAssetManager_->GetSoundManager()->Clear();
}

Camera* EngineContext::GetActiveCamera() {
   if (!sCameraManager_) return nullptr;
   return sCameraManager_->GetActiveCamera();
}

void EngineContext::AddCamera(Camera* camera) {
   if (!sCameraManager_) return;
   sCameraManager_->AddCamera(camera);
}

void EngineContext::SetActiveCamera(size_t index) {
   if (!sCameraManager_) return;
   sCameraManager_->SetActiveCamera(index);
}

size_t EngineContext::GetCameraCount() {
   if (!sCameraManager_) return 0;
   return sCameraManager_->GetCameraCount();
}

const std::vector<Camera*>& EngineContext::GetCameras() {
   static std::vector<Camera*> emptyCameras;
   if (!sCameraManager_) return emptyCameras;
   return sCameraManager_->GetCameras();
}

void EngineContext::ClearCameras() {
   if (!sCameraManager_) return;
   sCameraManager_->ClearCameras();
}

//================================================================
// Light Manager
//================================================================

// DirectionalLight
DirectionalLight* EngineContext::CreateDirectionalLight(const std::string& name, unsigned int color, const Vector3& direction, float intensity) {
   if (!sLightManager_) return nullptr;
   return sLightManager_->CreateDirectionalLight(name, color, direction, intensity);
}

DirectionalLight* EngineContext::GetDirectionalLight(const std::string& name) {
   if (!sLightManager_) return nullptr;
   return sLightManager_->GetDirectionalLight(name);
}

bool EngineContext::RemoveDirectionalLight(const std::string& name) {
   if (!sLightManager_) return false;
   return sLightManager_->RemoveDirectionalLight(name);
}

void EngineContext::ClearDirectionalLights() {
   if (!sLightManager_) return;
   sLightManager_->ClearDirectionalLights();
}

std::vector<std::string> EngineContext::GetDirectionalLightNames() {
   if (!sLightManager_) return std::vector<std::string>();
   return sLightManager_->GetDirectionalLightNames();
}

// PointLight
PointLight* EngineContext::CreatePointLight(const std::string& name, unsigned int color, const Vector3& position, float intensity, float radius, float decay) {
   if (!sLightManager_) return nullptr;
   return sLightManager_->CreatePointLight(name, color, position, intensity, radius, decay);
}

PointLight* EngineContext::GetPointLight(const std::string& name) {
   if (!sLightManager_) return nullptr;
   return sLightManager_->GetPointLight(name);
}

bool EngineContext::RemovePointLight(const std::string& name) {
   if (!sLightManager_) return false;
   return sLightManager_->RemovePointLight(name);
}

const std::vector<PointLight*>& EngineContext::GetPointLights() {
   static std::vector<PointLight*> emptyPointLights;
   if (!sLightManager_) return emptyPointLights;
   
   // NOTE: This method is deprecated and only kept for compatibility
   // It returns an empty vector since lights are now stored in a map
   emptyPointLights.clear();
   auto names = sLightManager_->GetPointLightNames();
   for (const auto& name : names) {
      emptyPointLights.push_back(sLightManager_->GetPointLight(name));
   }
   return emptyPointLights;
}

void EngineContext::ClearPointLights() {
   if (!sLightManager_) return;
   sLightManager_->ClearPointLights();
}

std::vector<std::string> EngineContext::GetPointLightNames() {
   if (!sLightManager_) return std::vector<std::string>();
   return sLightManager_->GetPointLightNames();
}

// SpotLight
SpotLight* EngineContext::CreateSpotLight(const std::string& name, unsigned int color, const Vector3& position, float intensity, const Vector3& direction, float distance, float decay, float cosAngle, float cosFalloffStart) {
   if (!sLightManager_) return nullptr;
   return sLightManager_->CreateSpotLight(name, color, position, intensity, direction, distance, decay, cosAngle, cosFalloffStart);
}

SpotLight* EngineContext::GetSpotLight(const std::string& name) {
   if (!sLightManager_) return nullptr;
   return sLightManager_->GetSpotLight(name);
}

bool EngineContext::RemoveSpotLight(const std::string& name) {
   if (!sLightManager_) return false;
   return sLightManager_->RemoveSpotLight(name);
}

void EngineContext::ClearSpotLights() {
   if (!sLightManager_) return;
   sLightManager_->ClearSpotLights();
}

std::vector<std::string> EngineContext::GetSpotLightNames() {
   if (!sLightManager_) return std::vector<std::string>();
   return sLightManager_->GetSpotLightNames();
}

// AreaLight
AreaLight* EngineContext::CreateAreaLight(const std::string& name, const Vector3& position, const Vector3& normal, const Vector3& tangent, const Vector2& size, const Vector3& color, float intensity) {
   if (!sLightManager_) return nullptr;
   return sLightManager_->CreateAreaLight(name, position, normal, tangent, size, color, intensity);
}

AreaLight* EngineContext::GetAreaLight(const std::string& name) {
   if (!sLightManager_) return nullptr;
   return sLightManager_->GetAreaLight(name);
}

bool EngineContext::RemoveAreaLight(const std::string& name) {
   if (!sLightManager_) return false;
   return sLightManager_->RemoveAreaLight(name);
}

void EngineContext::ClearAreaLights() {
   if (!sLightManager_) return;
   sLightManager_->ClearAreaLights();
}

std::vector<std::string> EngineContext::GetAreaLightNames() {
   if (!sLightManager_) return std::vector<std::string>();
   return sLightManager_->GetAreaLightNames();
}

void EngineContext::DebugDrawLights() {
#ifdef USE_IMGUI
   if (sLightManager_) {
      sLightManager_->DebugDraw();
   }
#endif
}

void EngineContext::Draw(Model* model, Texture* texture, std::optional<BlendMode> blendMode, bool applyPostProcess) {
   if (!sRenderer_) return;
   sRenderer_->Draw(model, texture, blendMode, applyPostProcess);
}

void EngineContext::Draw(Model* model, const std::vector<Texture*>& textures, std::optional<BlendMode> blendMode, bool applyPostProcess) {
   if (!sRenderer_) return;
   sRenderer_->Draw(model, textures, blendMode, applyPostProcess);
}

void EngineContext::Draw(Sprite* sprite, Texture* texture, std::optional<BlendMode> blendMode, bool applyPostProcess) {
   if (!sRenderer_) return;
   sRenderer_->Draw(sprite, texture, blendMode, applyPostProcess);
}

void EngineContext::Draw(ParticleSystem* particleSystem, bool applyPostProcess) {
   if (!sRenderer_) return;
   sRenderer_->Draw(particleSystem, applyPostProcess);
}

void EngineContext::DrawUI(Sprite* sprite, Texture* texture,
   Sprite::AnchorPoint anchorPoint,
   std::optional<BlendMode> blendMode,
   bool applyPostProcess,
   uint32_t screenWidth,
   uint32_t screenHeight) {
   if (!sRenderer_) return;
   sRenderer_->DrawUI(sprite, texture, anchorPoint, blendMode, applyPostProcess, screenWidth, screenHeight);
}

#ifdef USE_IMGUI
bool EngineContext::GetIsSceneHovered() {
   if (!sRenderer_) return false;
   return sRenderer_->GetIsSceneHovered();
}

bool EngineContext::GetIsDockSpaceVisible() {
   if (!sRenderer_) return false;
   return sRenderer_->GetIsDockSpaceVisible();
}

void EngineContext::SetDockSpaceVisible(bool visible) {
   if (!sRenderer_) return;
   sRenderer_->SetDockSpaceVisible(visible);
}

#endif // USE_IMGUI

void EngineContext::DrawLine(const Vector3& start, const Vector3& end, const Vector4& color, bool applyPostProcess) {
   if (!sRenderer_) return;
   sRenderer_->DrawLine(start, end, color, applyPostProcess);
}

void EngineContext::DrawSpline(const std::vector<Vector3>& controlPoints, const Vector4& color, size_t segmentCount, bool applyPostProcess) {
   if (!sRenderer_) return;
   sRenderer_->DrawSpline(controlPoints, color, segmentCount, applyPostProcess);
}

void EngineContext::DrawGrid(GridPlane plane, float gridSize, int thickLineInterval, int range, bool enableFade, float fadeDistance, bool applyPostProcess) {
   if (!sRenderer_) return;
   sRenderer_->DrawGrid(plane, gridSize, thickLineInterval, range, enableFade, fadeDistance, applyPostProcess);
}

void EngineContext::DrawSphere(const Vector3& center, float radius, const Vector4& color, bool applyPostProcess) {
   if (!sRenderer_) return;
   sRenderer_->DrawSphere(center, radius, color, applyPostProcess);
}

void EngineContext::DrawHemisphere(const Vector3& center, float radius, const Vector3& up, const Vector4& color, bool applyPostProcess) {
   if (!sRenderer_) return;
   sRenderer_->DrawHemisphere(center, radius, up, color, applyPostProcess);
}

void EngineContext::DrawCone(const Vector3& apex, float radius, float height, const Vector3& direction, const Vector4& color, bool applyPostProcess) {
   if (!sRenderer_) return;
   sRenderer_->DrawCone(apex, radius, height, direction, color, applyPostProcess);
}

void EngineContext::DrawBox(const Vector3& center, const Vector3& size, const Vector4& color, bool applyPostProcess) {
   if (!sRenderer_) return;
   sRenderer_->DrawBox(center, size, color, applyPostProcess);
}

void EngineContext::DrawCircle(const Vector3& center, float radius, const Vector3& normal, const Vector4& color, bool applyPostProcess) {
   if (!sRenderer_) return;
   sRenderer_->DrawCircle(center, radius, normal, color, applyPostProcess);
}

void EngineContext::SetBlendMode(BlendMode blendMode) {
   if (!sRenderer_) return;
   sRenderer_->SetBlendMode(blendMode);
}

BlendMode EngineContext::GetCurrentBlendMode() {
   if (!sRenderer_) return BlendMode::kBlendModeNormal;
   return sRenderer_->GetCurrentBlendMode();
}

//================================================================
// ポストプロセス制御
//================================================================

void EngineContext::SetPostProcessEnabled(bool enabled) {
   if (!sRenderer_) return;
   auto* postProcessManager = sRenderer_->GetPostProcessManager();
   if (postProcessManager) {
	  if (enabled) {
		 postProcessManager->EnableAllEffects();
	  } else {
		 postProcessManager->DisableAllEffects();
	  }
   }
}

bool EngineContext::IsPostProcessEnabled() {
   if (!sRenderer_) return false;
   auto* postProcessManager = sRenderer_->GetPostProcessManager();
   if (!postProcessManager) return false;

   // すべてのエフェクト名を取得して、いずれかが有効かチェック
   auto effectNames = postProcessManager->GetEffectNames();
   for (const auto& name : effectNames) {
	  if (postProcessManager->IsEffectEnabled(name)) {
		 return true;
	  }
   }
   return false;
}

void EngineContext::SetPostProcessEffectEnabled(const std::string& effectName, bool enabled) {
   if (!sRenderer_) return;
   auto* postProcessManager = sRenderer_->GetPostProcessManager();
   if (postProcessManager) {
	  postProcessManager->SetEffectEnabled(effectName, enabled);
   }
}

bool EngineContext::IsPostProcessEffectEnabled(const std::string& effectName) {
   if (!sRenderer_) return false;
   auto* postProcessManager = sRenderer_->GetPostProcessManager();
   if (!postProcessManager) return false;

   return postProcessManager->IsEffectEnabled(effectName);
}

std::vector<std::string> EngineContext::GetPostProcessEffectNames() {
   if (!sRenderer_) return std::vector<std::string>();
   auto* postProcessManager = sRenderer_->GetPostProcessManager();
   if (!postProcessManager) return std::vector<std::string>();

   return postProcessManager->GetEffectNames();
}

//================================================================
// レンズフレア
//================================================================

void EngineContext::BeginLensFlareOcclusionQuery() {
   if (!sRenderer_) return;
   sRenderer_->BeginLensFlareOcclusionQuery();
}

void EngineContext::EndLensFlareOcclusionQuery() {
   if (!sRenderer_) return;
   sRenderer_->EndLensFlareOcclusionQuery();
}

//================================================================
// JSON データマネージャー
//================================================================

// グループ操作

bool EngineContext::HasJsonGroup(const std::string& groupName) {
   if (!sJsonDataManager_) return false;
   return sJsonDataManager_->HasGroup(groupName);
}

bool EngineContext::RemoveJsonGroup(const std::string& groupName) {
   if (!sJsonDataManager_) return false;
   return sJsonDataManager_->RemoveGroup(groupName);
}

std::vector<std::string> EngineContext::GetJsonGroupNames() {
   if (!sJsonDataManager_) return std::vector<std::string>();
   return sJsonDataManager_->GetGroupNames();
}

void EngineContext::ClearJsonData() {
   if (!sJsonDataManager_) return;
   sJsonDataManager_->Clear();
}

// データ操作（グループ内）

bool EngineContext::HasJsonKey(const std::string& groupName, const std::string& key) {
   if (!sJsonDataManager_) return false;
   return sJsonDataManager_->Has(groupName, key);
}

bool EngineContext::RemoveJsonKey(const std::string& groupName, const std::string& key) {
   if (!sJsonDataManager_) return false;
   return sJsonDataManager_->Remove(groupName, key);
}

// グループレベルの操作

std::vector<std::string> EngineContext::GetJsonKeys(const std::string& groupName) {
   if (!sJsonDataManager_) return std::vector<std::string>();
   auto group = sJsonDataManager_->GetGroup(groupName);
   if (!group.has_value()) return std::vector<std::string>();
   return group->get().GetKeys();
}

bool EngineContext::IsJsonGroupEmpty(const std::string& groupName) {
   if (!sJsonDataManager_) return true;
   auto group = sJsonDataManager_->GetGroup(groupName);
   if (!group.has_value()) return true;
   return group->get().IsEmpty();
}

size_t EngineContext::GetJsonGroupSize(const std::string& groupName) {
   if (!sJsonDataManager_) return 0;
   auto group = sJsonDataManager_->GetGroup(groupName);
   if (!group.has_value()) return 0;
   return group->get().Size();
}

void EngineContext::ClearJsonGroup(const std::string& groupName) {
   if (!sJsonDataManager_) return;
   auto group = sJsonDataManager_->GetGroup(groupName);
   if (group.has_value()) {
	  group->get().Clear();
   }
}

// ファイル操作

bool EngineContext::LoadJsonFile(const std::string& filePath) {
   if (!sJsonDataManager_) return false;
   return sJsonDataManager_->LoadFromFile(filePath);
}

bool EngineContext::SaveJsonFile(const std::string& filePath, int indent) {
   if (!sJsonDataManager_) return false;
   return sJsonDataManager_->SaveToFile(filePath, indent);
}

// 検索機能

std::vector<std::string> EngineContext::FindJsonGroups(std::function<bool(const std::string&)> predicate) {
   if (!sJsonDataManager_) return std::vector<std::string>();

   std::vector<std::string> result;
   auto groupNames = sJsonDataManager_->GetGroupNames();

   for (const auto& name : groupNames) {
	  if (predicate(name)) {
		 result.push_back(name);
	  }
   }

   return result;
}

}
