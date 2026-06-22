#include "pch.h"
#include "ModelAssetManager.h"
#include "Graphics/GraphicsDevice.h"
#include <cassert>
#include <algorithm>
#include <filesystem>

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

   const std::string assetId = BuildAssetId(modelPath, modelName);
   auto idIt = modelAssetsById_.find(assetId);
   if (idIt != modelAssetsById_.end()) {
      modelAssets_[modelName] = idIt->second;
      return idIt->second;
   }

   return LoadModelInternal(modelPath, modelName, assetId);
}

ModelAssetManager::ModelHandle ModelAssetManager::LoadModelByAssetId(const std::string& assetId) {
   const std::string normalizedAssetId = NormalizeAssetId(assetId);
   auto idIt = modelAssetsById_.find(normalizedAssetId);
   if (idIt != modelAssetsById_.end()) {
      return idIt->second;
   }

   const std::filesystem::path relativePath(normalizedAssetId);
   const std::filesystem::path directory = std::filesystem::path("resources") / relativePath.parent_path();
   const std::string modelName = relativePath.filename().string();
   if (modelName.empty()) {
      return {};
   }

   return LoadModelInternal(directory.generic_string(), modelName, normalizedAssetId);
}

ModelAssetManager::ModelHandle ModelAssetManager::LoadModelInternal(const std::string& modelPath, const std::string& modelName, const std::string& assetId) {
   auto model = std::make_shared<ModelAsset>();
   model->SetAssetId(assetId);
   model->LoadFile(device_, modelPath, modelName);

   modelAssets_[modelName] = std::move(model);
   modelAssetsById_[assetId] = modelAssets_[modelName];
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

ModelAssetManager::ModelHandle ModelAssetManager::GetModelByAssetId(const std::string& assetId) {
   const std::string normalizedAssetId = NormalizeAssetId(assetId);
   auto it = modelAssetsById_.find(normalizedAssetId);
   if (it != modelAssetsById_.end()) {
    return it->second;
   }
   return {};
}

void ModelAssetManager::Clear() {
   modelAssets_.clear();
   modelAssetsById_.clear();
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

std::string ModelAssetManager::NormalizeAssetId(const std::string& path) {
   std::filesystem::path normalizedPath(path);
   std::string result = normalizedPath.lexically_normal().generic_string();
   constexpr const char* kResourcesPrefix = "resources/";
   if (result.rfind(kResourcesPrefix, 0) == 0) {
      result = result.substr(std::char_traits<char>::length(kResourcesPrefix));
   }
   return result;
}

std::string ModelAssetManager::BuildAssetId(const std::string& modelPath, const std::string& modelName) {
   return NormalizeAssetId((std::filesystem::path(modelPath) / modelName).generic_string());
}
}
