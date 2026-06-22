#pragma once

#ifdef USE_IMGUI

#include <filesystem>
#include <string>
#include <vector>

namespace GameEngine {

struct EditorModelAssetEntry {
   std::string assetId;
   std::string displayName;
   std::filesystem::path filePath;
};

struct EditorParticleAssetEntry {
   std::string assetId;
   std::string displayName;
   std::filesystem::path filePath;
};

class EditorAssetRegistry {
public:
   void Scan(const std::filesystem::path& resourcesRoot = "resources");

   const std::vector<EditorModelAssetEntry>& GetModelAssets() const { return modelAssets_; }
   const std::vector<EditorParticleAssetEntry>& GetParticleAssets() const { return particleAssets_; }
   const EditorModelAssetEntry* FindModelAsset(const std::string& assetId) const;
   const EditorParticleAssetEntry* FindParticleAsset(const std::string& assetId) const;

   static std::string NormalizeAssetId(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot = "resources");

private:
   std::vector<EditorModelAssetEntry> modelAssets_;
   std::vector<EditorParticleAssetEntry> particleAssets_;
};

} // namespace GameEngine

#endif
