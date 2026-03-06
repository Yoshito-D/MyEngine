#include "pch.h"
#include "TextureManager.h"
#include "Graphics/GraphicsDevice.h"

namespace {
Logger& log_ = Logger::GetInstance();
}

namespace GameEngine {
void TextureManager::Initialize(GraphicsDevice* device) {
   assert(device != nullptr);
   device_ = device;
   intermediateResource_.clear();
}

void TextureManager::LoadTexture(const std::string& filePath, const std::string& name) {
   if (textures_.find(name) != textures_.end()) {
	  log_.Log("Texture already loaded: " + name);
	  return;
   }

   auto texture = std::make_unique<Texture>();
   Microsoft::WRL::ComPtr<ID3D12Resource> intermediate = texture->LoadTexture(device_, filePath);
   intermediateResource_.push_back(intermediate);

   textures_[name] = std::move(texture);
   log_.Log("Texture loaded: " + name);
}


Texture* TextureManager::GetTexture(const std::string& name) {
   auto it = textures_.find(name);
   if (it != textures_.end()) {
	  return it->second.get();
   }
   log_.Log("Texture not found: " + name);
   return nullptr;
}

void TextureManager::ReleaseIntermediateResources() {
   if (intermediateResource_.empty()) return;

   for (auto& resource : intermediateResource_) {
	  if (resource) {
		 resource.Reset();
	  }
   }
   intermediateResource_.clear();
   log_.Log("Intermediate resources released.");
}

void TextureManager::Clear() {
   textures_.clear();
   intermediateResource_.clear();
}
}