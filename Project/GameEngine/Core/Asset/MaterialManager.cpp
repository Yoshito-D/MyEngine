#include "pch.h"
#include "MaterialManager.h"

namespace GameEngine {
void MaterialManager::Initialize(ID3D12Device* device) {
   assert(device);
   device_ = device;
}

void* MaterialManager::CreateMaterial(const std::string& name, uint32_t color, int32_t enableLighting, const Matrix4x4& uvTransform) {
   auto it = materials_.find(name);
   if (it != materials_.end()) {
	  return it->second.get();
   }

   auto material = std::make_unique<Material>();
   material->Create(color, enableLighting, uvTransform);

   Material* ptr = material.get();
   materials_[name] = std::move(material);
   return ptr;
}

Material* MaterialManager::GetMaterial(const std::string& name) const {
   auto it = materials_.find(name);
   return (it != materials_.end()) ? it->second.get() : nullptr;
}

void MaterialManager::Clear() {
   materials_.clear();
}
}