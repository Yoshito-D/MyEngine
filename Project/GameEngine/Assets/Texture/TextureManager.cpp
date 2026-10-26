#include "GameEngine/pch.h"
#include "GameEngine/Assets/Texture/TextureManager.h"
#include "GameEngine/Assets/ResourceAssetPath.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace GameEngine {
bool TextureManager::Initialize(GraphicsDevice* device) {
   Clear();
   catalog_.clear();
   device_ = device;
   try {
      if (!device) {
         throw std::runtime_error("Graphics device is unavailable");
      }
      resourcesRoot_ = "Resources";

      for (const auto* rootId : kTextureRoots) {
         const auto cooked = std::filesystem::path(rootId) / kTextureCookedDirectory;
         std::ifstream stream(resourcesRoot_ / cooked / "catalog.json");
         nlohmann::json catalog;
         stream >> catalog;

         if (catalog.at("version").get<std::int32_t>() != kTextureCookVersion || !catalog.at("textures").is_array()) {
            throw std::runtime_error("Unsupported texture catalog version or structure");
         }

         for (const auto& entry : catalog.at("textures")) {
            TextureInfo info;
            info.name = entry.at("name").get<std::string>();
            info.sourcePath = ResourceAssetIdPath(entry.at("sourcePath").get<std::string>());
            info.ddsPath = ResourceAssetIdPath(entry.at("ddsPath").get<std::string>());
            const auto namePath = ResourceAssetIdPath(info.name);
            const auto expected = cooked / std::filesystem::path(namePath.wstring() + L".dds");

            if (namePath.empty() || namePath.has_parent_path() || info.sourcePath.empty() ||
                ResourcePathToUtf8(info.sourcePath.stem()) != info.name || info.ddsPath != expected) {
               throw std::runtime_error("Invalid texture catalog path: " + info.name);
            }

            info.width = entry.at("width").get<std::uint32_t>();
            info.height = entry.at("height").get<std::uint32_t>();
            info.mipLevels = entry.at("mipLevels").get<std::uint32_t>();
            info.format = static_cast<DXGI_FORMAT>(entry.at("format").get<std::uint32_t>());
            info.isCubemap = entry.at("isCubemap").get<bool>();

            if (!info.width || !info.height || info.width > 16384 || info.height > 16384 || !info.mipLevels ||
                info.mipLevels > 15 || !catalog_.emplace(info.name, info).second) {
               throw std::runtime_error("Invalid or duplicate texture catalog entry: " + info.name);
            }
         }
      }

      Logger::Info(std::format("Texture catalog: {} available, 0 loaded", catalog_.size()));
      return true;
   } catch (const std::exception& error) {
      catalog_.clear();
      Logger::Error("Cooked texture catalog unavailable. Delete the affected cached DDS and restart, or rebuild: " +
                    std::string(error.what()));
      return false;
   }
}

const TextureInfo* TextureManager::FindTextureInfo(const std::string& textureName) const {
   const auto found = catalog_.find(textureName);
   return found == catalog_.end() ? nullptr : &found->second;
}

Texture* TextureManager::GetTexture(const std::string& textureName) {
   if (const auto loaded = textures_.find(textureName); loaded != textures_.end()) {
      return loaded->second.get();
   }
   if (!device_ || textureName.empty() || failedTextureNames_.contains(textureName)) {
      return nullptr;
   }
   const auto* info = FindTextureInfo(textureName);
   const auto path =
      info ? ResolveResourceAssetPath(ResourcePathToUtf8(info->ddsPath), resourcesRoot_) : std::filesystem::path{};
   if (!info || path.empty()) {
      failedTextureNames_.insert(textureName);
      Logger::Error(
         "Cooked texture is missing. Delete the affected cached DDS and restart, or rebuild: " + textureName);
      return nullptr;
   }
   auto texture = std::make_unique<Texture>();
   auto intermediate = texture->LoadTexture(device_, path, *info);
   if (!intermediate) {
      failedTextureNames_.insert(textureName);
      Logger::Error(
         "Cooked texture could not be loaded. Delete the affected cached DDS and restart, or rebuild: " + textureName);
      return nullptr;
   }
   intermediateResource_.push_back(std::move(intermediate));
   auto* result = texture.get();
   textures_.emplace(textureName, std::move(texture));
   if (info->isCubemap) {
      lastCubemapName_ = textureName;
   }
   Logger::Info("Texture loaded: " + textureName);
   return result;
}

bool TextureManager::Preload(std::span<const std::string> textureNames) {
   const auto start = std::chrono::steady_clock::now();
   const auto previousCount = textures_.size();
   bool succeeded = true;
   for (const auto& id : textureNames) {
      if (!GetTexture(id)) {
         succeeded = false;
      }
   }
   const double milliseconds =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
   Logger::Info(std::format("Scene textures: {} referenced, {} new, {} loaded, {:.3f} ms, success={}",
      textureNames.size(), textures_.size() - previousCount, textures_.size(), milliseconds, succeeded));
   return succeeded;
}

void TextureManager::RefreshFailedLoads() {
   failedTextureNames_.clear();
}

std::vector<std::string> TextureManager::GetTextureNames() const {
   std::vector<std::string> names;
   for (const auto& [id, info] : catalog_) {
      (void)info;
      names.push_back(id);
   }
   return names;
}

std::vector<std::string> TextureManager::GetCubemapTextureNames() const {
   std::vector<std::string> names;
   for (const auto& [id, info] : catalog_) {
      if (info.isCubemap) {
         names.push_back(id);
      }
   }
   return names;
}

Texture* TextureManager::GetLastCubemapTexture() const {
   const auto found = textures_.find(lastCubemapName_);
   return found == textures_.end() ? nullptr : found->second.get();
}

void TextureManager::ReleaseIntermediateResources() {
   if (intermediateResource_.empty()) {
      return;
   }
   intermediateResource_.clear();
   Logger::Info("Intermediate resources released.");
}

void TextureManager::Clear() {
   textures_.clear();
   failedTextureNames_.clear();
   intermediateResource_.clear();
   lastCubemapName_.clear();
}
}
