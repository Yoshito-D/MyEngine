#include "pch.h"
#include "ModelAssetManager.h"
#include <cassert>

namespace {
Logger& log_ = Logger::GetInstance();
}

namespace GameEngine {
void ModelAssetManager::Initialize(ID3D12Device* device) {
   assert(device);
   device_ = device;
}

void* ModelAssetManager::LoadModel(const std::string& modelPath, const std::string& modelName) {
   auto it = modelAssets_.find(modelName);
   if (it != modelAssets_.end()) {
	  log_.Log("Model already loaded: " + modelName);
	  return it->second.get();
   }

   auto model = std::make_unique<ModelAsset>();
   model->LoadFile(device_, modelPath, modelName);

   ModelAsset* modelPtr = model.get();
   modelAssets_[modelName] = std::move(model);
   log_.Log("Model loaded: " + modelName);
   return modelPtr;
}

ModelAsset* ModelAssetManager::GetModel(const std::string& modelName) {
   auto it = modelAssets_.find(modelName);
   if (it != modelAssets_.end()) {
	  log_.Log("Model found: " + modelName);
	  return it->second.get();
   }
   log_.Log("Model not found: " + modelName);
   return nullptr;
}

void ModelAssetManager::Clear() {
   modelAssets_.clear();
}
}