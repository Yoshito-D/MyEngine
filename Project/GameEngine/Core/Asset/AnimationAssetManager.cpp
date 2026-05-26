#include "pch.h"
#include "AnimationAssetManager.h"

namespace GameEngine {

AnimationAssetManager::AnimationHandle AnimationAssetManager::LoadAnimation(const std::string& animationPath, const std::string& animationName) {
   auto it = animationAssets_.find(animationName);
   if (it != animationAssets_.end()) {
      return it->second;
   }

   auto animationAsset = std::make_shared<AnimationAsset>();
   animationAsset->LoadFile(animationPath, animationName);

   animationAssets_[animationName] = std::move(animationAsset);
   return animationAssets_[animationName];
}

AnimationAssetManager::AnimationHandle AnimationAssetManager::GetAnimation(const std::string& animationName) {
   auto it = animationAssets_.find(animationName);
   if (it == animationAssets_.end()) {
      return {};
   }
   return it->second;
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
