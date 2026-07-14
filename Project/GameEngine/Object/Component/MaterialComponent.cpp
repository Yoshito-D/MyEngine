#include "pch.h"
#include "MaterialComponent.h"
#include "ComponentRegistry.h"
#include "Graphics/Texture.h"
#include "Object.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace {
   const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
      GameEngine::MaterialComponent::kTypeName,
      [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::MaterialComponent>(); },
      GameEngine::MaterialComponent::kDisplayName
   );

   bool ReadVector2(const nlohmann::json& data, const char* key, GameEngine::Vector2& out) {
      if (!data.contains(key) || !data.at(key).is_array() || data.at(key).size() != 2) {
         return false;
      }

      out.x = data.at(key)[0].get<float>();
      out.y = data.at(key)[1].get<float>();
      return true;
   }

   bool IsTextureFileExtension(std::string extension) {
      std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
         return static_cast<char>(std::tolower(c));
      });
      return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".dds";
   }

   std::string MakeSerializedTextureName(const std::string& textureId) {
      if (textureId.empty()) {
         return {};
      }

      const std::filesystem::path texturePath(textureId);
      if (!texturePath.has_parent_path() && !IsTextureFileExtension(texturePath.extension().string())) {
         return textureId;
      }

      // Editor asset IDs are resource-relative paths; material save data keeps only the texture name.
      const std::string textureName = texturePath.stem().string();
      return textureName.empty() ? textureId : textureName;
   }
}

#ifdef USE_IMGUI
#include "Object.h"
#include "imgui.h"
#include "Graphics/Texture.h"
#include "Utility/ImGuiHelper.h"
#include <cstring>
#endif

namespace GameEngine {

MaterialComponent::MaterialResolver MaterialComponent::resolver_ = nullptr;
MaterialComponent::MaterialCreator MaterialComponent::creator_ = nullptr;
MaterialComponent::MaterialNamesProvider MaterialComponent::namesProvider_ = nullptr;
MaterialComponent::TextureResolver MaterialComponent::textureResolver_ = nullptr;
MaterialComponent::TextureNamesProvider MaterialComponent::textureNamesProvider_ = nullptr;
MaterialComponent::EnvironmentTextureResolver MaterialComponent::environmentTextureResolver_ = nullptr;
MaterialComponent::EnvironmentTextureNamesProvider MaterialComponent::environmentTextureNamesProvider_ = nullptr;

void MaterialComponent::SetMaterialResolver(MaterialResolver resolver) {
   resolver_ = std::move(resolver);
}

void MaterialComponent::SetMaterialCreator(MaterialCreator creator) {
   creator_ = std::move(creator);
}

void MaterialComponent::SetMaterialNamesProvider(MaterialNamesProvider provider) {
   namesProvider_ = std::move(provider);
}

void MaterialComponent::SetTextureResolver(TextureResolver resolver) {
   textureResolver_ = std::move(resolver);
}

void MaterialComponent::SetTextureNamesProvider(TextureNamesProvider provider) {
   textureNamesProvider_ = std::move(provider);
}

void MaterialComponent::SetEnvironmentTextureResolver(EnvironmentTextureResolver resolver) {
   environmentTextureResolver_ = std::move(resolver);
}

void MaterialComponent::SetEnvironmentTextureNamesProvider(EnvironmentTextureNamesProvider provider) {
   environmentTextureNamesProvider_ = std::move(provider);
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

   std::string resolvedName = materialName;
   if (resolvedName.empty() && namesProvider_ && resolver_) {
      const auto names = namesProvider_();
      for (const auto& name : names) {
         if (auto* candidate = resolver_(name); candidate == material) {
            resolvedName = name;
            break;
         }
      }
   }

   materials.push_back(material);
   materialNames_.push_back(resolvedName);
}

void MaterialComponent::AppendMaterial(Material* material, const std::string& materialName) {
   if (!material) {
      return;
   }

   std::string resolvedName = materialName;
   if (resolvedName.empty() && namesProvider_ && resolver_) {
      const auto names = namesProvider_();
      for (const auto& name : names) {
         if (auto* candidate = resolver_(name); candidate == material) {
            resolvedName = name;
            break;
         }
      }
   }

   materials.push_back(material);
   materialNames_.push_back(resolvedName);
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
   if (textureNames_.size() < materials.size()) {
      textureNames_.resize(materials.size());
   } else if (textureNames_.size() > materials.size()) {
      textureNames_.resize(materials.size());
   }
}

Texture* MaterialComponent::GetTexture(size_t index) const {
   if (!textureResolver_) return nullptr;
   if (index >= textureNames_.size()) return nullptr;
   const auto& name = textureNames_[index];
   if (name.empty()) return nullptr;
   return textureResolver_(name);
}

const std::string& MaterialComponent::GetTextureName(size_t index) const {
   static const std::string kEmpty;
   if (index >= textureNames_.size()) return kEmpty;
   return textureNames_[index];
}

void MaterialComponent::SetTextureName(size_t slot, const std::string& name) {
   if (!name.empty() && textureResolver_) {
      if (Texture* texture = textureResolver_(name); texture && texture->GetMetadata().IsCubemap()) {
         return;
      }
   }

   if (textureNames_.size() <= slot) {
      textureNames_.resize(slot + 1);
   }
   textureNames_[slot] = name;
}

void MaterialComponent::SetTextureUV(const Vector2& leftTop, const Vector2& size) {
   textureLeftTop_ = leftTop;
   textureSize_ = size;
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
   json["environmentTextureName"] = MakeSerializedTextureName(environmentTextureName_);
   json["textureLeftTop"] = { textureLeftTop_.x, textureLeftTop_.y };
   json["textureSize"] = { textureSize_.x, textureSize_.y };

   // テクスチャ名（マテリアルスロット並行）
   {
      std::vector<std::string> texNames = textureNames_;
      texNames.resize(materials.size());
      for (auto& textureName : texNames) {
         textureName = MakeSerializedTextureName(textureName);
      }
      json["textureNames"] = texNames;
   }

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
      json["rimLightColor"] = {
         materialData->rimLightColor.x,
         materialData->rimLightColor.y,
         materialData->rimLightColor.z,
         materialData->rimLightColor.w
      };
      json["rimLightIntensity"] = materialData->rimLightIntensity;
      json["rimLightPower"] = materialData->rimLightPower;
      json["fillLightColor"] = {
         materialData->fillLightColor.x,
         materialData->fillLightColor.y,
         materialData->fillLightColor.z,
         materialData->fillLightColor.w
      };
      json["fillLightIntensity"] = materialData->fillLightIntensity;

      const Matrix4x4 uv = materialData->uvTransform;
      json["uvTransform"] = {
         { uv.m[0][0], uv.m[0][1], uv.m[0][2], uv.m[0][3] },
         { uv.m[1][0], uv.m[1][1], uv.m[1][2], uv.m[1][3] },
         { uv.m[2][0], uv.m[2][1], uv.m[2][2], uv.m[2][3] },
         { uv.m[3][0], uv.m[3][1], uv.m[3][2], uv.m[3][3] }
      };

      // blendMode（nullopt の場合は -1 として保存）
      const auto blendMode = materials[0]->GetBlendMode();
      json["blendMode"] = blendMode.has_value() ? static_cast<int>(blendMode.value()) : -1;

      // pipelineName
      json["pipelineName"] = materials[0]->GetPipelineName();
   }
   return json;
}

void MaterialComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }

   Vector2 textureValue = textureLeftTop_;
   if (ReadVector2(data, "textureLeftTop", textureValue)) {
      textureLeftTop_ = textureValue;
   }

   textureValue = textureSize_;
   if (ReadVector2(data, "textureSize", textureValue)) {
      textureSize_ = textureValue;
   }

   if (data.contains("environmentTextureName") && data.at("environmentTextureName").is_string()) {
      environmentTextureName_ = data.at("environmentTextureName").get<std::string>();
   }

   if (!data.contains("materialNames") || !data.at("materialNames").is_array()) {
      return;
   }

   materialNames_.clear();
   materials.clear();
   textureNames_.clear();

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

   // テクスチャ名の復元
   if (data.contains("textureNames") && data.at("textureNames").is_array()) {
      for (const auto& tv : data.at("textureNames")) {
         textureNames_.push_back(tv.is_string() ? tv.get<std::string>() : std::string{});
      }
   }
   textureNames_.resize(materials.size());

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

      if (data.contains("rimLightColor") && data.at("rimLightColor").is_array() && data.at("rimLightColor").size() == 4) {
         material->SetRimLightColor(Vector4(
            data.at("rimLightColor")[0].get<float>(),
            data.at("rimLightColor")[1].get<float>(),
            data.at("rimLightColor")[2].get<float>(),
            data.at("rimLightColor")[3].get<float>()
         ));
      }

      if (data.contains("rimLightIntensity") && data.at("rimLightIntensity").is_number()) {
         material->SetRimLightIntensity(data.at("rimLightIntensity").get<float>());
      }

      if (data.contains("rimLightPower") && data.at("rimLightPower").is_number()) {
         material->SetRimLightPower(data.at("rimLightPower").get<float>());
      }

      if (data.contains("fillLightColor") && data.at("fillLightColor").is_array() && data.at("fillLightColor").size() == 4) {
         material->SetFillLightColor(Vector4(
            data.at("fillLightColor")[0].get<float>(),
            data.at("fillLightColor")[1].get<float>(),
            data.at("fillLightColor")[2].get<float>(),
            data.at("fillLightColor")[3].get<float>()
         ));
      }

      if (data.contains("fillLightIntensity") && data.at("fillLightIntensity").is_number()) {
         material->SetFillLightIntensity(data.at("fillLightIntensity").get<float>());
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

      if (data.contains("blendMode") && data.at("blendMode").is_number_integer()) {
         const int val = data.at("blendMode").get<int>();
         if (val >= 0 && val < static_cast<int>(BlendMode::kCount)) {
            material->SetBlendMode(static_cast<BlendMode>(val));
         } else {
            material->SetBlendMode(std::nullopt);
         }
      }

      if (data.contains("pipelineName") && data.at("pipelineName").is_string()) {
         material->SetPipelineName(data.at("pipelineName").get<std::string>());
      }
   }
}

#ifdef USE_IMGUI
void MaterialComponent::DrawInspector() {
   auto Tr = [](const char* japanese, const char* english) {
      return ImGuiHelper::Localize({ japanese, english });
   };

   SyncMaterialNamesSize();

   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }

   if (materials.empty() || !materials[0] || !materials[0]->GetMaterialData()) {
      ImGui::Text("%s", Tr("マテリアルなし", "No material"));
      if (namesProvider_ && resolver_) {
         const auto names = namesProvider_();
         if (!names.empty()) {
            static int selectedIndex = 0;
            selectedIndex = std::clamp(selectedIndex, 0, static_cast<int>(names.size() - 1));
            if (ImGui::BeginCombo(Tr("マテリアルアセット", "Material Asset"), names[selectedIndex].c_str())) {
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

            if (ImGui::Button(Tr("マテリアルアセットを割り当て", "Assign Material Asset"))) {
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

         const char* preview = currentName.empty() ? Tr("<デフォルト>", "<default>") : currentName.c_str();
         if (ImGui::BeginCombo(Tr("マテリアルアセット", "Material Asset"), preview)) {
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

   // テクスチャスロット（マテリアルスロットごとに表示）
   if (textureNamesProvider_) {
      const auto texNames = textureNamesProvider_();
      if (!texNames.empty()) {
         SyncMaterialNamesSize();
         ImGui::SeparatorText(Tr("テクスチャ", "Textures"));
         for (size_t slot = 0; slot < materials.size(); ++slot) {
            ImGui::PushID(static_cast<int>(slot));

            const std::string currentTexName = slot < textureNames_.size() ? textureNames_[slot] : std::string();
            int texSelectedIndex = 0;
            for (size_t i = 0; i < texNames.size(); ++i) {
               if (texNames[i] == currentTexName) {
                  texSelectedIndex = static_cast<int>(i);
                  break;
               }
            }

            const std::string label = std::string(Tr("スロット", "Slot")) + " " + std::to_string(slot);
            const char* texPreview = currentTexName.empty() ? Tr("<なし>", "<none>") : currentTexName.c_str();
            if (ImGui::BeginCombo(label.c_str(), texPreview)) {
               if (ImGui::Selectable(Tr("<なし>", "<none>"), currentTexName.empty())) {
                  if (textureNames_.size() <= slot) textureNames_.resize(slot + 1);
                  textureNames_[slot].clear();
               }
               for (size_t i = 0; i < texNames.size(); ++i) {
                  if (textureResolver_) {
                     if (Texture* candidate = textureResolver_(texNames[i]); candidate && candidate->GetMetadata().IsCubemap()) {
                        continue;
                     }
                  }
                  const bool sel = (static_cast<int>(i) == texSelectedIndex && !currentTexName.empty());
                  if (ImGui::Selectable(texNames[i].c_str(), sel)) {
                     if (textureNames_.size() <= slot) textureNames_.resize(slot + 1);
                     textureNames_[slot] = texNames[i];
                  }
                  if (sel) { ImGui::SetItemDefaultFocus(); }
               }
               ImGui::EndCombo();
            }

            // テクスチャプレビュー
            if (textureResolver_ && !currentTexName.empty()) {
               if (Texture* tex = textureResolver_(currentTexName)) {
                  if (tex->GetMetadata().IsCubemap()) {
                     ImGui::TextDisabled("%s", Tr("TextureCube は 2D スロットではプレビューできません", "TextureCube cannot be previewed in a 2D slot"));
                     ImGui::PopID();
                     continue;
                  }
                  const float maxSize = 96.0f;
                  const float w = static_cast<float>(tex->GetWidth());
                  const float h = static_cast<float>(tex->GetHeight());
                  const float scale = (w > h) ? (maxSize / w) : (maxSize / h);
                  const ImVec2 displaySize(w * scale, h * scale);
                  ImU64 texId{};
                  const UINT64 gpuPtr = tex->GetTextureSrvHandleGPU().ptr;
                  static_assert(sizeof(texId) == sizeof(gpuPtr), "ImTextureID size mismatch");
                  std::memcpy(&texId, &gpuPtr, sizeof(texId));
                  ImGui::Image(ImTextureRef(texId), displaySize);
                  ImGui::SameLine();
                  ImGui::TextDisabled("%ux%u", tex->GetWidth(), tex->GetHeight());
               }
            }

            ImGui::PopID();
         }
      }
   }

   ImGui::SeparatorText(Tr("テクスチャ矩形", "Texture Rect"));
   ImGui::DragFloat2(Tr("左上", "Left Top"), &textureLeftTop_.x, 0.1f, 0.0f, 16384.0f);
   ImGui::DragFloat2(Tr("サイズ", "Size"), &textureSize_.x, 0.1f, 0.0f, 16384.0f);

   Vector4 color = data->color;
   if (ImGui::ColorEdit4(Tr("色", "Color"), &color.x)) {
      material->SetColor(color);
   }

   const char* lightingModeLabels[] = { Tr("なし", "None"), "Lambert", "HalfLambert", "Phong", "BlinnPhong" };
   int lightingMode = std::clamp(data->lightingMode, 0, 4);
   if (ImGui::BeginCombo(Tr("ライティングモード", "Lighting Mode"), lightingModeLabels[lightingMode])) {
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
   if (ImGui::DragFloat(Tr("光沢", "Shininess"), &shininess, 0.1f, 0.0f, 256.0f)) {
      material->SetShininess(shininess);
   }

   ImGui::SeparatorText(Tr("補助光", "Supplemental Lighting"));

   Vector4 rimLightColor = data->rimLightColor;
   if (ImGui::ColorEdit3(Tr("リムライト色", "Rim Light Color"), &rimLightColor.x)) {
      material->SetRimLightColor(rimLightColor);
   }

   float rimLightIntensity = data->rimLightIntensity;
   if (ImGui::DragFloat(Tr("リムライト強度", "Rim Light Intensity"), &rimLightIntensity, 0.01f, 0.0f, 10.0f)) {
      material->SetRimLightIntensity(rimLightIntensity);
   }

   float rimLightPower = data->rimLightPower;
   if (ImGui::DragFloat(Tr("リムライト減衰", "Rim Light Power"), &rimLightPower, 0.05f, 0.01f, 32.0f)) {
      material->SetRimLightPower(rimLightPower);
   }

   Vector4 fillLightColor = data->fillLightColor;
   if (ImGui::ColorEdit3(Tr("フィルライト色", "Fill Light Color"), &fillLightColor.x)) {
      material->SetFillLightColor(fillLightColor);
   }

   float fillLightIntensity = data->fillLightIntensity;
   if (ImGui::DragFloat(Tr("フィルライト強度", "Fill Light Intensity"), &fillLightIntensity, 0.01f, 0.0f, 10.0f)) {
      material->SetFillLightIntensity(fillLightIntensity);
   }

   // ブレンドモード
   {
      const char* blendModeLabels[] = {
         Tr("なし", "None"),
         Tr("通常", "Normal"),
         Tr("加算", "Add"),
         Tr("減算", "Subtract"),
         Tr("乗算", "Multiply"),
         Tr("スクリーン", "Screen")
      };
      const auto currentBlend = material->GetBlendMode();
      int blendIndex = currentBlend.has_value() ? static_cast<int>(currentBlend.value()) : -1;
      const char* blendPreview = (blendIndex >= 0 && blendIndex < 6) ? blendModeLabels[blendIndex] : Tr("<デフォルト>", "<default>");
      if (ImGui::BeginCombo(Tr("ブレンドモード", "Blend Mode"), blendPreview)) {
         if (ImGui::Selectable(Tr("<デフォルト>", "<default>"), blendIndex < 0)) {
            material->SetBlendMode(std::nullopt);
         }
         for (int i = 0; i < 6; ++i) {
            const bool sel = (i == blendIndex);
            if (ImGui::Selectable(blendModeLabels[i], sel)) {
               material->SetBlendMode(static_cast<BlendMode>(i));
            }
            if (sel) { ImGui::SetItemDefaultFocus(); }
         }
         ImGui::EndCombo();
      }
   }

   // パイプライン名
   {
      std::string pipelineName = material->GetPipelineName();
      char buf[128] = {};
      pipelineName.copy(buf, sizeof(buf) - 1);
      if (ImGui::InputText(Tr("パイプライン名", "Pipeline Name"), buf, sizeof(buf))) {
         material->SetPipelineName(buf);
      }
   }

   Vector2 uvScale = material->GetUVScale();
   float uvRotation = material->GetUVRotation();
   float uvRotationDegrees = ImGuiHelper::RadiansToDegrees(uvRotation);
   Vector2 uvTranslation = material->GetUVTranslation();
   bool changed = false;
   changed |= ImGui::DragFloat2(Tr("UVスケール", "UV Scale"), &uvScale.x, 0.01f);
   if (ImGui::DragFloat(Tr("UV回転 (deg)", "UV Rotation (deg)"), &uvRotationDegrees, 0.1f)) {
      uvRotation = ImGuiHelper::DegreesToRadians(uvRotationDegrees);
      changed = true;
   }
   changed |= ImGui::DragFloat2(Tr("UV移動", "UV Translation"), &uvTranslation.x, 0.01f);
   if (changed) {
      material->SetUVTransform(uvScale, uvRotation, uvTranslation);
   }

   ImGui::Spacing();
   ImGui::SeparatorText(Tr("環境テクスチャ", "Environment Texture"));

   if (environmentTextureNamesProvider_) {
      const auto texNames = environmentTextureNamesProvider_();
      if (!texNames.empty()) {
         int envSelectedIndex = 0;
         for (size_t i = 0; i < texNames.size(); ++i) {
            if (texNames[i] == environmentTextureName_) {
               envSelectedIndex = static_cast<int>(i);
               break;
            }
         }
         const char* envPreview = environmentTextureName_.empty() ? Tr("<なし>", "<none>") : environmentTextureName_.c_str();
         if (ImGui::BeginCombo(Tr("環境テクスチャ", "Environment Texture"), envPreview)) {
            if (ImGui::Selectable(Tr("<なし>", "<none>"), environmentTextureName_.empty())) {
               environmentTextureName_.clear();
            }
            for (size_t i = 0; i < texNames.size(); ++i) {
               const bool sel = (static_cast<int>(i) == envSelectedIndex && !environmentTextureName_.empty());
               if (ImGui::Selectable(texNames[i].c_str(), sel)) {
                  environmentTextureName_ = texNames[i];
               }
               if (sel) {
                  ImGui::SetItemDefaultFocus();
               }
            }
            ImGui::EndCombo();
         }

         if (!environmentTextureName_.empty() && environmentTextureResolver_) {
            if (auto* envTex = environmentTextureResolver_(environmentTextureName_)) {
               const auto& meta = envTex->GetMetadata();
               ImGui::Indent();
               ImGui::TextDisabled("%s  %ux%u  mips:%zu",
                  Tr("キューブマップ", "Cubemap"),
                  envTex->GetWidth(), envTex->GetHeight(), meta.mipLevels);
               ImGui::Unindent();
            }
         }
      }
   }

   float environmentCoefficient = data->environmentCoefficient;
   if (ImGui::DragFloat(Tr("環境反射係数", "Environment Coefficient"), &environmentCoefficient, 0.01f, 0.0f, 1.0f)) {
      material->SetEnvironmentTextureStrength(environmentCoefficient);
   }

   ImGui::Spacing();
}
#endif

}
