#include "GameEngine/pch.h"
#include "GameEngine/Assets/Model/ModelAssetManager.h"
#include "GameEngine/Assets/ResourceAssetPath.h"
#include "GameEngine/Assets/Animation/AnimationAssetManager.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include <cassert>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace {
bool HasGltfExtension(const std::string& fileName) {
   std::string extension = GameEngine::ResourcePathToUtf8(std::filesystem::path(std::u8string(fileName.begin(), fileName.end())).extension());
   std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
      return static_cast<char>(std::tolower(character));
   });
   return extension == ".gltf";
}

bool IsSupportedModelExtension(const std::filesystem::path& path) {
   std::string extension = GameEngine::ResourcePathToUtf8(path.extension());
   std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
      return static_cast<char>(std::tolower(character));
   });
   return extension == ".obj" || extension == ".gltf" || extension == ".fbx";
}
}

namespace GameEngine {
void ModelAssetManager::Initialize(GraphicsDevice* device, AnimationAssetManager* animationAssetManager) {
   assert(device);
   assert(animationAssetManager);
   device_ = device;
   animationAssetManager_ = animationAssetManager;
}

ModelAssetManager::ModelHandle ModelAssetManager::LoadModel(const std::string& modelPath, const std::string& modelName) {
   auto it = modelAssets_.find(modelName);
   if (it != modelAssets_.end()) {
	  RegisterGltfAnimation(modelPath, modelName);
	  Logger::Info("Model already loaded: " + modelName);
    return it->second;
   }

   const std::string assetId = BuildAssetId(modelPath, modelName);
   auto idIt = modelAssetsById_.find(assetId);
   if (!assetId.empty() && idIt != modelAssetsById_.end()) {
      // 短いモデル名でも同じアセットIDなら共有し、GPUリソースを二重ロードしない。
      modelAssets_[modelName] = idIt->second;
      RegisterGltfAnimation(modelPath, modelName);
      return idIt->second;
   }

   return LoadModelInternal(modelPath, modelName, assetId);
}

ModelAssetManager::ModelHandle ModelAssetManager::LoadModelByAssetId(const std::string& assetId) {
   const auto relativePath = ResourceAssetIdPath(assetId);
   if (relativePath.empty() || !IsSupportedModelExtension(relativePath)) return {};
   auto idIt = modelAssetsById_.find(assetId);
   if (idIt != modelAssetsById_.end()) {
      return idIt->second;
   }
   const auto path = ResolveResourceAssetPath(assetId);
   if (path.empty()) {
      Logger::Error("Model asset is missing or outside resources: " + assetId);
      return {};
   }
   // 永続化IDは維持し、検証済みの実際のパスだけをAssimpへ渡す。
   return LoadModelInternal(ResourcePathToUtf8(path.parent_path()), ResourcePathToUtf8(path.filename()), assetId);
}

ModelAssetManager::ModelHandle ModelAssetManager::LoadModelInternal(const std::string& modelPath, const std::string& modelName, const std::string& assetId) {
   auto model = std::make_shared<ModelAsset>();
   model->SetAssetId(assetId);
   if (!model->LoadFile(device_, modelPath, modelName)) return {};

   // ゲームコード用の論理名と、永続化用の完全IDは同じアセットを共有する。
   modelAssets_[modelName] = model;
   if (!assetId.empty()) modelAssetsById_[assetId] = model;
   RegisterGltfAnimation(modelPath, modelName);
   Logger::Info("Model loaded: " + modelName);
   return modelAssets_[modelName];
}

ModelAssetManager::ModelHandle ModelAssetManager::GetModel(const std::string& modelName) {
   auto it = modelAssets_.find(modelName);
   if (it != modelAssets_.end()) {
    return it->second;
   }
   Logger::Info("Model not found: " + modelName);
   return {};
}

ModelAssetManager::ModelHandle ModelAssetManager::GetModelByAssetId(const std::string& assetId) {
   const auto path = ResourceAssetIdPath(assetId);
   if (path.empty() || !IsSupportedModelExtension(path)) return {};
   auto it = modelAssetsById_.find(assetId);
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
   // エディタ選択肢がハッシュ配置で並び替わらないよう、公開直前に名前順へ揃える。
   std::sort(names.begin(), names.end());
   return names;
}

std::string ModelAssetManager::BuildAssetId(const std::string& modelPath, const std::string& modelName) {
   std::error_code error;
   const auto root = std::filesystem::absolute("resources", error);
   if (error) return {};
   const std::filesystem::path directory(std::u8string(modelPath.begin(), modelPath.end()));
   const std::filesystem::path filename(std::u8string(modelName.begin(), modelName.end()));
   const auto path = std::filesystem::absolute(directory / filename, error);
   if (error) return {};
   const auto assetId = ResourcePathToUtf8(path.lexically_normal().lexically_relative(root.lexically_normal()));
   return ResourceAssetIdPath(assetId).empty() ? std::string{} : assetId;
}

void ModelAssetManager::RegisterGltfAnimation(const std::string& modelPath, const std::string& modelName) {
   if (!animationAssetManager_ || !HasGltfExtension(modelName)) {
      return;
   }

   // モデルと同じglTFをアニメーション管理にも登録し、個別の明示ロードを不要にする。
   animationAssetManager_->LoadAnimation(modelPath, modelName);
}
}
