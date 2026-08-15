#include "pch.h"
#include "TextureManager.h"
#include "Graphics/GraphicsDevice.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace {
bool IsSupportedTextureExtension(const std::filesystem::path& path) {
   std::string ext = path.extension().string();
   // Windows上でも入力表記に依存しないよう、拡張子だけを小文字へ正規化する。
   std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
   });
   return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds";
}

std::string ToGenericString(std::filesystem::path path) {
   return path.lexically_normal().generic_string();
}

std::string NormalizeAssetId(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot) {
   std::error_code error;
   // 保存IDはresources相対を優先し、相対化不能な別ボリューム等では元パスを失わない。
   std::filesystem::path relative = std::filesystem::relative(path, resourcesRoot, error);
   if (error) {
      relative = path;
   }
   return ToGenericString(relative);
}
}

namespace GameEngine {
void TextureManager::Initialize(GraphicsDevice* device) {
   assert(device != nullptr);
   device_ = device;
   intermediateResource_.clear();
}

void TextureManager::LoadTexture(const std::string& filePath, const std::string& name) {
   if (textures_.find(name) != textures_.end()) {
	  Logger::Info("Texture already loaded: " + name);
	  return;
   }

   auto texture = std::make_unique<Texture>();
   Microsoft::WRL::ComPtr<ID3D12Resource> intermediate = texture->LoadTexture(device_, filePath);
   // GPUコピー完了まではアップロード用リソースが必要なため、明示解放まで所有する。
   intermediateResource_.push_back(intermediate);

   textures_[name] = std::move(texture);
   if (textures_[name]->GetMetadata().IsCubemap()) {
	  lastCubemapName_ = name;
   }
   Logger::Info("Texture loaded: " + name);
}

void TextureManager::LoadTexturesFromDirectory(const std::filesystem::path& directoryPath, const std::filesystem::path& resourcesRoot) {
   if (!std::filesystem::exists(directoryPath)) {
      Logger::Warning("Texture directory not found: " + directoryPath.generic_string());
      return;
   }

   // サブフォルダー名を含むアセットIDを作るため、対象ルートを再帰的に走査する。
   for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath)) {
      if (!entry.is_regular_file()) {
         continue;
      }

      const auto& path = entry.path();
      if (!IsSupportedTextureExtension(path)) {
         continue;
      }

      const std::string assetId = NormalizeAssetId(path, resourcesRoot);
      LoadTexture(path.generic_string(), assetId);
      // 旧シーンの短い名前も解決できるよう別名を登録しつつ、衝突時は先着を維持する。
      RegisterAlias(path.stem().string(), assetId);
      RegisterAlias(path.filename().string(), assetId);
   }
}

Texture* TextureManager::GetTexture(const std::string& name) {
   auto it = textures_.find(name);
   if (it != textures_.end()) {
	  return it->second.get();
   }

   auto aliasIt = textureAliases_.find(name);
   if (aliasIt != textureAliases_.end()) {
      return aliasIt->second;
   }

   Logger::Info("Texture not found: " + name);
   return nullptr;
}

std::vector<std::string> TextureManager::GetTextureNames() const {
   std::vector<std::string> names;
   names.reserve(textures_.size());
   for (const auto& [name, texture] : textures_) {
	  (void)texture;
	  names.push_back(name);
   }
   // unordered_mapの反復順を外へ出さず、Inspectorの候補順を実行ごとに安定させる。
   std::sort(names.begin(), names.end());
   return names;
}

std::vector<std::string> TextureManager::GetCubemapTextureNames() const {
   std::vector<std::string> names;
   for (const auto& [name, texture] : textures_) {
	  if (texture && texture->GetMetadata().IsCubemap()) {
		 names.push_back(name);
	  }
   }
   std::sort(names.begin(), names.end());
   return names;
}

Texture* TextureManager::GetLastCubemapTexture() const {
   if (lastCubemapName_.empty()) {
	  return nullptr;
   }
   auto it = textures_.find(lastCubemapName_);
   return (it != textures_.end()) ? it->second.get() : nullptr;
}

void TextureManager::ReleaseIntermediateResources() {
   if (intermediateResource_.empty()) return;

   for (auto& resource : intermediateResource_) {
	  if (resource) {
		 resource.Reset();
	  }
   }
   intermediateResource_.clear();
   Logger::Info("Intermediate resources released.");
}

void TextureManager::Clear() {
   // AliasはTextureへの非所有ポインタなので、本体破棄と同じ操作で必ず無効化する。
   textures_.clear();
   textureAliases_.clear();
   intermediateResource_.clear();
   lastCubemapName_.clear();
}

void TextureManager::RegisterAlias(const std::string& alias, const std::string& ownerName) {
   if (alias.empty() || alias == ownerName) {
      return;
   }

   auto ownerIt = textures_.find(ownerName);
   if (ownerIt == textures_.end()) {
      return;
   }

   if (textures_.contains(alias) || textureAliases_.contains(alias)) {
      // 同名ファイルが別フォルダーにある場合、曖昧な短縮名を後勝ちで変化させない。
      return;
   }

   textureAliases_[alias] = ownerIt->second.get();
}
}
