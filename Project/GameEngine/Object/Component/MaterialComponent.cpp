#include "pch.h"
#include "MaterialComponent.h"
#include "ComponentRegistry.h"
#include "Graphics/Texture.h"
#include "Object.h"
#include "PSOManager.h"
#include "MeshComponent.h"
#include <cmath>

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace {
   const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
      GameEngine::MaterialComponent::kTypeName,
      [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::MaterialComponent>(); },
      GameEngine::MaterialComponent::kDisplayName,
      GameEngine::ObjectType::Model | GameEngine::ObjectType::Sprite
   );

   bool ReadVector2(const nlohmann::json& data, const char* key, GameEngine::Vector2& out) {
      if (!data.contains(key) || !data.at(key).is_array() || data.at(key).size() != 2) {
         return false;
      }

      out.x = data.at(key)[0].get<float>();
      out.y = data.at(key)[1].get<float>();
      return true;
   }

   bool ReadVector4(const nlohmann::json& data, const char* key, GameEngine::Vector4& out) {
      if (!data.contains(key) || !data.at(key).is_array() || data.at(key).size() != 4) {
         return false;
      }

      const auto& value = data.at(key);
      for (const auto& component : value) {
         if (!component.is_number()) {
            return false;
         }
      }

      out = GameEngine::Vector4(
         value[0].get<float>(),
         value[1].get<float>(),
         value[2].get<float>(),
         value[3].get<float>());
      return true;
   }

   bool ReadMatrix4x4(const nlohmann::json& data, const char* key, GameEngine::Matrix4x4& out) {
      if (!data.contains(key) || !data.at(key).is_array() || data.at(key).size() != 4) {
         return false;
      }

      // 作業行列を最後まで検証してからoutへ代入し、不正な一行でUV行列が部分更新されるのを防ぐ。
      GameEngine::Matrix4x4 value = GameEngine::MakeIdentity4x4();
      for (size_t row = 0; row < 4; ++row) {
         const auto& rowData = data.at(key)[row];
         if (!rowData.is_array() || rowData.size() != 4) {
            return false;
         }
         for (size_t column = 0; column < 4; ++column) {
            if (!rowData[column].is_number()) {
               return false;
            }
            value.m[row][column] = rowData[column].get<float>();
         }
      }

      out = value;
      return true;
   }

   nlohmann::json SerializeMaterialProperties(const GameEngine::Material* material) {
      // Material Managerが所有する実体から、シーンで上書き可能な描画パラメーターだけを抽出する。
      // GPUリソースやディスクリプタはAsset側で再構築するため保存しない。
      nlohmann::json json = nlohmann::json::object();
      if (!material || !material->GetMaterialData()) {
         return json;
      }

      const auto* materialData = material->GetMaterialData();
      json["color"] = {
         materialData->color.x,
         materialData->color.y,
         materialData->color.z,
         materialData->color.w
      };
      json["lightingMode"] = materialData->lightingMode;
      json["shininess"] = materialData->shininess;
      json["environmentTextureStrength"] = materialData->environmentCoefficient;
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

      const GameEngine::Matrix4x4 uv = materialData->uvTransform;
      json["uvTransform"] = {
         { uv.m[0][0], uv.m[0][1], uv.m[0][2], uv.m[0][3] },
         { uv.m[1][0], uv.m[1][1], uv.m[1][2], uv.m[1][3] },
         { uv.m[2][0], uv.m[2][1], uv.m[2][2], uv.m[2][3] },
         { uv.m[3][0], uv.m[3][1], uv.m[3][2], uv.m[3][3] }
      };

      // -1はパイプライン既定のBlendModeを使うという意味で、明示的なモードと区別する。
      const auto blendMode = material->GetBlendMode();
      json["blendMode"] = blendMode.has_value() ? static_cast<int>(blendMode.value()) : -1;
      json["pipelineName"] = material->GetPipelineName();
      json["parameters"] = material->GetParameters();
      return json;
   }

   void DeserializeMaterialProperties(GameEngine::Material* material, const nlohmann::json& data) {
      if (!material || !material->GetMaterialData() || !data.is_object()) {
         return;
      }

      // MaterialDataへ直接書かずsetterを通し、永続MapされたGPU定数との同期規則を一か所に保つ。
      GameEngine::Vector4 vectorValue{};
      if (ReadVector4(data, "color", vectorValue)) {
         material->SetColor(vectorValue);
      }

      if (data.contains("lightingMode") && data.at("lightingMode").is_number_integer()) {
         const int32_t lightingMode = data.at("lightingMode").get<int32_t>();
         if (lightingMode >= GameEngine::Material::LightingMode::NONE &&
            lightingMode <= GameEngine::Material::LightingMode::BLINNPHONG) {
            material->SetLightingMode(static_cast<GameEngine::Material::LightingMode>(lightingMode));
         }
      }

      if (data.contains("shininess") && data.at("shininess").is_number()) {
         material->SetShininess(data.at("shininess").get<float>());
      }

      if (data.contains("environmentTextureStrength") && data.at("environmentTextureStrength").is_number()) {
         material->SetEnvironmentTextureStrength(data.at("environmentTextureStrength").get<float>());
      }

      if (ReadVector4(data, "rimLightColor", vectorValue)) {
         material->SetRimLightColor(vectorValue);
      }
      if (data.contains("rimLightIntensity") && data.at("rimLightIntensity").is_number()) {
         material->SetRimLightIntensity(data.at("rimLightIntensity").get<float>());
      }
      if (data.contains("rimLightPower") && data.at("rimLightPower").is_number()) {
         material->SetRimLightPower(data.at("rimLightPower").get<float>());
      }

      if (ReadVector4(data, "fillLightColor", vectorValue)) {
         material->SetFillLightColor(vectorValue);
      }
      if (data.contains("fillLightIntensity") && data.at("fillLightIntensity").is_number()) {
         material->SetFillLightIntensity(data.at("fillLightIntensity").get<float>());
      }

      GameEngine::Matrix4x4 uvTransform = GameEngine::MakeIdentity4x4();
      if (ReadMatrix4x4(data, "uvTransform", uvTransform)) {
         material->SetUVTransform(uvTransform);
      }

      if (data.contains("blendMode") && data.at("blendMode").is_number_integer()) {
         const int blendMode = data.at("blendMode").get<int>();
         if (blendMode >= 0 && blendMode < static_cast<int>(BlendMode::kCount)) {
            material->SetBlendMode(static_cast<BlendMode>(blendMode));
         } else {
            material->SetBlendMode(std::nullopt);
         }
      }

      if (data.contains("pipelineName") && data.at("pipelineName").is_string()) {
         material->SetPipelineName(data.at("pipelineName").get<std::string>());
      }
      material->ClearParameters();
      if (data.contains("parameters") && data.at("parameters").is_object()) {
         for (const auto& [name, value] : data.at("parameters").items()) {
            try {
               if (!value.is_array() || !material->SetParameter(name, value.get<std::vector<float>>())) {
                  Logger::Warning("[MaterialComponent] Invalid parameter: " + name);
               }
            } catch (const std::exception& error) {
               Logger::Warning("[MaterialComponent] Parameter " + name + ": " + error.what());
            }
         }
      }
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

      // 既に論理名だけならそのまま保持し、Editorの相対パスIDだけを拡張子なしのRuntime名へ正規化する。
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
#include "Scene/BaseScene.h"
#include "EditorSceneContext.h"
#include "EditorCommand.h"
#include "Graphics/Texture.h"
#include "Utility/ImGuiHelper.h"
#include <cstring>
#endif

namespace GameEngine {

PSOManager* MaterialComponent::pipelineManager_ = nullptr;
MaterialComponent::MaterialResolver MaterialComponent::resolver_ = nullptr;
MaterialComponent::MaterialCreator MaterialComponent::creator_ = nullptr;
MaterialComponent::MaterialNamesProvider MaterialComponent::namesProvider_ = nullptr;
MaterialComponent::TextureResolver MaterialComponent::textureResolver_ = nullptr;
MaterialComponent::TextureNamesProvider MaterialComponent::textureNamesProvider_ = nullptr;
MaterialComponent::EnvironmentTextureResolver MaterialComponent::environmentTextureResolver_ = nullptr;
MaterialComponent::EnvironmentTextureNamesProvider MaterialComponent::environmentTextureNamesProvider_ = nullptr;

// Resolver群はAssetManagerへの依存を注入する境界である。ComponentをEditor/Runtimeの
// どちらでも同じ形式のまま使い、所有権を持たないMaterial/Textureを名前から再解決する。

void MaterialComponent::SetPipelineManager(PSOManager* manager) { pipelineManager_ = manager; }

Material* MaterialComponent::GetMaterial(size_t slot) const {
   return materials.empty() ? nullptr : (slot < materials.size() && materials[slot] ? materials[slot] : materials[0]);
}

bool MaterialComponent::IsOverridden(size_t slot) const {
   return slot < overrides_.size() && slot < materials.size() && overrides_[slot] && materials[slot] == overrides_[slot].get();
}

Material* MaterialComponent::EditMaterial(size_t slot) {
   if (slot >= 4096) return nullptr;
   if (IsOverridden(slot)) return materials[slot];
   Material* source = GetMaterial(slot);
   // Persist the inherited asset reference as well as its local values, so ResetOverride
   // returns to the same shared material after a scene reload.
   const std::string sourceName = slot < materials.size() && materials[slot] && slot < materialNames_.size()
      ? materialNames_[slot] : materialNames_.empty() ? std::string{} : materialNames_[0];
   Material* shared = source;
   for (size_t i = 0; i < overrides_.size(); ++i) {
      if (overrides_[i].get() == source && i < sharedMaterials_.size()) shared = sharedMaterials_[i];
   }
   if (materials.size() <= slot) materials.resize(slot + 1, nullptr);
   SyncMaterialNamesSize();
   overrides_.resize(materials.size());
   sharedMaterials_.resize(materials.size());
   materialNames_[slot] = sourceName;
   sharedMaterials_[slot] = shared;
   overrides_[slot] = source ? source->Clone() : std::make_unique<Material>();
   if (!source) overrides_[slot]->Create();
   materials[slot] = overrides_[slot].get();
   return materials[slot];
}

void MaterialComponent::ResetOverride(size_t slot) {
   if (!IsOverridden(slot)) return;
   materials[slot] = sharedMaterials_[slot];
   overrides_[slot].reset();
}

void MaterialComponent::SetSharedMaterial(size_t slot, Material* material, const std::string& name) {
   if (slot >= 4096) return;
   if (materials.size() <= slot) materials.resize(slot + 1, nullptr);
   overrides_.resize(materials.size());
   sharedMaterials_.resize(materials.size());
   overrides_[slot].reset();
   materials[slot] = material;
   sharedMaterials_[slot] = material;
   SyncMaterialNamesSize();
   materialNames_[slot] = name;
}

bool MaterialComponent::SetPipeline(const std::string& name, size_t slot) {
   if (!pipelineManager_ || !pipelineManager_->GetModelPipeline(name)) {
      Logger::Warning("[MaterialComponent] Unavailable model pipeline=" + name + ", slot=" + std::to_string(slot));
      return false;
   }
   if (auto* material = EditMaterial(slot)) { material->SetPipelineName(name); return true; }
   return false;
}

bool MaterialComponent::SetParameter(const std::string& name, const std::vector<float>& value, size_t slot) {
   Material* material = GetMaterial(slot);
   const auto* pipeline = pipelineManager_ && material ? pipelineManager_->GetModelPipeline(material->GetPipelineName()) : nullptr;
   if (pipeline) {
      const auto field = std::find_if(pipeline->parameters.begin(), pipeline->parameters.end(), [&](const auto& entry) { return entry.name == name; });
      if (field != pipeline->parameters.end() && field->defaultValue.size() == value.size() &&
         std::all_of(value.begin(), value.end(), [](float v) { return std::isfinite(v); })) {
         material = EditMaterial(slot);
         return material && material->SetParameter(name, value);
      }
   }
   Logger::Warning("[MaterialComponent] Invalid parameter=" + name + ", slot=" + std::to_string(slot));
   return false;
}

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

   // 共有アセットを先に検索し、存在しないシーン固有名だけを生成して重複Materialを避ける。
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
   overrides_.clear();
   sharedMaterials_.clear();
   materials.clear();
   materialNames_.clear();

   if (!material) {
      return;
   }

   // 呼び出し元が名前を持たない旧APIの場合だけ、Provider一覧を逆引きして保存可能な名前を補う。
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
   overrides_.clear();
   sharedMaterials_.clear();
   materials = newMaterials;
   materialNames_ = materialNames;
   SyncMaterialNamesSize();
}

void MaterialComponent::SyncMaterialNamesSize() {
   // Names follow explicit materials; textures may also override inherited submesh slots.
   if (materialNames_.size() < materials.size()) {
      materialNames_.resize(materials.size());
   } else if (materialNames_.size() > materials.size()) {
      materialNames_.resize(materials.size());
   }
   if (textureNames_.size() < materials.size()) {
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
   if (slot >= 4096) return;
   // 通常Texture2DスロットへCubemapを入れるとShaderのView次元と一致しないため、割り当てを拒否する。
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
   // 一時的にMaterial解決が失敗していても名前やTexture割り当てを失わないよう、
   // 3配列の最大長を保存スロット数として採用する。
   const size_t materialSlotCount = std::max({ materials.size(), materialNames_.size(), textureNames_.size() });
   std::vector<std::string> materialNames = materialNames_;
   materialNames.resize(materialSlotCount);
   json["materialNames"] = materialNames;
   json["materialCount"] = materialSlotCount;
   json["environmentTextureName"] = MakeSerializedTextureName(environmentTextureName_);
   json["textureLeftTop"] = { textureLeftTop_.x, textureLeftTop_.y };
   json["textureSize"] = { textureSize_.x, textureSize_.y };

   // テクスチャ名（マテリアルスロット並行）
   {
      std::vector<std::string> texNames = textureNames_;
      texNames.resize(materialSlotCount);
      for (auto& textureName : texNames) {
         textureName = MakeSerializedTextureName(textureName);
      }
      json["textureNames"] = texNames;
   }

   // 旧形式との互換性を保ちながら、複数スロットを欠落なく保存する。
   json["materialSlots"] = nlohmann::json::array();
   for (size_t slot = 0; slot < materialSlotCount; ++slot) {
      const Material* material = slot < materials.size() ? materials[slot] : nullptr;
      nlohmann::json slotData = SerializeMaterialProperties(material);
      slotData["ownership"] = IsOverridden(slot) ? "local" : "shared";
      slotData["name"] = materialNames[slot];
      slotData["textureName"] = slot < textureNames_.size()
         ? MakeSerializedTextureName(textureNames_[slot])
         : std::string{};
      json["materialSlots"].push_back(std::move(slotData));
   }

   // v1 Reader向けに第0スロットを直下にも複製する。現行ReaderはmaterialSlotsを優先する。
   if (!materials.empty()) {
      const nlohmann::json firstMaterial = SerializeMaterialProperties(materials[0]);
      for (auto it = firstMaterial.begin(); it != firstMaterial.end(); ++it) {
         json[it.key()] = it.value();
      }
   }
   return json;
}

void MaterialComponent::Deserialize(const nlohmann::json& data) try {
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

   // 名前解決不能な旧データでも既存描画を維持できるよう、現在のslot実体をfallbackとして退避する。
   std::vector<Material*> previousMaterials = materials;
   for (size_t i = 0; i < previousMaterials.size(); ++i) {
      if (IsOverridden(i)) previousMaterials[i] = sharedMaterials_[i];
   }
   auto previousOverrides = std::move(overrides_);
   const auto previousShared = sharedMaterials_;
   const auto previousEffective = materials;
   sharedMaterials_.clear();
   const std::vector<std::string> previousMaterialNames = materialNames_;
   const std::vector<std::string> previousTextureNames = textureNames_;
   auto resolveMaterial = [&previousMaterials](const std::string& name, size_t slot) -> Material* {
      Material* material = nullptr;
      if (!name.empty() && resolver_) {
         material = resolver_(name);
      }
      if (!material && !name.empty() && creator_) {
         // シーン固有マテリアルは旧シーンクラスに依存せず、保存データから再生成する。
         material = creator_(name, 0xffffffff, Material::LightingMode::HALFLAMBERT, MakeIdentity4x4());
      }
      if (!material && slot < previousMaterials.size()) {
         material = previousMaterials[slot];
      }
      return material;
   };

   materialNames_.clear();
   materials.clear();
   textureNames_.clear();

   // 現行形式はslotごとにMaterialプロパティとTexture名を完結して保持するため最優先で読む。
   if (data.contains("materialSlots") && data.at("materialSlots").is_array()) {
      const auto& materialSlots = data.at("materialSlots");
      materialNames_.reserve(std::min<size_t>(materialSlots.size(), 4096));
      materials.reserve(std::min<size_t>(materialSlots.size(), 4096));
      textureNames_.reserve(std::min<size_t>(materialSlots.size(), 4096));

      for (size_t slot = 0; slot < std::min<size_t>(materialSlots.size(), 4096); ++slot) {
         const auto& slotData = materialSlots[slot];
         if (!slotData.is_object()) {
            materialNames_.emplace_back();
            materials.push_back(slot < previousMaterials.size() ? previousMaterials[slot] : nullptr);
            textureNames_.emplace_back();
            continue;
         }

         const std::string materialName = slotData.contains("name") && slotData.at("name").is_string()
            ? slotData.at("name").get<std::string>()
            : std::string{};
         const std::string textureName = slotData.contains("textureName") && slotData.at("textureName").is_string()
            ? slotData.at("textureName").get<std::string>()
            : std::string{};
         // Old scenes become local snapshots so loading one object never overwrites another shared material.
         Material* material = resolveMaterial(materialName, slot);
         materialNames_.push_back(materialName);
         materials.push_back(material);
         textureNames_.push_back(textureName);
         const bool local = !slotData.contains("ownership") || slotData.at("ownership") != "shared";
         if (local) material = EditMaterial(slot);
         DeserializeMaterialProperties(material, slotData);
      }
      return;
   }

   // materialSlotsも旧materialNamesもない部分データでは、現在の割り当てを破棄しない。
   if (!data.contains("materialNames") || !data.at("materialNames").is_array()) {
      materials = previousEffective;
      overrides_ = std::move(previousOverrides);
      sharedMaterials_ = previousShared;
      materialNames_ = previousMaterialNames;
      textureNames_ = previousTextureNames;
      SyncMaterialNamesSize();
      return;
   }

   const auto& serializedNames = data.at("materialNames");
   // v1では空slotをmaterialCountだけで表せるため、名前配列と明示Countの大きい方を復元する。
   size_t materialSlotCount = serializedNames.size();
   if (data.contains("materialCount") && data.at("materialCount").is_number_unsigned()) {
      materialSlotCount = std::max(materialSlotCount, data.at("materialCount").get<size_t>());
   } else if (data.contains("materialCount") && data.at("materialCount").is_number_integer()) {
      const int64_t serializedSlotCount = data.at("materialCount").get<int64_t>();
      if (serializedSlotCount >= 0) {
         materialSlotCount = std::max(materialSlotCount, static_cast<size_t>(serializedSlotCount));
      }
   }

   materialSlotCount = std::min<size_t>(materialSlotCount, 4096);
   materialNames_.reserve(materialSlotCount);
   materials.reserve(materialSlotCount);
   for (size_t slot = 0; slot < materialSlotCount; ++slot) {
      const std::string materialName = slot < serializedNames.size() && serializedNames[slot].is_string()
         ? serializedNames[slot].get<std::string>()
         : std::string{};
      materialNames_.push_back(materialName);
      materials.push_back(resolveMaterial(materialName, slot));
   }

   if (data.contains("textureNames") && data.at("textureNames").is_array()) {
      for (const auto& textureName : data.at("textureNames")) {
         textureNames_.push_back(textureName.is_string() ? textureName.get<std::string>() : std::string{});
      }
   }
   textureNames_.resize(materialSlotCount);

   // v1 のシーンJSONは第0スロットの値をコンポーネント直下に保持している。
   if (!materials.empty()) {
      DeserializeMaterialProperties(EditMaterial(0), data);
   }
}

catch (const std::exception& error) {
   Logger::Warning("[MaterialComponent] Invalid scene data: " + std::string(error.what()));
}

#ifdef USE_IMGUI
void MaterialComponent::DrawInspector() {
   const auto before = Serialize();
   DrawInspectorContent();
   const auto after = Serialize();
   if (before != after && inspectorBefore_.is_null()) inspectorBefore_ = before;
   // A drag becomes one command when the active control is released.
   if (!inspectorBefore_.is_null() && !ImGui::IsAnyItemActive()) {
      if (HasOwner()) {
         auto* scene = BaseScene::GetCurrentScene();
         auto* context = scene ? scene->GetEditorSceneContext() : nullptr;
         if (context && inspectorBefore_ != after) {
            context->GetCommandStack().Execute(std::make_unique<SetMaterialSettingsCommand>(
               GetOwner().GetEntityId(), &GetOwner(), inspectorBefore_, after), *context);
         }
      }
      inspectorBefore_ = nullptr;
   }
}

void MaterialComponent::DrawInspectorContent() {
   auto Tr = [](const char* japanese, const char* english) {
      return ImGuiHelper::Localize({ japanese, english });
   };

   // 外部コードからMaterial配列だけ更新された場合も、UIを描く前に平行配列を整える。
   SyncMaterialNamesSize();

   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }

   size_t slotCount = std::max<size_t>(1, materials.size());
   if (HasOwner()) {
      if (auto* mesh = GetOwner().GetComponent<MeshComponent>(); mesh && mesh->GetModelAsset()) {
         slotCount = std::max(slotCount, mesh->GetModelAsset()->GetMeshData().size());
      }
   }
   inspectorSlot_ = std::min(inspectorSlot_, slotCount - 1);
   const std::string slotLabel = std::to_string(inspectorSlot_);
   if (ImGui::BeginCombo(Tr("マテリアルスロット", "Material Slot"), slotLabel.c_str())) {
      for (size_t slot = 0; slot < slotCount; ++slot) {
         const std::string label = std::to_string(slot);
         if (ImGui::Selectable(label.c_str(), slot == inspectorSlot_)) { inspectorSlot_ = slot; editShared_ = false; }
      }
      ImGui::EndCombo();
   }
   if (!GetMaterial(inspectorSlot_) || !GetMaterial(inspectorSlot_)->GetMaterialData()) {
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
                  SetSharedMaterial(inspectorSlot_, selectedMaterial, names[selectedIndex]);
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
         const std::string currentName = materialNames_.empty() ? std::string() : materialNames_[inspectorSlot_ < materialNames_.size() ? inspectorSlot_ : 0];
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
                     SetSharedMaterial(inspectorSlot_, selectedMaterial, names[i]);
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

   ImGui::TextUnformatted(IsOverridden(inspectorSlot_) ? Tr("個別設定", "Local override") : Tr("共有設定を参照", "Shared material"));
   if (IsOverridden(inspectorSlot_)) {
      if (ImGui::Button(Tr("共有設定へ戻す", "Reset to shared"))) ResetOverride(inspectorSlot_);
   } else {
      if (ImGui::Button(Tr("このオブジェクトだけ編集", "Make local override"))) EditMaterial(inspectorSlot_);
      ImGui::Checkbox(Tr("共有マテリアルを編集（全参照に反映）", "Edit shared material (all references)"), &editShared_);
   }
   auto* material = GetMaterial(inspectorSlot_);
   if (!material || !material->GetMaterialData()) return;
   auto* data = material->GetMaterialData();


   // テクスチャスロット（マテリアルスロットごとに表示）
   if (textureNamesProvider_) {
      const auto texNames = textureNamesProvider_();
      if (!texNames.empty()) {
         SyncMaterialNamesSize();
         ImGui::SeparatorText(Tr("テクスチャ", "Textures"));
         for (size_t slot = 0; slot < slotCount; ++slot) {
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
                     // Shader Resourceの次元が異なるCubemapは2D候補一覧から除外する。
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
                  // 縦横比を保ったまま最大96pxへ収め、Inspector幅を大きなTexture寸法へ依存させない。
                  const float maxSize = 96.0f;
                  const float w = static_cast<float>(tex->GetWidth());
                  const float h = static_cast<float>(tex->GetHeight());
                  const float scale = (w > h) ? (maxSize / w) : (maxSize / h);
                  const ImVec2 displaySize(w * scale, h * scale);
                  // ImGuiバックエンドが要求するImTextureIDへGPU descriptor値のビット列を安全に移す。
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

   ImGui::BeginDisabled(!IsOverridden(inspectorSlot_) && !editShared_);
   Vector4 color = data->color;
   if (ImGui::ColorEdit4(Tr("色", "Color"), &color.x)) {
      material->SetColor(color);
   }

   const char* lightingModeLabels[] = { Tr("なし", "None"), "Lambert", "HalfLambert", "Phong", "BlinnPhong" };
   int lightingMode = std::clamp(
      data->lightingMode,
      static_cast<int>(Material::LightingMode::NONE),
      static_cast<int>(Material::LightingMode::BLINNPHONG));
   if (ImGui::BeginCombo(Tr("ライティングモード", "Lighting Mode"), lightingModeLabels[lightingMode])) {
      for (int i = static_cast<int>(Material::LightingMode::NONE);
         i <= static_cast<int>(Material::LightingMode::BLINNPHONG);
         ++i) {
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
      const char* blendPreview =
         (blendIndex >= 0 && blendIndex < static_cast<int>(BlendMode::kCount))
         ? blendModeLabels[blendIndex]
         : Tr("<デフォルト>", "<default>");
      if (ImGui::BeginCombo(Tr("ブレンドモード", "Blend Mode"), blendPreview)) {
         if (ImGui::Selectable(Tr("<デフォルト>", "<default>"), blendIndex < 0)) {
            material->SetBlendMode(std::nullopt);
         }
         for (int i = 0; i < static_cast<int>(BlendMode::kCount); ++i) {
            const bool sel = (i == blendIndex);
            if (ImGui::Selectable(blendModeLabels[i], sel)) {
               material->SetBlendMode(static_cast<BlendMode>(i));
            }
            if (sel) { ImGui::SetItemDefaultFocus(); }
         }
         ImGui::EndCombo();
      }
   }

   // Model choices come only from the validated registry, including custom parameter metadata.
   if (pipelineManager_ && HasOwner() && GetOwner().GetComponent<MeshComponent>()) {
      const std::string name = material->GetPipelineName().empty() ? "Object3D" : material->GetPipelineName();
      if (ImGui::BeginCombo(Tr("シェーダー / パイプライン", "Shader / Pipeline"), name.c_str())) {
         for (const auto& candidate : pipelineManager_->GetModelPipelineNames()) {
            if (ImGui::Selectable(candidate.c_str(), candidate == name)) material->SetPipelineName(candidate);
         }
         ImGui::EndCombo();
      }
      const auto* definition = pipelineManager_->GetModelPipeline(material->GetPipelineName());
      if (!definition) ImGui::TextWrapped("%s", Tr("利用できない設定です。Object3Dへフォールバックします。", "Unavailable setting; drawing falls back to Object3D."));
      if (definition && !definition->parameters.empty()) {
         ImGui::SeparatorText(Tr("固有パラメーター", "Shader Parameters"));
         for (const auto& field : definition->parameters) {
            auto value = field.defaultValue;
            const auto it = material->GetParameters().find(field.name);
            if (it != material->GetParameters().end() && it->second.size() == value.size()) value = it->second;
            if (ImGui::DragScalarN(field.name.c_str(), ImGuiDataType_Float, value.data(), static_cast<int>(value.size()), 0.01f)) {
               material->SetParameter(field.name, value);
            }
         }
         if (ImGui::Button(Tr("パラメーターを既定値へ", "Reset parameters"))) material->ClearParameters();
      }
   } else {
      // Preserve the existing non-model pipeline editor.
      char buf[128] = {};
      material->GetPipelineName().copy(buf, sizeof(buf) - 1);
      if (ImGui::InputText(Tr("パイプライン名", "Pipeline Name"), buf, sizeof(buf))) material->SetPipelineName(buf);
   }
   Vector2 uvScale = material->GetUVScale();
   float uvRotation = material->GetUVRotation();
   float uvRotationDegrees = ImGuiHelper::RadiansToDegrees(uvRotation);
   Vector2 uvTranslation = material->GetUVTranslation();
   // Scale/Rotation/Translationを個別に編集し、いずれかが変わったフレームだけ行列を再合成する。
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

   ImGui::EndDisabled();
   ImGui::Spacing();
}
#endif

}
