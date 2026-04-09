#include "pch.h"
#include "ModelAssetManager.h"
#include "Graphics/GraphicsDevice.h"
#include <cassert>
#include <algorithm>

namespace {
Logger& log_ = Logger::GetInstance();
}

namespace GameEngine {
void ModelAssetManager::Initialize(GraphicsDevice* device) {
   assert(device);
   device_ = device;
}

ModelAssetManager::ModelHandle ModelAssetManager::LoadModel(const std::string& modelPath, const std::string& modelName) {
   auto it = modelAssets_.find(modelName);
   if (it != modelAssets_.end()) {
	  log_.Log("Model already loaded: " + modelName);
    return it->second;
   }

   auto model = std::make_shared<ModelAsset>();
   model->LoadFile(device_, modelPath, modelName);

   modelAssets_[modelName] = std::move(model);
   log_.Log("Model loaded: " + modelName);
   return modelAssets_[modelName];
}

ModelAssetManager::ModelHandle ModelAssetManager::GetModel(const std::string& modelName) {
   auto it = modelAssets_.find(modelName);
   if (it != modelAssets_.end()) {
    return it->second;
   }
   log_.Log("Model not found: " + modelName);
   return {};
}

void ModelAssetManager::Clear() {
   modelAssets_.clear();
}

std::vector<std::string> ModelAssetManager::GetModelNames() const {
   std::vector<std::string> names;
   names.reserve(modelAssets_.size());
   for (const auto& [name, asset] : modelAssets_) {
	  (void)asset;
	  names.push_back(name);
   }
   std::sort(names.begin(), names.end());
   return names;
}
}