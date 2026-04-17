#pragma once

#include "IObjectComponent.h"
#include "MathUtils.h"
#include "Utility/VectorMath.h"
#include <string>

namespace GameEngine {
class TransformComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "TransformComponent";
   const char* GetTypeName() const override;

   nlohmann::json Serialize() const override;

   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   Transform transform = {};
   Matrix4x4 parentMatrix = MakeIdentity4x4();
   bool useParentMatrix = false;
   std::string parentObjectName;
};
}
