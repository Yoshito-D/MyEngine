#pragma once

#include "IObjectComponent.h"
#include <string>

namespace GameEngine {
class ObjectNameComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "ObjectNameComponent";
   const char* GetTypeName() const override;

   nlohmann::json Serialize() const override;

   void Deserialize(const nlohmann::json& data) override;

   std::string name = "Object";
};
}
