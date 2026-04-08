#include "pch.h"
#include "AnimationAssetManager.h"
#include <algorithm>

namespace GameEngine {

AnimationAsset* AnimationAssetManager::LoadAnimation(const std::string& animationPath, const std::string& animationName) {
   auto it = animationAssets_.find(animationName);
   if (it != animationAssets_.end()) {
      return it->second.get();
   }

   auto animationAsset = std::make_unique<AnimationAsset>();
   animationAsset->LoadFile(animationPath, animationName);

   AnimationAsset* animationAssetPtr = animationAsset.get();
   animationAssets_[animationName] = std::move(animationAsset);
   return animationAssetPtr;
}

AnimationAsset* AnimationAssetManager::GetAnimation(const std::string& animationName) {
   auto it = animationAssets_.find(animationName);
   if (it == animationAssets_.end()) {
      return nullptr;
   }
   return it->second.get();
}

void AnimationAssetManager::Clear() {
   animationAssets_.clear();
}

std::vector<std::string> AnimationAssetManager::GetAnimationNames() const {
   std::vector<std::string> names;
   names.reserve(animationAssets_.size());
   for (const auto& [name, animationAsset] : animationAssets_) {
      (void)animationAsset;
      names.push_back(name);
   }
   std::sort(names.begin(), names.end());
   return names;
}

}
