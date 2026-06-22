#include "pch.h"
#include "EditorAssetRegistry.h"

#ifdef USE_IMGUI

#include <algorithm>

namespace GameEngine {

namespace {
bool IsSupportedModelExtension(const std::filesystem::path& path) {
   const std::string ext = path.extension().string();
   return ext == ".obj" || ext == ".gltf";
}

std::string ToGenericString(std::filesystem::path path) {
   return path.lexically_normal().generic_string();
}
} // namespace

void EditorAssetRegistry::Scan(const std::filesystem::path& resourcesRoot) {
   modelAssets_.clear();
   particleAssets_.clear();

   const std::filesystem::path modelsRoot = resourcesRoot / "models";
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
         modelEntry.displayName = modelEntry.assetId;
         modelEntry.filePath = path;
         modelAssets_.push_back(std::move(modelEntry));
      }
   }

   const std::filesystem::path particlesRoot = resourcesRoot / "particles";
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
         particleEntry.displayName = particleEntry.assetId;
         particleEntry.filePath = path;
         particleAssets_.push_back(std::move(particleEntry));
      }
   }

   std::sort(modelAssets_.begin(), modelAssets_.end(),
      [](const EditorModelAssetEntry& lhs, const EditorModelAssetEntry& rhs) {
         return lhs.assetId < rhs.assetId;
      });

   std::sort(particleAssets_.begin(), particleAssets_.end(),
      [](const EditorParticleAssetEntry& lhs, const EditorParticleAssetEntry& rhs) {
         return lhs.assetId < rhs.assetId;
      });
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

std::string EditorAssetRegistry::NormalizeAssetId(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot) {
   std::error_code error;
   std::filesystem::path relative = std::filesystem::relative(path, resourcesRoot, error);
   if (error) {
      relative = path;
   }
   return ToGenericString(relative);
}

} // namespace GameEngine

#endif
