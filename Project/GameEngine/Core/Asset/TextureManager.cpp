#include "pch.h"
#include "TextureManager.h"
#include "Graphics/GraphicsDevice.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace {
Logger& log_ = Logger::GetInstance();

bool IsSupportedTextureExtension(const std::filesystem::path& path) {
   std::string ext = path.extension().string();
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
	  log_.Log("Texture already loaded: " + name);
	  return;
   }

   auto texture = std::make_unique<Texture>();
   Microsoft::WRL::ComPtr<ID3D12Resource> intermediate = texture->LoadTexture(device_, filePath);
   intermediateResource_.push_back(intermediate);

   textures_[name] = std::move(texture);
   if (textures_[name]->GetMetadata().IsCubemap()) {
	  lastCubemapName_ = name;
   }
   log_.Log("Texture loaded: " + name);
}

void TextureManager::LoadTexturesFromDirectory(const std::filesystem::path& directoryPath, const std::filesystem::path& resourcesRoot) {
   if (!std::filesystem::exists(directoryPath)) {
      log_.Log("Texture directory not found: " + directoryPath.generic_string(), Logger::LogLevel::Warning);
      return;
   }

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

   log_.Log("Texture not found: " + name);
   return nullptr;
}

std::vector<std::string> TextureManager::GetTextureNames() const {
   std::vector<std::string> names;
   names.reserve(textures_.size());
   for (const auto& [name, texture] : textures_) {
	  (void)texture;
	  names.push_back(name);
   }
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
   log_.Log("Intermediate resources released.");
}

void TextureManager::Clear() {
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
      return;
   }

   textureAliases_[alias] = ownerIt->second.get();
}
}
