#include "GameEngine/pch.h"
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
   return nlohmann::json{{"lightType", TypeToString(type_)}, {"color", {color.x, color.y, color.z, color.w}},
      {"intensity", intensity}, {"radius", radius}, {"decay", decay}, {"distance", distance}, {"cosAngle", cosAngle},
      {"cosFalloffStart", cosFalloffStart}, {"areaSize", {areaSize.x, areaSize.y}}};
}

void LightComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) return;

   Type parsedType = type_;

   if (TryParseType(data.value("lightType", data.value("type", "")), parsedType)) {
      SetLightType(parsedType);
   }

   color = data.contains("color") ? JsonValue::ReadVector4(data.at("color"), color) : color;
   intensity = JsonValue::ReadFloat(data, "intensity", intensity);
   radius = JsonValue::ReadFloat(data, "radius", radius);
   decay = JsonValue::ReadFloat(data, "decay", decay);
   distance = JsonValue::ReadFloat(data, "distance", distance);
   cosAngle = JsonValue::ReadFloat(data, "cosAngle", cosAngle);
   cosFalloffStart = JsonValue::ReadFloat(data, "cosFalloffStart", cosFalloffStart);

   areaSize = data.contains("areaSize") ? JsonValue::ReadVector2(data.at("areaSize"), areaSize) : areaSize;
   Configure(DescribeSettings());
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
            data.color = color;
            data.direction = forward;
            data.intensity = intensity;
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
            data.color = color;
            data.position = position;
            data.intensity = intensity;

            // 負値は減衰式を不安定にするため、GPUへ渡す境界で物理量を0以上に制限する。
            data.radius = std::max(radius, 0.0f);
            data.decay = std::max(decay, 0.0f);
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
            data.color = color;
            data.position = position;
            data.intensity = intensity;
            data.direction = forward;
            data.distance = std::max(distance, 0.0f);
            data.decay = std::max(decay, 0.0f);
            data.cosAngle = cosAngle;
            data.cosFalloffStart = cosFalloffStart;
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
            data.color = color;
            data.position = position;
            data.intensity = intensity;

            // Area Lightは面法線にforward、面内方向にrightを用い、Transform回転へ追従させる。
            data.normal = forward;
            data.tangent = right;
            data.width = std::max(areaSize.x, 0.0f);
            data.height = std::max(areaSize.y, 0.0f);
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

   if (!ImGui::CollapsingHeader(header.c_str())) return;

   const char* types[] = {"Directional", "Point", "Spot", "Area"};
   std::int32_t selectedType = static_cast<std::int32_t>(type_);

   if (ImGui::Combo("Type", &selectedType, types, IM_ARRAYSIZE(types))) {
      SetLightType(static_cast<Type>(selectedType));
   }

   ImGui::ColorEdit4("Color", &color.x);
   ImGui::DragFloat("Intensity", &intensity, 0.05f, 0.0f, 1000.0f);

   if (type_ == Type::Point) {
      ImGui::DragFloat("Radius", &radius, 0.05f, 0.0f, 10000.0f);
      ImGui::DragFloat("Decay", &decay, 0.01f, 0.0f, 100.0f);
   } else if (type_ == Type::Spot) {
      ImGui::DragFloat("Distance", &distance, 0.05f, 0.0f, 10000.0f);
      ImGui::DragFloat("Decay", &decay, 0.01f, 0.0f, 100.0f);
      ImGui::SliderFloat("Cos Angle", &cosAngle, -1.0f, 1.0f);
      ImGui::SliderFloat("Cos Falloff Start", &cosFalloffStart, -1.0f, 1.0f);
   } else if (type_ == Type::Area) {
      ImGui::DragFloat2("Size", &areaSize.x, 0.05f, 0.0f, 10000.0f);
   }

   // Edit停止中でもInspector操作の結果を同じフレームのViewportへ反映する。
   SynchronizeRuntimeLight();
   Configure(DescribeSettings());
}
#endif

} // namespace GameEngine

namespace GameEngine {

LightComponent::Settings LightComponent::DescribeSettings() const {
   Settings settings;
   settings.color = color;
   settings.intensity = intensity;
   settings.radius = radius;
   settings.decay = decay;
   settings.distance = distance;
   settings.cosAngle = cosAngle;
   settings.cosFalloffStart = cosFalloffStart;
   settings.areaSize = areaSize;
   return settings;
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

   color = settings.color;
   intensity = settings.intensity;
   radius = settings.radius;
   decay = settings.decay;
   distance = settings.distance;
   cosAngle = settings.cosAngle;
   cosFalloffStart = settings.cosFalloffStart;
   areaSize = settings.areaSize;
}

} // namespace GameEngine
