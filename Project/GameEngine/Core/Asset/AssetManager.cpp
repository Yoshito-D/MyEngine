#include "pch.h"
#include "AssetManager.h"
#include "Graphics/GraphicsDevice.h"
#include "Audio.h"

namespace GameEngine {
void AssetManager::Initialize(GraphicsDevice* device, Audio* audio) {
   assert(device != nullptr);
   assert(audio != nullptr);
   materialManager_->Initialize(device->GetDevice());
   modelAssetManager_->Initialize(device->GetDevice());
   textureManager_->Initialize(device);
   soundManager_->Initialize(audio);
}
}