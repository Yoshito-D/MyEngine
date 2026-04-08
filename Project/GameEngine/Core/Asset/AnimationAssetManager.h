#pragma once

#pragma once

#include "AnimationAsset.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace GameEngine {

class AnimationAssetManager {
public:
   AnimationAsset* LoadAnimation(const std::string& animationPath, const std::string& animationName);

   AnimationAsset* GetAnimation(const std::string& animationName);

   void Clear();

   std::vector<std::string> GetAnimationNames() const;

private:
   std::unordered_map<std::string, std::unique_ptr<AnimationAsset>> animationAssets_;
};

}
