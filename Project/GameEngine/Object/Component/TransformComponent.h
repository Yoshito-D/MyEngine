#pragma once
#pragma once

#include "IObjectComponent.h"
#include "MathUtils.h"
#include "Utility/VectorMath.h"
#include <string>

namespace GameEngine {
class TransformComponent final : public IObjectComponent {
public:
   const char* GetTypeName() const override;

   nlohmann::json Serialize() const override;

   void Deserialize(const nlohmann::json& data) override;

   Transform transform = {};
   Matrix4x4 parentMatrix = MakeIdentity4x4();
   bool useParentMatrix = false;
   std::string parentObjectName;
};
}
