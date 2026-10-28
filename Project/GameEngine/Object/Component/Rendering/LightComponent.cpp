#include "GameEngine/pch.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include <cstdint>
#include "GameEngine/Utility/JsonValue.h"
#include "GameEngine/Object/Component/Rendering/LightComponent.h"

#include "GameEngine/Object/Component/Base/ComponentRegistry.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Scene/Light/AreaLight.h"
#include "GameEngine/Scene/Light/DirectionalLight.h"
#include "GameEngine/Scene/Light/PointLight.h"
#include "GameEngine/Scene/Light/SpotLight.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include "GameEngine/Math/Functions/VectorOperations.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include <imgui.h>
#endif

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::LightComponent::kTypeName,
   [](GameEngine::Object& object) -> GameEngine::IObjectComponent* {
      // ライトの位置と向きはEntityのWorld Transformから同期するため、
      // Inspector追加時にも必ずTransformComponentを先に用意する。
      if (!object.HasComponent<GameEngine::TransformComponent>()) {
         object.AddComponent<GameEngine::TransformComponent>();
      }

      return object.AddComponent<GameEngine::LightComponent>();
   },
   GameEngine::LightComponent::kDisplayName, GameEngine::ObjectType::Generic | GameEngine::ObjectType::Model);

}

namespace GameEngine {

void LightComponent::SetLightType(Type type) {
   if (type_ == type) return;

   // LightManagerは種類別コンテナを持つ。同じキーのまま型だけ変えると旧コンテナにも
   // 登録が残るため、型を書き換える前に現在のRuntimeライトを解除する。
   ReleaseRuntimeLight();
   type_ = type;
}

void LightComponent::Update(float deltaTime) {
   (void)deltaTime;
   SynchronizeRuntimeLight();
}

void LightComponent::OnDisable() {
   // 無効ComponentをLightManagerに残すと描画だけ継続するため、ライフサイクルに合わせて登録解除する。
   ReleaseRuntimeLight();
}

void LightComponent::OnEnable() {
   SynchronizeRuntimeLight();
}

void LightComponent::OnDetach() {
   ReleaseRuntimeLight();
}

nlohmann::json LightComponent::Serialize() const {
   return nlohmann::json{{"lightType", TypeToString(type_)},
      {"color", {settings_.color.x, settings_.color.y, settings_.color.z, settings_.color.w}},
      {"intensity", settings_.intensity}, {"radius", settings_.radius}, {"decay", settings_.decay},
      {"distance", settings_.distance}, {"cosAngle", settings_.cosAngle},
      {"cosFalloffStart", settings_.cosFalloffStart}, {"areaSize", {settings_.areaSize.x, settings_.areaSize.y}}};
}

nlohmann::json LightComponent::GetSharedParameters() const {
   return ExtractSharedParameters({});
}

void LightComponent::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void LightComponent::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void LightComponent::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   if (!data.is_object()) return;

   Type parsedType = type_;

   if (TryParseType(data.value("lightType", data.value("type", "")), parsedType)) {
      SetLightType(parsedType);
   }

   auto settings = DescribeSettings();
   settings.color = data.contains("color") ? JsonValue::ReadVector4(data.at("color"), settings.color) : settings.color;
   settings.intensity = JsonValue::ReadFloat(data, "intensity", settings.intensity);
   settings.radius = JsonValue::ReadFloat(data, "radius", settings.radius);
   settings.decay = JsonValue::ReadFloat(data, "decay", settings.decay);
   settings.distance = JsonValue::ReadFloat(data, "distance", settings.distance);
   settings.cosAngle = JsonValue::ReadFloat(data, "cosAngle", settings.cosAngle);
   settings.cosFalloffStart = JsonValue::ReadFloat(data, "cosFalloffStart", settings.cosFalloffStart);

   settings.areaSize =
      data.contains("areaSize") ? JsonValue::ReadVector2(data.at("areaSize"), settings.areaSize) : settings.areaSize;
   Configure(settings);
}

void LightComponent::SynchronizeRuntimeLight() {
   if (!HasOwner() || !IsEnabled()) return;

   // Entity IDをManager上の一意キーとして使い、Componentと描画用ライトを一対一に対応させる。
   const std::string runtimeKey = ResolveRuntimeKey();

   if (runtimeKey.empty()) return;

   // Entity IDの変更またはライト型変更があれば、旧キー/旧型の登録を先に削除する。
   if (!runtimeLightKey_.empty() && (runtimeLightKey_ != runtimeKey || registeredType_ != type_)) {
      ReleaseRuntimeLight();
   }

   // 親子Transformを含むWorld行列から位置とローカル基底を取り出す。
   // 方向ベクトルには平行移動を適用せず、スケールの影響をNormalizeで除去する。
   const Matrix4x4 worldMatrix = GetOwner().GetWorldMatrix();
   const Vector3 position(worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2]);
   const Vector3 forward = TransformNormal(Vector3(0.0f, 0.0f, 1.0f), worldMatrix).Normalize();
   const Vector3 right = TransformNormal(Vector3(1.0f, 0.0f, 0.0f), worldMatrix).Normalize();

   // 以降は種類別Managerに「なければ作成、あれば更新」する。
   // キーと登録型を先に記録し、Disable/Detach時に正しいコンテナから削除できるようにする。
   runtimeLightKey_ = runtimeKey;
   registeredType_ = type_;

   switch (type_) {
      case Type::Directional: {
         auto* light = EngineContext::GetDirectionalLight(runtimeKey);

         if (!light) {
            light = EngineContext::CreateDirectionalLight(runtimeKey);
         }

         if (light && light->GetDirectionalLightData()) {
            auto data = *light->GetDirectionalLightData();
            data.color = settings_.color;
            data.direction = forward;
            data.intensity = settings_.intensity;
            light->ApplyIllumination(data);
         }

         break;
      }
      case Type::Point: {
         auto* light = EngineContext::GetPointLight(runtimeKey);

         if (!light) {
            light = EngineContext::CreatePointLight(runtimeKey);
         }

         if (light && light->GetPointLightData()) {
            auto data = *light->GetPointLightData();
            data.color = settings_.color;
            data.position = position;
            data.intensity = settings_.intensity;

            // 負値は減衰式を不安定にするため、GPUへ渡す境界で物理量を0以上に制限する。
            data.radius = std::max(settings_.radius, 0.0f);
            data.decay = std::max(settings_.decay, 0.0f);
            light->ApplyIllumination(data);
         }

         break;
      }
      case Type::Spot: {
         auto* light = EngineContext::GetSpotLight(runtimeKey);

         if (!light) {
            light = EngineContext::CreateSpotLight(runtimeKey);
         }

         if (light && light->GetSpotLightData()) {
            auto data = *light->GetSpotLightData();
            data.color = settings_.color;
            data.position = position;
            data.intensity = settings_.intensity;
            data.direction = forward;
            data.distance = std::max(settings_.distance, 0.0f);
            data.decay = std::max(settings_.decay, 0.0f);
            data.cosAngle = settings_.cosAngle;
            data.cosFalloffStart = settings_.cosFalloffStart;
            light->ApplyIllumination(data);
         }

         break;
      }
      case Type::Area: {
         auto* light = EngineContext::GetAreaLight(runtimeKey);

         if (!light) {
            light = EngineContext::CreateAreaLight(runtimeKey);
         }

         if (light && light->GetAreaLightData()) {
            auto data = *light->GetAreaLightData();
            data.color = settings_.color;
            data.position = position;
            data.intensity = settings_.intensity;

            // Area Lightは面法線にforward、面内方向にrightを用い、Transform回転へ追従させる。
            data.normal = forward;
            data.tangent = right;
            data.width = std::max(settings_.areaSize.x, 0.0f);
            data.height = std::max(settings_.areaSize.y, 0.0f);
            light->ApplyIllumination(data);
         }

         break;
      }
   }
}

void LightComponent::ReleaseRuntimeLight() {
   if (runtimeLightKey_.empty()) return;

   // type_ではなく実際に登録した型を使う。SetLightTypeは解除後にtype_を更新するため、
   // ここで現在値を見ると別のManagerへ削除要求を送る可能性がある。
   switch (registeredType_) {
      case Type::Directional:
         EngineContext::RemoveDirectionalLight(runtimeLightKey_);
         break;
      case Type::Point:
         EngineContext::RemovePointLight(runtimeLightKey_);
         break;
      case Type::Spot:
         EngineContext::RemoveSpotLight(runtimeLightKey_);
         break;
      case Type::Area:
         EngineContext::RemoveAreaLight(runtimeLightKey_);
         break;
   }

   runtimeLightKey_.clear();
}

std::string LightComponent::ResolveRuntimeKey() const {
   return HasOwner() ? GetOwner().GetEntityId() : std::string();
}

const char* LightComponent::TypeToString(Type type) {
   switch (type) {
      case Type::Directional:
         return "directional";
      case Type::Point:
         return "point";
      case Type::Spot:
         return "spot";
      case Type::Area:
         return "area";
   }

   return "directional";
}

bool LightComponent::TryParseType(const std::string& value, Type& type) {
   if (value == "directional") {
      type = Type::Directional;
      return true;
   }

   if (value == "point") {
      type = Type::Point;
      return true;
   }

   if (value == "spot") {
      type = Type::Spot;
      return true;
   }

   if (value == "area") {
      type = Type::Area;
      return true;
   }

   return false;
}

#ifdef USE_IMGUI
void LightComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   const char* types[] = {"Directional", "Point", "Spot", "Area"};
   std::int32_t selectedType = static_cast<std::int32_t>(type_);

   if (ImGui::Combo("Type", &selectedType, types, IM_ARRAYSIZE(types))) {
      SetLightType(static_cast<Type>(selectedType));
   }

   auto settings = DescribeSettings();
   ImGui::ColorEdit4("Color", &settings.color.x);
   ImGui::DragFloat("Intensity", &settings.intensity, 0.05f, 0.0f, 1000.0f);

   if (type_ == Type::Point) {
      ImGui::DragFloat("Radius", &settings.radius, 0.05f, 0.0f, 10000.0f);
      ImGui::DragFloat("Decay", &settings.decay, 0.01f, 0.0f, 100.0f);
   } else if (type_ == Type::Spot) {
      ImGui::DragFloat("Distance", &settings.distance, 0.05f, 0.0f, 10000.0f);
      ImGui::DragFloat("Decay", &settings.decay, 0.01f, 0.0f, 100.0f);
      ImGui::SliderFloat("Cos Angle", &settings.cosAngle, -1.0f, 1.0f);
      ImGui::SliderFloat("Cos Falloff Start", &settings.cosFalloffStart, -1.0f, 1.0f);
   } else if (type_ == Type::Area) {
      ImGui::DragFloat2("Size", &settings.areaSize.x, 0.05f, 0.0f, 10000.0f);
   }

   Configure(settings);

   // Edit停止中でもInspector操作の結果を同じフレームのViewportへ反映する。
   SynchronizeRuntimeLight();
}
#endif

} // namespace GameEngine

namespace GameEngine {

LightComponent::Settings LightComponent::DescribeSettings() const {
   return settings_;
}

void LightComponent::Configure(const Settings& requested) {
   auto settings = requested;
   [[maybe_unused]] const Settings defaults;

   if (!std::isfinite(settings.color.x)) {
      settings.color.x = defaults.color.x;
   }

   if (!std::isfinite(settings.color.y)) {
      settings.color.y = defaults.color.y;
   }

   if (!std::isfinite(settings.color.z)) {
      settings.color.z = defaults.color.z;
   }

   if (!std::isfinite(settings.color.w)) {
      settings.color.w = defaults.color.w;
   }

   if (!std::isfinite(settings.intensity)) {
      settings.intensity = defaults.intensity;
   }

   if (!std::isfinite(settings.radius)) {
      settings.radius = defaults.radius;
   }

   if (!std::isfinite(settings.decay)) {
      settings.decay = defaults.decay;
   }

   if (!std::isfinite(settings.distance)) {
      settings.distance = defaults.distance;
   }

   if (!std::isfinite(settings.cosAngle)) {
      settings.cosAngle = defaults.cosAngle;
   }

   if (!std::isfinite(settings.cosFalloffStart)) {
      settings.cosFalloffStart = defaults.cosFalloffStart;
   }

   if (!std::isfinite(settings.areaSize.x)) {
      settings.areaSize.x = defaults.areaSize.x;
   }

   if (!std::isfinite(settings.areaSize.y)) {
      settings.areaSize.y = defaults.areaSize.y;
   }

   settings_ = settings;
}

} // namespace GameEngine
