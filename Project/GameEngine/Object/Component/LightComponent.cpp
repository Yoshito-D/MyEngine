#include "pch.h"
#include "LightComponent.h"

#include "ComponentRegistry.h"
#include "Framework/EngineContext.h"
#include "Object.h"
#include "TransformComponent.h"
#include "Scene/Light/AreaLight.h"
#include "Scene/Light/DirectionalLight.h"
#include "Scene/Light/PointLight.h"
#include "Scene/Light/SpotLight.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include "Utility/MathUtils/VectorOperations.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "Utility/ImGuiHelper.h"
#include <imgui.h>
#endif

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::LightComponent::kTypeName,
   [](GameEngine::Object& object) -> GameEngine::IObjectComponent* {
      if (!object.HasComponent<GameEngine::TransformComponent>()) {
         object.AddComponent<GameEngine::TransformComponent>();
      }
      return object.AddComponent<GameEngine::LightComponent>();
   },
   GameEngine::LightComponent::kDisplayName,
   GameEngine::ObjectType::Generic | GameEngine::ObjectType::Model);

GameEngine::Vector3 ReadVector3(
   const nlohmann::json& data,
   const char* key,
   const GameEngine::Vector3& fallback) {
   if (!data.contains(key) || !data.at(key).is_array() || data.at(key).size() != 3) {
      return fallback;
   }
   return {
      data.at(key)[0].get<float>(),
      data.at(key)[1].get<float>(),
      data.at(key)[2].get<float>()
   };
}

GameEngine::Vector4 ReadVector4(
   const nlohmann::json& data,
   const char* key,
   const GameEngine::Vector4& fallback) {
   if (!data.contains(key) || !data.at(key).is_array() || data.at(key).size() != 4) {
      return fallback;
   }
   return {
      data.at(key)[0].get<float>(),
      data.at(key)[1].get<float>(),
      data.at(key)[2].get<float>(),
      data.at(key)[3].get<float>()
   };
}

GameEngine::Vector2 ReadVector2(
   const nlohmann::json& data,
   const char* key,
   const GameEngine::Vector2& fallback) {
   if (!data.contains(key) || !data.at(key).is_array() || data.at(key).size() != 2) {
      return fallback;
   }
   return {
      data.at(key)[0].get<float>(),
      data.at(key)[1].get<float>()
   };
}

float ReadFloat(const nlohmann::json& data, const char* key, float fallback) {
   return data.contains(key) && data.at(key).is_number()
      ? data.at(key).get<float>()
      : fallback;
}
}

namespace GameEngine {

void LightComponent::SetLightType(Type type) {
   if (type_ == type) {
      return;
   }
   ReleaseRuntimeLight();
   type_ = type;
}

bool LightComponent::DeserializeLegacy(const nlohmann::json& data) {
   if (!data.is_object()) {
      return false;
   }

   Type parsedType;
   if (!TryParseType(data.value("type", ""), parsedType)) {
      return false;
   }

   SetLightType(parsedType);
   color = ReadVector4(data, "color", color);
   intensity = ReadFloat(data, "intensity", intensity);
   radius = ReadFloat(data, "radius", radius);
   decay = ReadFloat(data, "decay", decay);
   distance = ReadFloat(data, "distance", distance);
   cosAngle = ReadFloat(data, "cosAngle", cosAngle);
   cosFalloffStart = ReadFloat(data, "cosFalloffStart", cosFalloffStart);
   areaSize = ReadVector2(data, "size", areaSize);

   if (auto* transform = GetOwner().GetComponent<TransformComponent>()) {
      transform->transform.translation = ReadVector3(data, "position", transform->transform.translation);
      const Vector3 direction = type_ == Type::Area
         ? ReadVector3(data, "normal", Vector3(0.0f, -1.0f, 0.0f))
         : ReadVector3(data, "direction", Vector3(0.0f, -1.0f, 0.0f));
      if (type_ == Type::Directional || type_ == Type::Spot || type_ == Type::Area) {
         transform->transform.SetRotationQuaternion(LookRotation(direction, Vector3(0.0f, 1.0f, 0.0f)));
      }
   }
   return true;
}

void LightComponent::Update(float deltaTime) {
   (void)deltaTime;
   SynchronizeRuntimeLight();
}

void LightComponent::OnDisable() {
   ReleaseRuntimeLight();
}

void LightComponent::OnEnable() {
   SynchronizeRuntimeLight();
}

void LightComponent::OnDetach() {
   ReleaseRuntimeLight();
}

nlohmann::json LightComponent::Serialize() const {
   return nlohmann::json{
      { "lightType", TypeToString(type_) },
      { "color", { color.x, color.y, color.z, color.w } },
      { "intensity", intensity },
      { "radius", radius },
      { "decay", decay },
      { "distance", distance },
      { "cosAngle", cosAngle },
      { "cosFalloffStart", cosFalloffStart },
      { "areaSize", { areaSize.x, areaSize.y } }
   };
}

void LightComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }

   Type parsedType = type_;
   if (TryParseType(data.value("lightType", data.value("type", "")), parsedType)) {
      SetLightType(parsedType);
   }
   color = ReadVector4(data, "color", color);
   intensity = ReadFloat(data, "intensity", intensity);
   radius = ReadFloat(data, "radius", radius);
   decay = ReadFloat(data, "decay", decay);
   distance = ReadFloat(data, "distance", distance);
   cosAngle = ReadFloat(data, "cosAngle", cosAngle);
   cosFalloffStart = ReadFloat(data, "cosFalloffStart", cosFalloffStart);
   areaSize = ReadVector2(data, "areaSize", ReadVector2(data, "size", areaSize));
}

void LightComponent::SynchronizeRuntimeLight() {
   if (!HasOwner() || !IsEnabled()) {
      return;
   }

   const std::string runtimeKey = ResolveRuntimeKey();
   if (runtimeKey.empty()) {
      return;
   }
   if (!runtimeLightKey_.empty() &&
      (runtimeLightKey_ != runtimeKey || registeredType_ != type_)) {
      ReleaseRuntimeLight();
   }

   const Matrix4x4 worldMatrix = GetOwner().GetWorldMatrix();
   const Vector3 position(worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2]);
   const Vector3 forward = TransformNormal(Vector3(0.0f, 0.0f, 1.0f), worldMatrix).Normalize();
   const Vector3 right = TransformNormal(Vector3(1.0f, 0.0f, 0.0f), worldMatrix).Normalize();

   runtimeLightKey_ = runtimeKey;
   registeredType_ = type_;
   switch (type_) {
   case Type::Directional: {
      auto* light = EngineContext::GetDirectionalLight(runtimeKey);
      if (!light) {
         light = EngineContext::CreateDirectionalLight(runtimeKey);
      }
      if (light && light->GetDirectionalLightData()) {
         auto& data = *light->GetDirectionalLightData();
         data.color = color;
         data.direction = forward;
         data.intensity = intensity;
      }
      break;
   }
   case Type::Point: {
      auto* light = EngineContext::GetPointLight(runtimeKey);
      if (!light) {
         light = EngineContext::CreatePointLight(runtimeKey);
      }
      if (light && light->GetPointLightData()) {
         auto& data = *light->GetPointLightData();
         data.color = color;
         data.position = position;
         data.intensity = intensity;
         data.radius = std::max(radius, 0.0f);
         data.decay = std::max(decay, 0.0f);
      }
      break;
   }
   case Type::Spot: {
      auto* light = EngineContext::GetSpotLight(runtimeKey);
      if (!light) {
         light = EngineContext::CreateSpotLight(runtimeKey);
      }
      if (light && light->GetSpotLightData()) {
         auto& data = *light->GetSpotLightData();
         data.color = color;
         data.position = position;
         data.intensity = intensity;
         data.direction = forward;
         data.distance = std::max(distance, 0.0f);
         data.decay = std::max(decay, 0.0f);
         data.cosAngle = cosAngle;
         data.cosFalloffStart = cosFalloffStart;
      }
      break;
   }
   case Type::Area: {
      auto* light = EngineContext::GetAreaLight(runtimeKey);
      if (!light) {
         light = EngineContext::CreateAreaLight(runtimeKey);
      }
      if (light && light->GetAreaLightData()) {
         auto& data = *light->GetAreaLightData();
         data.color = color;
         data.position = position;
         data.intensity = intensity;
         data.normal = forward;
         data.tangent = right;
         data.width = std::max(areaSize.x, 0.0f);
         data.height = std::max(areaSize.y, 0.0f);
      }
      break;
   }
   }
}

void LightComponent::ReleaseRuntimeLight() {
   if (runtimeLightKey_.empty()) {
      return;
   }

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
   case Type::Directional: return "directional";
   case Type::Point: return "point";
   case Type::Spot: return "spot";
   case Type::Area: return "area";
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
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }

   const char* types[] = { "Directional", "Point", "Spot", "Area" };
   int selectedType = static_cast<int>(type_);
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

   SynchronizeRuntimeLight();
}
#endif

} // namespace GameEngine
