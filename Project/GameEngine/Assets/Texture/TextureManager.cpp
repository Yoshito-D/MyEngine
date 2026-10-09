#include "GameEngine/pch.h"
#include "GameEngine/Assets/Texture/TextureManager.h"
#include "GameEngine/Assets/ResourceAssetPath.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace {
bool IsSupportedTextureExtension(const std::filesystem::path& path) {
   std::string ext = GameEngine::ResourcePathToUtf8(path.extension());
   // Windows上でも入力表記に依存しないよう、拡張子だけを小文字へ正規化する。
   std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
   });
   return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds";
}

std::string ToGenericString(std::filesystem::path path) {
   return GameEngine::ResourcePathToUtf8(path.lexically_normal());
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

std::filesystem::path ResolveResourceTexturePath(const std::string& assetId) {
   const auto path = GameEngine::ResourceAssetIdPath(assetId);
   return !path.empty() && IsSupportedTextureExtension(path) ? GameEngine::ResolveResourceAssetPath(assetId) : std::filesystem::path{};
}
}

namespace GameEngine {
void TextureManager::Initialize(GraphicsDevice* device) {
   assert(device != nullptr);
   device_ = device;
   intermediateResource_.clear();
   failedTextureNames_.clear();
}

void TextureManager::LoadTexture(const std::string& filePath, const std::string& name) {
   if (textures_.find(name) != textures_.end()) {
	  return;
   }
   if (!device_ || failedTextureNames_.contains(name)) return;

   auto texture = std::make_unique<Texture>();
   Microsoft::WRL::ComPtr<ID3D12Resource> intermediate = texture->LoadTexture(device_, filePath);
   if (!intermediate || !texture->GetWidth() || !texture->GetHeight()) {
      failedTextureNames_.insert(name);
      return;
   }
   // GPUコピー完了まではアップロード用リソースが必要なため、明示解放まで所有する。
   intermediateResource_.push_back(intermediate);

   textures_[name] = std::move(texture);
   if (textures_[name]->GetMetadata().IsCubemap()) {
	  lastCubemapName_ = name;
   }
   Logger::Info("Texture loaded: " + name);
}

void TextureManager::LoadTexturesFromDirectory(const std::filesystem::path& directoryPath, const std::filesystem::path& resourcesRoot) {
   std::error_code error;
   if (!std::filesystem::is_directory(directoryPath, error)) {
      Logger::Warning("Texture directory not found: " + directoryPath.generic_string());
      return;
   }

   // サブフォルダー名を含むアセットIDを作るため、対象ルートを再帰的に走査する。
   std::filesystem::recursive_directory_iterator iterator(directoryPath, std::filesystem::directory_options::skip_permission_denied, error);
   const std::filesystem::recursive_directory_iterator end;
   for (; !error && iterator != end; iterator.increment(error)) {
      const auto& entry = *iterator;
      if (!entry.is_regular_file(error)) {
         error.clear();
         continue;
      }

      const auto& path = entry.path();
      if (!IsSupportedTextureExtension(path)) {
         continue;
      }

      const std::string assetId = NormalizeAssetId(path, resourcesRoot);
      LoadTexture(ToGenericString(path), assetId);
      // ゲームコードが使う論理名も登録する。Projectで保存する参照は完全IDなので衝突しない。
      RegisterAlias(ToGenericString(path.stem()), assetId);
      RegisterAlias(ToGenericString(path.filename()), assetId);
   }
   if (error) Logger::Warning("Texture directory scan failed: " + directoryPath.generic_string());
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

   if (!device_ || failedTextureNames_.contains(name)) return nullptr;
   const auto path = ResolveResourceTexturePath(name);
   if (path.empty()) {
      failedTextureNames_.insert(name);
      return nullptr;
   }
   // 短縮名の推測は行わず、検証済みの完全IDだけで読込と登録を行う。
   LoadTexture(ToGenericString(path), name);
   const auto loaded = textures_.find(name);
   return loaded != textures_.end() ? loaded->second.get() : nullptr;
}

void TextureManager::RefreshFailedLoads() {
   failedTextureNames_.clear();
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
   failedTextureNames_.clear();
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
