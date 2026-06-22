#pragma once

#ifdef USE_IMGUI

#include <filesystem>
#include <string>
#include <vector>

namespace GameEngine {

enum class EditorAssetType {
   Folder,
   Model,
   Texture,
   Particle,
   Scene,
   Material,
   Prefab,
   Json,
   Unknown,
};

struct EditorAssetEntry {
   EditorAssetType type = EditorAssetType::Unknown;
   std::string assetId;
   std::string displayName;
   std::filesystem::path filePath;
};

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

struct EditorTextureAssetEntry {
   std::string assetId;
   std::string displayName;
   std::filesystem::path filePath;
};

class EditorAssetRegistry {
public:
   void Scan(const std::filesystem::path& resourcesRoot = "resources");

   const std::vector<EditorAssetEntry>& GetAllAssets() const { return allAssets_; }
   const std::vector<EditorModelAssetEntry>& GetModelAssets() const { return modelAssets_; }
   const std::vector<EditorParticleAssetEntry>& GetParticleAssets() const { return particleAssets_; }
   const std::vector<EditorTextureAssetEntry>& GetTextureAssets() const { return textureAssets_; }
   const EditorAssetEntry* FindAsset(const std::string& assetId) const;
   const EditorModelAssetEntry* FindModelAsset(const std::string& assetId) const;
   const EditorParticleAssetEntry* FindParticleAsset(const std::string& assetId) const;
   const EditorTextureAssetEntry* FindTextureAsset(const std::string& assetId) const;

   static std::string NormalizeAssetId(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot = "resources");
   static const char* GetAssetTypeLabel(EditorAssetType type);

private:
   static EditorAssetType ClassifyAsset(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot);

   std::vector<EditorAssetEntry> allAssets_;
   std::vector<EditorModelAssetEntry> modelAssets_;
   std::vector<EditorParticleAssetEntry> particleAssets_;
   std::vector<EditorTextureAssetEntry> textureAssets_;
};

} // namespace GameEngine

#endif
