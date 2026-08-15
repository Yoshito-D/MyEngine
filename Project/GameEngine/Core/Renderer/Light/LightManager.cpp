#include "LightManager.h"
#ifdef USE_IMGUI
#include "imgui.h"
#include "Utility/ImGuiHelper.h"
#endif
#include "DirectionalLight.h"
#include "PointLight.h"
#include "SpotLight.h"
#include "AreaLight.h"
#include "MathUtils.h"
#include "Utility/Logger.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>
#include <nlohmann/json.hpp>

namespace {
using json = nlohmann::json;

json SerializeVector2(const GameEngine::Vector2& value) {
   return json::array({ value.x, value.y });
}

json SerializeVector3(const GameEngine::Vector3& value) {
   return json::array({ value.x, value.y, value.z });
}

json SerializeVector4(const GameEngine::Vector4& value) {
   return json::array({ value.x, value.y, value.z, value.w });
}

bool ReadFloat(const json& source, const char* key, float& destination) {
   const auto it = source.find(key);
   if (it == source.end() || !it->is_number()) {
      return false;
   }
   try {
      destination = it->get<float>();
      // JSONとして数値でもNaN/Infは行列・照明計算を伝播して画面全体を壊すため拒否する。
      return std::isfinite(destination);
   } catch (const json::exception&) {
      return false;
   }
}

bool ReadVector2(const json& source, const char* key, GameEngine::Vector2& destination) {
   const auto it = source.find(key);
   if (it == source.end() || !it->is_array() || it->size() != 2 ||
      !(*it)[0].is_number() || !(*it)[1].is_number()) {
      return false;
   }
   try {
      destination = GameEngine::Vector2((*it)[0].get<float>(), (*it)[1].get<float>());
      return std::isfinite(destination.x) && std::isfinite(destination.y);
   } catch (const json::exception&) {
      return false;
   }
}

bool ReadVector3(const json& source, const char* key, GameEngine::Vector3& destination) {
   const auto it = source.find(key);
   if (it == source.end() || !it->is_array() || it->size() != 3 ||
      !(*it)[0].is_number() || !(*it)[1].is_number() || !(*it)[2].is_number()) {
      return false;
   }
   try {
      destination = GameEngine::Vector3(
         (*it)[0].get<float>(),
         (*it)[1].get<float>(),
         (*it)[2].get<float>());
      return std::isfinite(destination.x) &&
         std::isfinite(destination.y) &&
         std::isfinite(destination.z);
   } catch (const json::exception&) {
      return false;
   }
}

bool ReadVector4(const json& source, const char* key, GameEngine::Vector4& destination) {
   const auto it = source.find(key);
   if (it == source.end() || !it->is_array() || it->size() != 4 ||
      !(*it)[0].is_number() || !(*it)[1].is_number() ||
      !(*it)[2].is_number() || !(*it)[3].is_number()) {
      return false;
   }
   try {
      destination = GameEngine::Vector4(
         (*it)[0].get<float>(),
         (*it)[1].get<float>(),
         (*it)[2].get<float>(),
         (*it)[3].get<float>());
      return std::isfinite(destination.x) &&
         std::isfinite(destination.y) &&
         std::isfinite(destination.z) &&
         std::isfinite(destination.w);
   } catch (const json::exception&) {
      return false;
   }
}

struct DirectionalSceneLight {
   std::string id;
   GameEngine::DirectionalLight::DirectionalLightData data{};
};

struct PointSceneLight {
   std::string id;
   GameEngine::PointLight::PointLightData data{};
};

struct SpotSceneLight {
   std::string id;
   GameEngine::SpotLight::SpotLightData data{};
};

struct AreaSceneLight {
   std::string id;
   GameEngine::AreaLight::AreaLightData data{};
};

#ifdef USE_IMGUI
const char* LocalizeEditorText(const char* japanese, const char* english) {
   return GameEngine::ImGuiHelper::Localize({ japanese, english });
}

std::string StableEditorLabel(const char* japanese, const char* english, const char* id) {
   return std::string(LocalizeEditorText(japanese, english)) + "###" + id;
}
#endif
} // namespace

namespace GameEngine {

void LightManager::Initialize() {
   // シェーダー側の固定上限と同じ容量で各StructuredBufferを一度だけ確保する。
   // フレーム更新では件数分だけ内容を書き換え、リソース自体は再利用する。
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
   return directionalLights_.erase(name) > 0;
}

void LightManager::ClearDirectionalLights() {
   directionalLights_.clear();
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
   return pointLights_.erase(name) > 0;
}

void LightManager::ClearPointLights() {
   pointLights_.clear();
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
   return spotLights_.erase(name) > 0;
}

void LightManager::ClearSpotLights() {
   spotLights_.clear();
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
   return areaLights_.erase(name) > 0;
}

void LightManager::ClearAreaLights() {
   areaLights_.clear();
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

   // ゲームオブジェクト側のライト構造体を、HLSL StructuredBufferと一致する
   // GPU専用レイアウトへ型別に詰め直す。明示paddingもここで初期化する。
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

   // CPU構造体の未初期化領域をGPUへ送らないよう、パディングを含め全フィールドを代入する。
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

nlohmann::json LightManager::SerializeSceneState() const {
   // 型ごとの内部コンテナを単一配列へ正規化し、idとtypeの組で復元できる形式にする。
   // GPU向けpaddingはシーンの意味を持たないため保存しない。
   nlohmann::json lights = nlohmann::json::array();
   for (const auto& [id, light] : directionalLights_) {
      if (!light || !light->GetDirectionalLightData()) {
         continue;
      }
      const auto& data = *light->GetDirectionalLightData();
      lights.push_back({
         { "id", id },
         { "type", "directional" },
         { "color", SerializeVector4(data.color) },
         { "direction", SerializeVector3(data.direction) },
         { "intensity", data.intensity }
      });
   }
   for (const auto& [id, light] : pointLights_) {
      if (!light || !light->GetPointLightData()) {
         continue;
      }
      const auto& data = *light->GetPointLightData();
      lights.push_back({
         { "id", id },
         { "type", "point" },
         { "color", SerializeVector4(data.color) },
         { "position", SerializeVector3(data.position) },
         { "intensity", data.intensity },
         { "radius", data.radius },
         { "decay", data.decay }
      });
   }
   for (const auto& [id, light] : spotLights_) {
      if (!light || !light->GetSpotLightData()) {
         continue;
      }
      const auto& data = *light->GetSpotLightData();
      lights.push_back({
         { "id", id },
         { "type", "spot" },
         { "color", SerializeVector4(data.color) },
         { "position", SerializeVector3(data.position) },
         { "intensity", data.intensity },
         { "direction", SerializeVector3(data.direction) },
         { "distance", data.distance },
         { "decay", data.decay },
         { "cosAngle", data.cosAngle },
         { "cosFalloffStart", data.cosFalloffStart }
      });
   }
   for (const auto& [id, light] : areaLights_) {
      if (!light || !light->GetAreaLightData()) {
         continue;
      }
      const auto& data = *light->GetAreaLightData();
      lights.push_back({
         { "id", id },
         { "type", "area" },
         { "color", SerializeVector4(data.color) },
         { "position", SerializeVector3(data.position) },
         { "intensity", data.intensity },
         { "normal", SerializeVector3(data.normal) },
         { "tangent", SerializeVector3(data.tangent) },
         { "size", SerializeVector2(Vector2(data.width, data.height)) }
      });
   }
   return nlohmann::json{ { "lights", std::move(lights) } };
}

bool LightManager::ApplySceneState(const nlohmann::json& state) {
   if (!state.is_object()) {
      return false;
   }
   const auto lightsIt = state.find("lights");
   if (lightsIt == state.end()) {
      return true;
   }
   if (!lightsIt->is_array()) {
      return false;
   }

   std::vector<DirectionalSceneLight> directionalLights;
   std::vector<PointSceneLight> pointLights;
   std::vector<SpotSceneLight> spotLights;
   std::vector<AreaSceneLight> areaLights;
   std::unordered_set<std::string> lightKeys;

   // 現在のライトを消す前に、入力全体を一時配列へ検証・変換する。
   // 途中で壊れた値が見つかった場合は既存シーンを維持したまま失敗できる。
   for (const auto& entry : *lightsIt) {
      if (!entry.is_object()) {
         return false;
      }
      const auto idIt = entry.find("id");
      const auto typeIt = entry.find("type");
      if (idIt == entry.end() || !idIt->is_string() ||
         typeIt == entry.end() || !typeIt->is_string()) {
         return false;
      }
      const std::string id = idIt->get<std::string>();
      const std::string type = typeIt->get<std::string>();
      // idは型ごとの名前空間なので、異なるライト型では同名を許可する。
      if (id.empty() || !lightKeys.insert(type + ":" + id).second) {
         return false;
      }

      if (type == "directional") {
         DirectionalSceneLight light;
         light.id = id;
         if (!ReadVector4(entry, "color", light.data.color) ||
            !ReadVector3(entry, "direction", light.data.direction) ||
            !ReadFloat(entry, "intensity", light.data.intensity)) {
            return false;
         }
         directionalLights.push_back(std::move(light));
      } else if (type == "point") {
         PointSceneLight light;
         light.id = id;
         if (!ReadVector4(entry, "color", light.data.color) ||
            !ReadVector3(entry, "position", light.data.position) ||
            !ReadFloat(entry, "intensity", light.data.intensity) ||
            !ReadFloat(entry, "radius", light.data.radius) ||
            !ReadFloat(entry, "decay", light.data.decay)) {
            return false;
         }
         pointLights.push_back(std::move(light));
      } else if (type == "spot") {
         SpotSceneLight light;
         light.id = id;
         if (!ReadVector4(entry, "color", light.data.color) ||
            !ReadVector3(entry, "position", light.data.position) ||
            !ReadFloat(entry, "intensity", light.data.intensity) ||
            !ReadVector3(entry, "direction", light.data.direction) ||
            !ReadFloat(entry, "distance", light.data.distance) ||
            !ReadFloat(entry, "decay", light.data.decay) ||
            !ReadFloat(entry, "cosAngle", light.data.cosAngle) ||
            !ReadFloat(entry, "cosFalloffStart", light.data.cosFalloffStart)) {
            return false;
         }
         spotLights.push_back(std::move(light));
      } else if (type == "area") {
         AreaSceneLight light;
         light.id = id;
         Vector2 size;
         if (!ReadVector4(entry, "color", light.data.color) ||
            !ReadVector3(entry, "position", light.data.position) ||
            !ReadFloat(entry, "intensity", light.data.intensity) ||
            !ReadVector3(entry, "normal", light.data.normal) ||
            !ReadVector3(entry, "tangent", light.data.tangent) ||
            !ReadVector2(entry, "size", size)) {
            return false;
         }
         light.data.width = size.x;
         light.data.height = size.y;
         areaLights.push_back(std::move(light));
      } else {
         // Unknown light types are ignored so newer scene files remain forward-compatible.
         Logger::EngineWarning("[LightManager] Scene references an unknown light type: " + type);
         continue;
      }
   }

   if (directionalLights.size() > 1 || pointLights.size() > 32 ||
      spotLights.size() > 32 || areaLights.size() > 16) {
      // LightDataBufferの固定容量を越えるデータは切り捨てず、保存内容と描画結果の不一致を防ぐため拒否する。
      return false;
   }

   // 全項目の形式・有限値・重複・GPU容量を確認できた後にだけ現在状態を置換する。
   ClearDirectionalLights();
   ClearPointLights();
   ClearSpotLights();
   ClearAreaLights();

   for (const auto& source : directionalLights) {
      if (auto* light = CreateDirectionalLight(source.id)) {
         *light->GetDirectionalLightData() = source.data;
      }
   }
   for (const auto& source : pointLights) {
      if (auto* light = CreatePointLight(source.id)) {
         *light->GetPointLightData() = source.data;
      }
   }
   for (const auto& source : spotLights) {
      if (auto* light = CreateSpotLight(source.id)) {
         *light->GetSpotLightData() = source.data;
      }
   }
   for (const auto& source : areaLights) {
      if (auto* light = CreateAreaLight(source.id)) {
         *light->GetAreaLightData() = source.data;
      }
   }
   // 復元直後のフレームからGPU側の件数と内容を一致させる。
   UpdateStructureBuffer();
   return true;
}

bool LightManager::DebugDraw() {
#ifdef USE_IMGUI
   const nlohmann::json stateBeforeEditing = SerializeSceneState();
   const std::string windowLabel = StableEditorLabel("ライト", "Light Manager", "LightManager");
   if (ImGui::Begin(windowLabel.c_str())) {
      // ディレクショナルライト
      const std::string directionalLabel = StableEditorLabel(
         "ディレクショナルライト", "Directional Light", "DirectionalLightSection");
      if (ImGui::TreeNode(directionalLabel.c_str())) {
         if (directionalLights_.empty() &&
            ImGui::Button(LocalizeEditorText("ディレクショナルライトを追加", "Add Directional Light"))) {
            CreateDirectionalLight("DirectionalLight");
         }

         // 既存のライトを表示・編集
         std::vector<std::string> toRemove;
         for (auto& pair : directionalLights_) {
            ImGui::PushID(pair.first.c_str());
            if (ImGui::TreeNode(pair.first.c_str())) {
               auto data = pair.second->GetDirectionalLightData();
               ImGui::ColorEdit4(LocalizeEditorText("色", "Color"), &data->color.x);
               if (ImGui::DragFloat3(LocalizeEditorText("方向", "Direction"), &data->direction.x, 0.01f)) {
                  data->direction = Normalize(data->direction);
               }
               ImGui::DragFloat(LocalizeEditorText("強度", "Intensity"), &data->intensity, 0.01f);
               if (ImGui::Button(LocalizeEditorText("削除", "Remove"))) {
                  toRemove.push_back(pair.first);
               }
               
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
      const std::string pointLabel = StableEditorLabel(
         "ポイントライト", "Point Lights", "PointLightSection");
      if (ImGui::TreeNode(pointLabel.c_str())) {
         static char newPointLightName[128] = "";
         const std::string pointNameLabel = StableEditorLabel(
            "新規名", "New Name", "PointLightName");
         ImGui::InputText(pointNameLabel.c_str(), newPointLightName, sizeof(newPointLightName));
         ImGui::SameLine();
         const std::string addPointLabel = StableEditorLabel(
            "追加", "Add", "AddPointLight");
         if (ImGui::Button(addPointLabel.c_str())) {
            if (strlen(newPointLightName) > 0) {
               CreatePointLight(newPointLightName);
               newPointLightName[0] = '\0';
            }
         }
         
          ImGui::Separator();

         std::vector<std::string> toRemove;
         for (auto& pair : pointLights_) {
            ImGui::PushID(pair.first.c_str());
            if (ImGui::TreeNode(pair.first.c_str())) {
               auto data = pair.second->GetPointLightData();
               ImGui::ColorEdit4(LocalizeEditorText("色", "Color"), &data->color.x);
               ImGui::DragFloat3(LocalizeEditorText("位置", "Position"), &data->position.x, 0.1f);
               ImGui::DragFloat(LocalizeEditorText("強度", "Intensity"), &data->intensity, 0.01f);
               ImGui::DragFloat(LocalizeEditorText("半径", "Radius"), &data->radius, 0.1f);
               ImGui::DragFloat(LocalizeEditorText("減衰", "Decay"), &data->decay, 0.01f);
               
               if (ImGui::Button(LocalizeEditorText("削除", "Remove"))) {
                  toRemove.push_back(pair.first);
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
      const std::string spotLabel = StableEditorLabel(
         "スポットライト", "Spot Lights", "SpotLightSection");
      if (ImGui::TreeNode(spotLabel.c_str())) {
         static char newSpotLightName[128] = "";
         const std::string spotNameLabel = StableEditorLabel(
            "新規名", "New Name", "SpotLightName");
         ImGui::InputText(spotNameLabel.c_str(), newSpotLightName, sizeof(newSpotLightName));
         ImGui::SameLine();
         const std::string addSpotLabel = StableEditorLabel(
            "追加", "Add", "AddSpotLight");
         if (ImGui::Button(addSpotLabel.c_str())) {
            if (strlen(newSpotLightName) > 0) {
               CreateSpotLight(newSpotLightName);
               newSpotLightName[0] = '\0';
            }
         }
         
          ImGui::Separator();

         std::vector<std::string> toRemove;
         for (auto& pair : spotLights_) {
            ImGui::PushID(pair.first.c_str());
            if (ImGui::TreeNode(pair.first.c_str())) {
               auto data = pair.second->GetSpotLightData();
               ImGui::ColorEdit4(LocalizeEditorText("色", "Color"), &data->color.x);
               ImGui::DragFloat3(LocalizeEditorText("位置", "Position"), &data->position.x, 0.1f);
               ImGui::DragFloat(LocalizeEditorText("強度", "Intensity"), &data->intensity, 0.01f);
               if (ImGui::DragFloat3(LocalizeEditorText("方向", "Direction"), &data->direction.x, 0.01f)) {
                  data->direction = Normalize(data->direction);
               }
               ImGui::DragFloat(LocalizeEditorText("距離", "Distance"), &data->distance, 0.1f);
               ImGui::DragFloat(LocalizeEditorText("減衰", "Decay"), &data->decay, 0.01f);
               float angleDegrees = ImGuiHelper::RadiansToDegrees(std::acos(std::clamp(data->cosAngle, -1.0f, 1.0f)));
               if (ImGui::DragFloat(LocalizeEditorText("角度 (度)", "Angle (deg)"), &angleDegrees, 0.1f, 0.0f, 180.0f)) {
                  data->cosAngle = std::cos(ImGuiHelper::DegreesToRadians(angleDegrees));
               }

               float falloffStartDegrees = ImGuiHelper::RadiansToDegrees(std::acos(std::clamp(data->cosFalloffStart, -1.0f, 1.0f)));
               if (ImGui::DragFloat(
                  LocalizeEditorText("減衰開始角度 (度)", "Falloff Start (deg)"),
                  &falloffStartDegrees, 0.1f, 0.0f, 180.0f)) {
                  data->cosFalloffStart = std::cos(ImGuiHelper::DegreesToRadians(falloffStartDegrees));
               }
               
               if (ImGui::Button(LocalizeEditorText("削除", "Remove"))) {
                  toRemove.push_back(pair.first);
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
      const std::string areaLabel = StableEditorLabel(
         "エリアライト", "Area Lights", "AreaLightSection");
      if (ImGui::TreeNode(areaLabel.c_str())) {
         static char newAreaLightName[128] = "";
         const std::string areaNameLabel = StableEditorLabel(
            "新規名", "New Name", "AreaLightName");
         ImGui::InputText(areaNameLabel.c_str(), newAreaLightName, sizeof(newAreaLightName));
         ImGui::SameLine();
         const std::string addAreaLabel = StableEditorLabel(
            "追加", "Add", "AddAreaLight");
         if (ImGui::Button(addAreaLabel.c_str())) {
            if (strlen(newAreaLightName) > 0) {
               CreateAreaLight(newAreaLightName);
               newAreaLightName[0] = '\0';
            }
         }
         
          ImGui::Separator();

         std::vector<std::string> toRemove;
         for (auto& pair : areaLights_) {
            ImGui::PushID(pair.first.c_str());
            if (ImGui::TreeNode(pair.first.c_str())) {
               auto data = pair.second->GetAreaLightData();
               ImGui::ColorEdit4(LocalizeEditorText("色", "Color"), &data->color.x);
               ImGui::DragFloat3(LocalizeEditorText("位置", "Position"), &data->position.x, 0.1f);
               ImGui::DragFloat(LocalizeEditorText("強度", "Intensity"), &data->intensity, 0.01f);
               if (ImGui::DragFloat3(LocalizeEditorText("法線", "Normal"), &data->normal.x, 0.01f)) {
                  data->normal = Normalize(data->normal);
               }
               ImGui::DragFloat(LocalizeEditorText("幅", "Width"), &data->width, 0.1f);
               if (ImGui::DragFloat3(LocalizeEditorText("接線", "Tangent"), &data->tangent.x, 0.01f)) {
                  data->tangent = Normalize(data->tangent);
               }
               ImGui::DragFloat(LocalizeEditorText("高さ", "Height"), &data->height, 0.1f);
               
               if (ImGui::Button(LocalizeEditorText("削除", "Remove"))) {
                  toRemove.push_back(pair.first);
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
   return stateBeforeEditing != SerializeSceneState();
#else
   return false;
#endif
}
}
