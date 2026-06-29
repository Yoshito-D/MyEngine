#pragma once

#include "IObjectComponent.h"
#include <string>

namespace GameEngine {
class ObjectNameComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "ObjectNameComponent";
   static constexpr ComponentDisplayName kDisplayName{ "オブジェクト名", "Object Name" };
   const char* GetTypeName() const override;

   nlohmann::json Serialize() const override;

   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   std::string name = "Object";
};
}
