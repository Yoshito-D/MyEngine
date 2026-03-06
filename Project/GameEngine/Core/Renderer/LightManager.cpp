#include "LightManager.h"
#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#endif
#include "../Graphics/DirectionalLight.h"
#include "../Graphics/PointLight.h"
#include "../Graphics/SpotLight.h"
#include "../Graphics/AreaLight.h"
#include "MathUtils.h"
#include <string>

namespace GameEngine {

void LightManager::Initialize() {
   lightDataBuffer_ = std::make_unique<LightDataBuffer>();
   lightDataBuffer_->Create(1, 32, 32, 16);
}

//================================================================
// DirectionalLight
//================================================================

DirectionalLight* LightManager::CreateDirectionalLight(const std::string& name, unsigned int color, const Vector3& direction, float intensity) {
   auto light = std::make_unique<DirectionalLight>();
   light->Create(color, direction, intensity);
   auto* ptr = light.get();
   directionalLights_[name] = std::move(light);
   return ptr;
}

DirectionalLight* LightManager::GetDirectionalLight(const std::string& name) const {
   auto it = directionalLights_.find(name);
   if (it != directionalLights_.end()) {
      return it->second.get();
   }
   return nullptr;
}

bool LightManager::RemoveDirectionalLight(const std::string& name) {
   // 最後の1つは削除できない
   if (directionalLights_.size() <= 1) {
      return false;
   }
   return directionalLights_.erase(name) > 0;
}

void LightManager::ClearDirectionalLights() {
   // 最低1つは残す
   if (directionalLights_.size() <= 1) {
      return;
   }
   
   // 最初の1つを残して削除
   auto it = directionalLights_.begin();
   ++it; // 最初の要素をスキップ
   directionalLights_.erase(it, directionalLights_.end());
}

std::vector<std::string> LightManager::GetDirectionalLightNames() const {
   std::vector<std::string> names;
   names.reserve(directionalLights_.size());
   for (const auto& pair : directionalLights_) {
      names.push_back(pair.first);
   }
   return names;
}

//================================================================
// PointLight
//================================================================

PointLight* LightManager::CreatePointLight(const std::string& name, unsigned int color, const Vector3& position, float intensity, float radius, float decay) {
   auto light = std::make_unique<PointLight>();
   light->Create(color, position, intensity, radius, decay);
   auto* ptr = light.get();
   pointLights_[name] = std::move(light);
   return ptr;
}

PointLight* LightManager::GetPointLight(const std::string& name) const {
   auto it = pointLights_.find(name);
   if (it != pointLights_.end()) {
      return it->second.get();
   }
   return nullptr;
}

bool LightManager::RemovePointLight(const std::string& name) {
   // 最後の1つは削除できない
   if (pointLights_.size() <= 1) {
      return false;
   }
   return pointLights_.erase(name) > 0;
}

void LightManager::ClearPointLights() {
   // 最低1つは残す
   if (pointLights_.size() <= 1) {
      return;
   }
   
   // 最初の1つを残して削除
   auto it = pointLights_.begin();
   ++it; // 最初の要素をスキップ
   pointLights_.erase(it, pointLights_.end());
}

std::vector<std::string> LightManager::GetPointLightNames() const {
   std::vector<std::string> names;
   names.reserve(pointLights_.size());
   for (const auto& pair : pointLights_) {
      names.push_back(pair.first);
   }
   return names;
}

//================================================================
// SpotLight
//================================================================

SpotLight* LightManager::CreateSpotLight(const std::string& name, unsigned int color, const Vector3& position, float intensity, const Vector3& direction, float distance, float decay, float cosAngle, float cosFalloffStart) {
   auto light = std::make_unique<SpotLight>();
   light->Create(color, position, intensity, direction, distance, decay, cosAngle, cosFalloffStart);
   auto* ptr = light.get();
   spotLights_[name] = std::move(light);
   return ptr;
}

SpotLight* LightManager::GetSpotLight(const std::string& name) const {
   auto it = spotLights_.find(name);
   if (it != spotLights_.end()) {
      return it->second.get();
   }
   return nullptr;
}

bool LightManager::RemoveSpotLight(const std::string& name) {
   // 最後の1つは削除できない
   if (spotLights_.size() <= 1) {
      return false;
   }
   return spotLights_.erase(name) > 0;
}

void LightManager::ClearSpotLights() {
   // 最低1つは残す
   if (spotLights_.size() <= 1) {
      return;
   }
   
   // 最初の1つを残して削除
   auto it = spotLights_.begin();
   ++it; // 最初の要素をスキップ
   spotLights_.erase(it, spotLights_.end());
}

std::vector<std::string> LightManager::GetSpotLightNames() const {
   std::vector<std::string> names;
   names.reserve(spotLights_.size());
   for (const auto& pair : spotLights_) {
      names.push_back(pair.first);
   }
   return names;
}

//================================================================
// AreaLight
//================================================================

AreaLight* LightManager::CreateAreaLight(const std::string& name, const Vector3& position, const Vector3& normal, const Vector3& tangent, const Vector2& size, const Vector3& color, float intensity) {
   auto light = std::make_unique<AreaLight>();
   light->Create(position, normal, tangent, size, color, intensity);
   auto* ptr = light.get();
   areaLights_[name] = std::move(light);
   return ptr;
}

AreaLight* LightManager::GetAreaLight(const std::string& name) const {
   auto it = areaLights_.find(name);
   if (it != areaLights_.end()) {
      return it->second.get();
   }
   return nullptr;
}

bool LightManager::RemoveAreaLight(const std::string& name) {
   // 最後の1つは削除できない
   if (areaLights_.size() <= 1) {
      return false;
   }
   return areaLights_.erase(name) > 0;
}

void LightManager::ClearAreaLights() {
   // 最低1つは残す
   if (areaLights_.size() <= 1) {
      return;
   }
   
   // 最初の1つを残して削除
   auto it = areaLights_.begin();
   ++it; // 最初の要素をスキップ
   areaLights_.erase(it, areaLights_.end());
}

std::vector<std::string> LightManager::GetAreaLightNames() const {
   std::vector<std::string> names;
   names.reserve(areaLights_.size());
   for (const auto& pair : areaLights_) {
      names.push_back(pair.first);
   }
   return names;
}

//================================================================
// Buffer & Debug
//================================================================

void LightManager::UpdateStructureBuffer() {
   if (!lightDataBuffer_) return;

   // ディレクショナルライトの更新
   std::vector<LightDataBuffer::DirectionalLightData> dirLights;
   for (const auto& pair : directionalLights_) {
      auto* data = pair.second->GetDirectionalLightData();
      LightDataBuffer::DirectionalLightData lightData;
      lightData.color = data->color;
      lightData.direction = data->direction;
      lightData.intensity = data->intensity;
      dirLights.push_back(lightData);
   }
   lightDataBuffer_->UpdateDirectionalLights(dirLights);

   // ポイントライトの更新
   std::vector<LightDataBuffer::PointLightData> ptLights;
   for (const auto& pair : pointLights_) {
      auto* data = pair.second->GetPointLightData();
      LightDataBuffer::PointLightData lightData;
      lightData.color = data->color;
      lightData.position = data->position;
      lightData.intensity = data->intensity;
      lightData.radius = data->radius;
      lightData.decay = data->decay;
      lightData.padding[0] = 0.0f;
      lightData.padding[1] = 0.0f;
      ptLights.push_back(lightData);
   }
   lightDataBuffer_->UpdatePointLights(ptLights);

   // スポットライトの更新
   std::vector<LightDataBuffer::SpotLightData> spLights;
   for (const auto& pair : spotLights_) {
      auto* data = pair.second->GetSpotLightData();
      LightDataBuffer::SpotLightData lightData;
      lightData.color = data->color;
      lightData.position = data->position;
      lightData.intensity = data->intensity;
      lightData.direction = data->direction;
      lightData.distance = data->distance;
      lightData.decay = data->decay;
      lightData.cosAngle = data->cosAngle;
      lightData.cosFalloffStart = data->cosFalloffStart;
      lightData.padding = 0.0f;
      spLights.push_back(lightData);
   }
   lightDataBuffer_->UpdateSpotLights(spLights);

   // エリアライトの更新
   std::vector<LightDataBuffer::AreaLightData> areaLightsData;
   for (const auto& pair : areaLights_) {
      auto* data = pair.second->GetAreaLightData();
      LightDataBuffer::AreaLightData lightData;
      lightData.color = data->color;
      lightData.position = data->position;
      lightData.intensity = data->intensity;
      lightData.normal = data->normal;
      lightData.width = data->width;
      lightData.tangent = data->tangent;
      lightData.height = data->height;
      lightData.padding = Vector3(0.0f, 0.0f, 0.0f);
      lightData.padding2 = 0.0f;
      areaLightsData.push_back(lightData);
   }
   lightDataBuffer_->UpdateAreaLights(areaLightsData);
}

void LightManager::DebugDraw() {
#ifdef USE_IMGUI
   if (ImGui::Begin("LightManager")) {
      // ディレクショナルライト
      if (ImGui::TreeNode("Directional Light")) {

         // 既存のライトを表示・編集
         std::vector<std::string> toRemove;
         for (auto& pair : directionalLights_) {
            ImGui::PushID(pair.first.c_str());
            if (ImGui::TreeNode(pair.first.c_str())) {
               auto data = pair.second->GetDirectionalLightData();
               ImGui::ColorEdit4("Color", &data->color.x);
               ImGui::DragFloat3("Direction", &data->direction.x, 0.01f);
               data->direction = Normalize(data->direction);
               ImGui::DragFloat("Intensity", &data->intensity, 0.01f);
               
               ImGui::TreePop();
            }
            ImGui::PopID();
         }
         
         for (const auto& name : toRemove) {
            RemoveDirectionalLight(name);
         }
         
         ImGui::TreePop();
      }

      // ポイントライト
      if (ImGui::TreeNode("Point Lights")) {
         static char newPointLightName[128] = "";
         ImGui::InputText("New Name##PointLight", newPointLightName, sizeof(newPointLightName));
         ImGui::SameLine();
         if (ImGui::Button("Add##PointLight")) {
            if (strlen(newPointLightName) > 0) {
               CreatePointLight(newPointLightName);
               newPointLightName[0] = '\0';
            }
         }
         
         // 最小ライト数の警告
         if (pointLights_.size() == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ERROR: At least 1 Point Light is required!");
         } else if (pointLights_.size() == 1) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "WARNING: Cannot delete the last Point Light!");
         }
         
         ImGui::Separator();

         std::vector<std::string> toRemove;
         for (auto& pair : pointLights_) {
            ImGui::PushID(pair.first.c_str());
            if (ImGui::TreeNode(pair.first.c_str())) {
               auto data = pair.second->GetPointLightData();
               ImGui::ColorEdit4("Color", &data->color.x);
               ImGui::DragFloat3("Position", &data->position.x, 0.1f);
               ImGui::DragFloat("Intensity", &data->intensity, 0.01f);
               ImGui::DragFloat("Radius", &data->radius, 0.1f);
               ImGui::DragFloat("Decay", &data->decay, 0.01f);
               
               // 最後の1つの場合は削除ボタンを無効化
               bool canRemove = pointLights_.size() > 1;
               if (!canRemove) {
                  ImGui::BeginDisabled();
               }
               
               if (ImGui::Button("Remove")) {
                  toRemove.push_back(pair.first);
               }
               
               if (!canRemove) {
                  ImGui::EndDisabled();
                  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                     ImGui::SetTooltip("Cannot delete the last Point Light!");
                  }
               }
               
               ImGui::TreePop();
            }
            ImGui::PopID();
         }
         
         for (const auto& name : toRemove) {
            RemovePointLight(name);
         }
         
         ImGui::TreePop();
      }

      // スポットライト
      if (ImGui::TreeNode("Spot Lights")) {
         static char newSpotLightName[128] = "";
         ImGui::InputText("New Name##SpotLight", newSpotLightName, sizeof(newSpotLightName));
         ImGui::SameLine();
         if (ImGui::Button("Add##SpotLight")) {
            if (strlen(newSpotLightName) > 0) {
               CreateSpotLight(newSpotLightName);
               newSpotLightName[0] = '\0';
            }
         }
         
         // 最小ライト数の警告
         if (spotLights_.size() == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ERROR: At least 1 Spot Light is required!");
         } else if (spotLights_.size() == 1) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "WARNING: Cannot delete the last Spot Light!");
         }
         
         ImGui::Separator();

         std::vector<std::string> toRemove;
         for (auto& pair : spotLights_) {
            ImGui::PushID(pair.first.c_str());
            if (ImGui::TreeNode(pair.first.c_str())) {
               auto data = pair.second->GetSpotLightData();
               ImGui::ColorEdit4("Color", &data->color.x);
               ImGui::DragFloat3("Position", &data->position.x, 0.1f);
               ImGui::DragFloat("Intensity", &data->intensity, 0.01f);
               ImGui::DragFloat3("Direction", &data->direction.x, 0.01f);
               data->direction = Normalize(data->direction);
               ImGui::DragFloat("Distance", &data->distance, 0.1f);
               ImGui::DragFloat("Decay", &data->decay, 0.01f);
               ImGui::DragFloat("CosAngle", &data->cosAngle, 0.01f);
               ImGui::DragFloat("CosFalloffStart", &data->cosFalloffStart, 0.01f);
               
               // 最後の1つの場合は削除ボタンを無効化
               bool canRemove = spotLights_.size() > 1;
               if (!canRemove) {
                  ImGui::BeginDisabled();
               }
               
               if (ImGui::Button("Remove")) {
                  toRemove.push_back(pair.first);
               }
               
               if (!canRemove) {
                  ImGui::EndDisabled();
                  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                     ImGui::SetTooltip("Cannot delete the last Spot Light!");
                  }
               }
               
               ImGui::TreePop();
            }
            ImGui::PopID();
         }
         
         for (const auto& name : toRemove) {
            RemoveSpotLight(name);
         }
         
         ImGui::TreePop();
      }

      // エリアライト
      if (ImGui::TreeNode("Area Lights")) {
         static char newAreaLightName[128] = "";
         ImGui::InputText("New Name##AreaLight", newAreaLightName, sizeof(newAreaLightName));
         ImGui::SameLine();
         if (ImGui::Button("Add##AreaLight")) {
            if (strlen(newAreaLightName) > 0) {
               CreateAreaLight(newAreaLightName);
               newAreaLightName[0] = '\0';
            }
         }
         
         // 最小ライト数の警告
         if (areaLights_.size() == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ERROR: At least 1 Area Light is required!");
         } else if (areaLights_.size() == 1) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "WARNING: Cannot delete the last Area Light!");
         }
         
         ImGui::Separator();

         std::vector<std::string> toRemove;
         for (auto& pair : areaLights_) {
            ImGui::PushID(pair.first.c_str());
            if (ImGui::TreeNode(pair.first.c_str())) {
               auto data = pair.second->GetAreaLightData();
               ImGui::ColorEdit4("Color", &data->color.x);
               ImGui::DragFloat3("Position", &data->position.x, 0.1f);
               ImGui::DragFloat("Intensity", &data->intensity, 0.01f);
               ImGui::DragFloat3("Normal", &data->normal.x, 0.01f);
               data->normal = Normalize(data->normal);
               ImGui::DragFloat("Width", &data->width, 0.1f);
               ImGui::DragFloat3("Tangent", &data->tangent.x, 0.01f);
               data->tangent = Normalize(data->tangent);
               ImGui::DragFloat("Height", &data->height, 0.1f);
               
               // 最後の1つの場合は削除ボタンを無効化
               bool canRemove = areaLights_.size() > 1;
               if (!canRemove) {
                  ImGui::BeginDisabled();
               }
               
               if (ImGui::Button("Remove")) {
                  toRemove.push_back(pair.first);
               }
               
               if (!canRemove) {
                  ImGui::EndDisabled();
                  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                     ImGui::SetTooltip("Cannot delete the last Area Light!");
                  }
               }
               
               ImGui::TreePop();
            }
            ImGui::PopID();
         }
         
         for (const auto& name : toRemove) {
            RemoveAreaLight(name);
         }
         
         ImGui::TreePop();
      }
   }
   ImGui::End();
#endif
}
}
