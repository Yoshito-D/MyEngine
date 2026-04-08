#pragma once

#pragma once

#include "IObjectComponent.h"
#include "MathUtils.h"
#include <string>

namespace GameEngine {

class AnimationComponent final : public IObjectComponent {
public:
   const char* GetTypeName() const override;

   nlohmann::json Serialize() const override;

   void Deserialize(const nlohmann::json& data) override;

   void Update(Object& owner, float deltaTime) override;

   std::string animationName;
   std::string targetNodeName;
   float currentTime = 0.0f;
   float playbackSpeed = 1.0f;
   bool loop = true;
   bool playing = true;
   bool applyTranslation = true;
   bool applyRotation = true;
   bool applyScale = true;

private:
   Vector3 QuaternionToEuler_(const Quaternion& q) const;
};

}
