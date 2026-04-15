#pragma once

#pragma once

#include "IObjectComponent.h"
#include <string>

namespace GameEngine {
class RenderComponent final : public IObjectComponent {
public:
   const char* GetTypeName() const override;

   nlohmann::json Serialize() const override;

   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   void DrawInspector(Object& owner) override;
#endif

   bool visible = true;
   bool autoRender = true;
   bool applyPostProcess = true;
   std::string textureName = "white1x1";
};
}
