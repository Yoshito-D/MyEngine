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

   static void SetMaterialResolver(MaterialResolver resolver) {
      resolver_ = std::move(resolver);
   }

   const char* GetTypeName() const override {
      return "MaterialComponent";
   }

   nlohmann::json Serialize() const override {
      nlohmann::json json = nlohmann::json::object();
      json["materialNames"] = materialNames_;
      json["materialCount"] = materials.size();

      if (!materials.empty() && materials[0] && materials[0]->GetMaterialData()) {
         const auto* materialData = materials[0]->GetMaterialData();
         json["color"] = {
            materialData->color.x,
            materialData->color.y,
            materialData->color.z,
            materialData->color.w
         };
         json["lightingMode"] = materialData->lightingMode;
         json["shininess"] = materialData->shininess;

         const Matrix4x4 uv = materialData->uvTransform;
         json["uvTransform"] = {
            { uv.m[0][0], uv.m[0][1], uv.m[0][2], uv.m[0][3] },
            { uv.m[1][0], uv.m[1][1], uv.m[1][2], uv.m[1][3] },
            { uv.m[2][0], uv.m[2][1], uv.m[2][2], uv.m[2][3] },
            { uv.m[3][0], uv.m[3][1], uv.m[3][2], uv.m[3][3] }
         };
      }
      return json;
   }

   void Deserialize(const nlohmann::json& data) override {
      if (!data.contains("materialNames") || !data.at("materialNames").is_array()) {
         return;
      }

      materialNames_.clear();
      materials.clear();

      for (const auto& nameValue : data.at("materialNames")) {
         if (!nameValue.is_string()) {
            continue;
         }

         const std::string name = nameValue.get<std::string>();
         materialNames_.push_back(name);

         if (resolver_) {
            auto* material = resolver_(name);
            if (material) {
               materials.push_back(material);
            }
         }

      if (!materials.empty() && materials[0] && materials[0]->GetMaterialData()) {
         auto* material = materials[0];

         if (data.contains("color") && data.at("color").is_array() && data.at("color").size() == 4) {
            material->SetColor(Vector4(
               data.at("color")[0].get<float>(),
               data.at("color")[1].get<float>(),
               data.at("color")[2].get<float>(),
               data.at("color")[3].get<float>()
            ));
         }

         if (data.contains("lightingMode") && data.at("lightingMode").is_number_integer()) {
            material->SetLightingMode(static_cast<Material::LightingMode>(data.at("lightingMode").get<int32_t>()));
         }

         if (data.contains("shininess") && data.at("shininess").is_number()) {
            material->SetShininess(data.at("shininess").get<float>());
         }

         if (data.contains("uvTransform") && data.at("uvTransform").is_array() && data.at("uvTransform").size() == 4) {
            Matrix4x4 uv = MakeIdentity4x4();
            bool valid = true;
            for (int row = 0; row < 4; ++row) {
               const auto& rowData = data.at("uvTransform")[row];
               if (!rowData.is_array() || rowData.size() != 4) {
                  valid = false;
                  break;
               }
               for (int col = 0; col < 4; ++col) {
                  uv.m[row][col] = rowData[col].get<float>();
               }
            }
            if (valid) {
               material->SetUVTransform(uv);
            }
         }
      }
      }
   }

   std::vector<Material*> materials;

private:
   inline static MaterialResolver resolver_ = nullptr;
   std::vector<std::string> materialNames_;
};
}
