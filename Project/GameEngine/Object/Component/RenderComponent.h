#pragma once

#include "IObjectComponent.h"

namespace GameEngine {
class RenderComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "RenderComponent";
   const char* GetTypeName() const override;

   nlohmann::json Serialize() const override;

   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   bool visible = true;
   bool autoRender = true;
   bool applyPostProcess = true;
};
}
