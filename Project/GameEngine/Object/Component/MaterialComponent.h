#pragma once
#include "IObjectComponent.h"
#include "Material.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace GameEngine {
class Material;

class MaterialComponent final : public IObjectComponent {
public:
   using MaterialResolver = std::function<Material*(const std::string&)>;

   static void SetMaterialResolver(MaterialResolver resolver);

   const char* GetTypeName() const override;

   nlohmann::json Serialize() const override;

   void Deserialize(const nlohmann::json& data) override;

   std::vector<Material*> materials;

private:
   static MaterialResolver resolver_;
   std::vector<std::string> materialNames_;
};
}
