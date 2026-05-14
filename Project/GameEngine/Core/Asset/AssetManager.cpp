#include "pch.h"
#include "AssetManager.h"
#include "Graphics/GraphicsDevice.h"
#include "Audio.h"
#include "Component/MaterialComponent.h"
#include "Graphics/Texture.h"

namespace GameEngine {
void AssetManager::Initialize(GraphicsDevice* device, Audio* audio) {
   assert(device != nullptr);
   assert(audio != nullptr);
   materialManager_->Initialize(device->GetDevice());
   MaterialComponent::SetMaterialResolver([this](const std::string& name) {
      return materialManager_ ? materialManager_->GetMaterial(name) : nullptr;
   });
   MaterialComponent::SetMaterialCreator([this](const std::string& name, uint32_t color, int32_t lightingMode, const Matrix4x4& uvTransform) {
      return materialManager_ ? static_cast<Material*>(materialManager_->CreateMaterial(name, color, lightingMode, uvTransform)) : nullptr;
   });
   MaterialComponent::SetMaterialNamesProvider([this]() {
      return materialManager_ ? materialManager_->GetMaterialNames() : std::vector<std::string>{};
   });
   MaterialComponent::SetEnvironmentTextureResolver([this](const std::string& name) -> Texture* {
      return textureManager_ ? textureManager_->GetTexture(name) : nullptr;
   });
   MaterialComponent::SetEnvironmentTextureNamesProvider([this]() {
      return textureManager_ ? textureManager_->GetCubemapTextureNames() : std::vector<std::string>{};
   });
   modelAssetManager_->Initialize(device);
   textureManager_->Initialize(device);
   soundManager_->Initialize(audio);
}
}