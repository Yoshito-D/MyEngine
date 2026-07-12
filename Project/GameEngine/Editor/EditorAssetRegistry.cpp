#include "pch.h"
#include "EditorAssetRegistry.h"

#ifdef USE_IMGUI

#include <algorithm>
#include <cctype>

namespace GameEngine {

namespace {
std::string ToLowerExtension(const std::filesystem::path& path) {
   std::string ext = path.extension().string();
   std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
   });
   return ext;
}

bool IsSupportedModelExtension(const std::filesystem::path& path) {
   const std::string ext = ToLowerExtension(path);
   return ext == ".obj" || ext == ".gltf" || ext == ".fbx";
}

bool IsSupportedTextureExtension(const std::filesystem::path& path) {
   const std::string ext = ToLowerExtension(path);
   return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds";
}

std::string ToGenericString(std::filesystem::path path) {
   return path.lexically_normal().generic_string();
}

std::string BuildDisplayName(const std::filesystem::path& path) {
   if (std::filesystem::is_directory(path)) {
      return path.filename().string();
   }

   const std::string stem = path.stem().string();
   if (!stem.empty()) {
      return stem;
   }
   return path.filename().string();
}
} // namespace

void EditorAssetRegistry::Scan(const std::filesystem::path& resourcesRoot) {
   allAssets_.clear();
   modelAssets_.clear();
   particleAssets_.clear();
   textureAssets_.clear();

   if (std::filesystem::exists(resourcesRoot)) {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(resourcesRoot)) {
         const auto& path = entry.path();

         EditorAssetEntry assetEntry{};
         assetEntry.type = entry.is_directory() ? EditorAssetType::Folder : ClassifyAsset(path, resourcesRoot);
         if (assetEntry.type == EditorAssetType::Unknown) {
            continue;
         }

         assetEntry.assetId = NormalizeAssetId(path, resourcesRoot);
         assetEntry.displayName = BuildDisplayName(path);
         if (assetEntry.displayName.empty()) {
            assetEntry.displayName = assetEntry.assetId;
         }
         assetEntry.filePath = path;
         allAssets_.push_back(assetEntry);

         if (!entry.is_regular_file()) {
            continue;
         }

         if (assetEntry.type == EditorAssetType::Model) {
            EditorModelAssetEntry modelEntry{};
            modelEntry.assetId = assetEntry.assetId;
            modelEntry.displayName = assetEntry.displayName;
            modelEntry.filePath = assetEntry.filePath;
            modelAssets_.push_back(std::move(modelEntry));
         } else if (assetEntry.type == EditorAssetType::Texture) {
            EditorTextureAssetEntry textureEntry{};
            textureEntry.assetId = assetEntry.assetId;
            textureEntry.displayName = assetEntry.displayName;
            textureEntry.filePath = assetEntry.filePath;
            textureAssets_.push_back(std::move(textureEntry));
         } else if (assetEntry.type == EditorAssetType::Particle) {
            EditorParticleAssetEntry particleEntry{};
            particleEntry.assetId = assetEntry.assetId;
            particleEntry.displayName = assetEntry.displayName;
            particleEntry.filePath = assetEntry.filePath;
            particleAssets_.push_back(std::move(particleEntry));
         }
      }
   }

   const std::filesystem::path modelsRoot = resourcesRoot / "game" / "models";
   if (std::filesystem::exists(modelsRoot)) {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(modelsRoot)) {
         if (!entry.is_regular_file()) {
            continue;
         }

         const auto& path = entry.path();
         if (!IsSupportedModelExtension(path)) {
            continue;
         }

         EditorModelAssetEntry modelEntry{};
         modelEntry.assetId = NormalizeAssetId(path, resourcesRoot);
         modelEntry.displayName = BuildDisplayName(path);
         modelEntry.filePath = path;
         if (!FindModelAsset(modelEntry.assetId)) {
            modelAssets_.push_back(std::move(modelEntry));
         }
      }
   }

   const std::filesystem::path particlesRoot = resourcesRoot / "game" / "particles";
   if (std::filesystem::exists(particlesRoot)) {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(particlesRoot)) {
         if (!entry.is_regular_file()) {
            continue;
         }

         const auto& path = entry.path();
         if (path.extension() != ".json") {
            continue;
         }

         EditorParticleAssetEntry particleEntry{};
         particleEntry.assetId = NormalizeAssetId(path, resourcesRoot);
         particleEntry.displayName = BuildDisplayName(path);
         particleEntry.filePath = path;
         if (!FindParticleAsset(particleEntry.assetId)) {
            particleAssets_.push_back(std::move(particleEntry));
         }
      }
   }

   std::sort(allAssets_.begin(), allAssets_.end(),
      [](const EditorAssetEntry& lhs, const EditorAssetEntry& rhs) {
         if (lhs.filePath.parent_path() == rhs.filePath.parent_path()) {
            if (lhs.type != rhs.type) {
               return lhs.type == EditorAssetType::Folder;
            }
         }
         return lhs.assetId < rhs.assetId;
      });

   std::sort(modelAssets_.begin(), modelAssets_.end(),
      [](const EditorModelAssetEntry& lhs, const EditorModelAssetEntry& rhs) {
         return lhs.assetId < rhs.assetId;
      });

   std::sort(particleAssets_.begin(), particleAssets_.end(),
      [](const EditorParticleAssetEntry& lhs, const EditorParticleAssetEntry& rhs) {
         return lhs.assetId < rhs.assetId;
      });

   std::sort(textureAssets_.begin(), textureAssets_.end(),
      [](const EditorTextureAssetEntry& lhs, const EditorTextureAssetEntry& rhs) {
         return lhs.assetId < rhs.assetId;
      });
}

const EditorAssetEntry* EditorAssetRegistry::FindAsset(const std::string& assetId) const {
   auto it = std::find_if(allAssets_.begin(), allAssets_.end(),
      [&assetId](const EditorAssetEntry& entry) {
         return entry.assetId == assetId;
      });
   return it == allAssets_.end() ? nullptr : &(*it);
}

const EditorModelAssetEntry* EditorAssetRegistry::FindModelAsset(const std::string& assetId) const {
   auto it = std::find_if(modelAssets_.begin(), modelAssets_.end(),
      [&assetId](const EditorModelAssetEntry& entry) {
         return entry.assetId == assetId;
      });
   return it == modelAssets_.end() ? nullptr : &(*it);
}

const EditorParticleAssetEntry* EditorAssetRegistry::FindParticleAsset(const std::string& assetId) const {
   auto it = std::find_if(particleAssets_.begin(), particleAssets_.end(),
      [&assetId](const EditorParticleAssetEntry& entry) {
         return entry.assetId == assetId;
      });
   return it == particleAssets_.end() ? nullptr : &(*it);
}

const EditorTextureAssetEntry* EditorAssetRegistry::FindTextureAsset(const std::string& assetId) const {
   auto it = std::find_if(textureAssets_.begin(), textureAssets_.end(),
      [&assetId](const EditorTextureAssetEntry& entry) {
         return entry.assetId == assetId;
      });
   return it == textureAssets_.end() ? nullptr : &(*it);
}

std::string EditorAssetRegistry::NormalizeAssetId(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot) {
   std::error_code error;
   std::filesystem::path relative = std::filesystem::relative(path, resourcesRoot, error);
   if (error) {
      relative = path;
   }
   return ToGenericString(relative);
}

const char* EditorAssetRegistry::GetAssetTypeLabel(EditorAssetType type) {
   switch (type) {
      case EditorAssetType::Folder:
         return "Folder";
      case EditorAssetType::Model:
         return "Model";
      case EditorAssetType::Texture:
         return "Texture";
      case EditorAssetType::Particle:
         return "Particle";
      case EditorAssetType::Scene:
         return "Scene";
      case EditorAssetType::Material:
         return "Material";
      case EditorAssetType::Prefab:
         return "Prefab";
      case EditorAssetType::Json:
         return "Json";
      case EditorAssetType::Unknown:
      default:
         return "Unknown";
   }
}

EditorAssetType EditorAssetRegistry::ClassifyAsset(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot) {
   if (std::filesystem::is_directory(path)) {
      return EditorAssetType::Folder;
   }

   if (IsSupportedModelExtension(path)) {
      return EditorAssetType::Model;
   }

   if (IsSupportedTextureExtension(path)) {
      return EditorAssetType::Texture;
   }

   const std::string ext = ToLowerExtension(path);
   if (ext != ".json") {
      return EditorAssetType::Unknown;
   }

   const std::string assetId = NormalizeAssetId(path, resourcesRoot);
   const std::filesystem::path relative(assetId);
   if (!relative.empty()) {
      auto first = relative.begin();
      if (first != relative.end()) {
         std::string rootFolder = first->string();
         if (rootFolder == "game" && ++first != relative.end()) {
            rootFolder = first->string();
         }
         if (rootFolder == "particles") {
            return EditorAssetType::Particle;
         }
         if (rootFolder == "scenes") {
            return EditorAssetType::Scene;
         }
         if (rootFolder == "materials") {
            return EditorAssetType::Material;
         }
         if (rootFolder == "prefabs") {
            return EditorAssetType::Prefab;
         }
      }
   }

   const std::string stem = path.stem().string();
   if (stem.find("prefab") != std::string::npos || stem.find("Prefab") != std::string::npos) {
      return EditorAssetType::Prefab;
   }
   if (stem.find("material") != std::string::npos || stem.find("Material") != std::string::npos) {
      return EditorAssetType::Material;
   }
   return EditorAssetType::Json;
}

} // namespace GameEngine

#endif
