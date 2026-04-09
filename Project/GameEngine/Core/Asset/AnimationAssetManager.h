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
   using AnimationHandle = std::shared_ptr<AnimationAsset>;

   AnimationHandle LoadAnimation(const std::string& animationPath, const std::string& animationName);

   AnimationHandle GetAnimation(const std::string& animationName);

   void Clear();

   std::vector<std::string> GetAnimationNames() const;

private:
   std::unordered_map<std::string, AnimationHandle> animationAssets_;
};

}
