#pragma once

#pragma once

#include "IObjectComponent.h"
#include "MathUtils.h"
#include "AnimationAsset.h"
#include <string>
#include <memory>

namespace GameEngine {

class AnimationComponent final : public IObjectComponent {
public:
   const char* GetTypeName() const override;

   nlohmann::json Serialize() const override;

   void Deserialize(const nlohmann::json& data) override;

   void Update(Object& owner, float deltaTime) override;

   std::string animationName;
   std::string clipName;
   std::string targetNodeName;
   float currentTime = 0.0f;
   float playbackSpeed = 1.0f;
   bool loop = true;
   bool playing = true;
   bool applyTranslation = true;
   bool applyRotation = true;
   bool applyScale = true;
   bool useSkinning = true;

private:
   Vector3 QuaternionToEuler_(const Quaternion& q) const;

   std::shared_ptr<AnimationAsset> cachedAnimationAsset_;
   std::string cachedAnimationName_;
   Animator animator_;
};

}
