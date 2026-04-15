#include "pch.h"
#include "MaterialComponent.h"

#include <algorithm>

#ifdef USE_IMGUI
#include "Object.h"
#include "externals/imgui/imgui.h"
#endif

namespace GameEngine {

MaterialComponent::MaterialResolver MaterialComponent::resolver_ = nullptr;
MaterialComponent::MaterialCreator MaterialComponent::creator_ = nullptr;
MaterialComponent::MaterialNamesProvider MaterialComponent::namesProvider_ = nullptr;

void MaterialComponent::SetMaterialResolver(MaterialResolver resolver) {
   resolver_ = std::move(resolver);
}

void MaterialComponent::SetMaterialCreator(MaterialCreator creator) {
   creator_ = std::move(creator);
}

void MaterialComponent::SetMaterialNamesProvider(MaterialNamesProvider provider) {
   namesProvider_ = std::move(provider);
}

Material* MaterialComponent::EnsureMaterial(const std::string& name, uint32_t color, int32_t lightingMode, const Matrix4x4& uvTransform) {
   if (name.empty()) {
      return nullptr;
   }

   Material* material = nullptr;
   if (resolver_) {
      material = resolver_(name);
   }

   if (!material && creator_) {
      material = creator_(name, color, lightingMode, uvTransform);
   }

   if (material) {
      AssignMaterial(material, name);
   }

   return material;
}

void MaterialComponent::AssignMaterial(Material* material, const std::string& materialName) {
   materials.clear();
   materialNames_.clear();

   if (!material) {
      return;
   }

   materials.push_back(material);
   materialNames_.push_back(materialName);
}

void MaterialComponent::AppendMaterial(Material* material, const std::string& materialName) {
   if (!material) {
      return;
   }

   materials.push_back(material);
   materialNames_.push_back(materialName);
   SyncMaterialNamesSize();
}

void MaterialComponent::AssignMaterials(const std::vector<Material*>& newMaterials, const std::vector<std::string>& materialNames) {
   materials = newMaterials;
   materialNames_ = materialNames;
   SyncMaterialNamesSize();
}

void MaterialComponent::SyncMaterialNamesSize() {
   if (materialNames_.size() < materials.size()) {
      materialNames_.resize(materials.size());
   } else if (materialNames_.size() > materials.size()) {
      materialNames_.resize(materials.size());
   }
}

const char* MaterialComponent::GetTypeName() const {
   return "MaterialComponent";
}

nlohmann::json MaterialComponent::Serialize() const {
   nlohmann::json json = nlohmann::json::object();
   std::vector<std::string> materialNames = materialNames_;
   if (materialNames.size() < materials.size()) {
      materialNames.resize(materials.size());
   } else if (materialNames.size() > materials.size()) {
      materialNames.resize(materials.size());
   }
   json["materialNames"] = materialNames;
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

void MaterialComponent::Deserialize(const nlohmann::json& data) {
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

#ifdef USE_IMGUI
void MaterialComponent::DrawInspector(Object& owner) {
   (void)owner;

   SyncMaterialNamesSize();

   if (!ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
      return;
   }

   if (materials.empty() || !materials[0] || !materials[0]->GetMaterialData()) {
      ImGui::Text("No material");
      if (namesProvider_ && resolver_) {
         const auto names = namesProvider_();
         if (!names.empty()) {
            static int selectedIndex = 0;
            selectedIndex = std::clamp(selectedIndex, 0, static_cast<int>(names.size() - 1));
            if (ImGui::BeginCombo("Material Asset", names[selectedIndex].c_str())) {
               for (size_t i = 0; i < names.size(); ++i) {
                  const bool selected = static_cast<int>(i) == selectedIndex;
                  if (ImGui::Selectable(names[i].c_str(), selected)) {
                     selectedIndex = static_cast<int>(i);
                  }
                  if (selected) {
                     ImGui::SetItemDefaultFocus();
                  }
               }
               ImGui::EndCombo();
            }

            if (ImGui::Button("Assign Material Asset")) {
               if (auto* selectedMaterial = resolver_(names[selectedIndex])) {
                  AssignMaterial(selectedMaterial, names[selectedIndex]);
               }
            }
         }
      }
      return;
   }

   if (namesProvider_ && resolver_) {
      const auto names = namesProvider_();
      if (!names.empty()) {
         int selectedIndex = 0;
         const std::string currentName = materialNames_.empty() ? std::string() : materialNames_[0];
         for (size_t i = 0; i < names.size(); ++i) {
            if (names[i] == currentName) {
               selectedIndex = static_cast<int>(i);
               break;
            }
         }

         const char* preview = currentName.empty() ? "<default>" : currentName.c_str();
         if (ImGui::BeginCombo("Material Asset", preview)) {
            for (size_t i = 0; i < names.size(); ++i) {
               const bool selected = static_cast<int>(i) == selectedIndex;
               if (ImGui::Selectable(names[i].c_str(), selected)) {
                  if (auto* selectedMaterial = resolver_(names[i])) {
                     AssignMaterial(selectedMaterial, names[i]);
                  }
               }
               if (selected) {
                  ImGui::SetItemDefaultFocus();
               }
            }
            ImGui::EndCombo();
         }
      }
   }

   auto* material = materials[0];
   auto* data = material->GetMaterialData();
   Vector4 color = data->color;
   if (ImGui::ColorEdit4("Color", &color.x)) {
      material->SetColor(color);
   }

   const char* lightingModeLabels[] = { "None", "Lambert", "HalfLambert", "Phong", "BlinnPhong" };
   int lightingMode = std::clamp(data->lightingMode, 0, 4);
   if (ImGui::BeginCombo("Lighting Mode", lightingModeLabels[lightingMode])) {
      for (int i = 0; i < 5; ++i) {
         const bool selected = (i == lightingMode);
         if (ImGui::Selectable(lightingModeLabels[i], selected)) {
            material->SetLightingMode(static_cast<Material::LightingMode>(i));
         }
         if (selected) {
            ImGui::SetItemDefaultFocus();
         }
      }
      ImGui::EndCombo();
   }

   float shininess = data->shininess;
   if (ImGui::DragFloat("Shininess", &shininess, 0.1f, 0.0f, 256.0f)) {
      material->SetShininess(shininess);
   }

   Vector2 uvScale = material->GetUVScale();
   float uvRotation = material->GetUVRotation();
   Vector2 uvTranslation = material->GetUVTranslation();
   bool changed = false;
   changed |= ImGui::DragFloat2("UV Scale", &uvScale.x, 0.01f);
   changed |= ImGui::DragFloat("UV Rotation", &uvRotation, 0.01f);
   changed |= ImGui::DragFloat2("UV Translation", &uvTranslation.x, 0.01f);
   if (changed) {
      material->SetUVTransform(uvScale, uvRotation, uvTranslation);
   }

   ImGui::Spacing();
}
#endif

}
