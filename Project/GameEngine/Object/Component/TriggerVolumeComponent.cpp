#include "pch.h"
#include "TriggerVolumeComponent.h"

#include "ComponentRegistry.h"
#include "Framework/EngineContext.h"
#include "Object/Object.h"
#include "Scene/SceneWorld.h"
#include "TransformComponent.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "Utility/ImGuiHelper.h"
#include <imgui.h>
#endif

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::TriggerVolumeComponent::kTypeName,
   [](GameEngine::Object& object) -> GameEngine::IObjectComponent* {
      return object.AddComponent<GameEngine::TriggerVolumeComponent>();
   },
   GameEngine::TriggerVolumeComponent::kDisplayName,
   GameEngine::ObjectType::Generic | GameEngine::ObjectType::Model | GameEngine::ObjectType::Sprite);
#ifdef USE_IMGUI
constexpr float kInspectorColumnWidth = 140.0f;
#endif
}

namespace GameEngine {

void TriggerVolumeComponent::OnSceneLoaded(SceneWorld& sceneWorld) {
   sceneWorld_ = &sceneWorld;
   isInside_ = false;
   enteredThisFrame_ = false;
   exitedThisFrame_ = false;
}

void TriggerVolumeComponent::Update(float deltaTime) {
   (void)deltaTime;
   enteredThisFrame_ = false;
   exitedThisFrame_ = false;

   Object* target = ResolveTarget();
   const bool overlaps = target && CalculateOverlap(*target);
   // 前フレームの状態との差分から、1フレームだけ有効なEnter/Exitイベントを作る。
   enteredThisFrame_ = overlaps && !isInside_;
   exitedThisFrame_ = !overlaps && isInside_;
   isInside_ = overlaps;

   if (debugDraw_ && HasOwner()) {
      if (const auto* transform = GetOwner().GetComponent<TransformComponent>()) {
         const Vector3 center = transform->transform.translation + centerOffset_;
         const float debugRadius = shape_ == Shape::Sphere
            ? radius_
            : std::max({ halfExtents_.x, halfExtents_.y, halfExtents_.z });
         const Vector4 color = isInside_
            ? Vector4(0.2f, 1.0f, 0.2f, 1.0f)
            : Vector4(1.0f, 0.8f, 0.1f, 1.0f);
         EngineContext::DrawSphere(center, debugRadius, color, false);
      }
   }
}

nlohmann::json TriggerVolumeComponent::Serialize() const {
   return nlohmann::json{
      { "targetObjectId", targetObjectId_ },
      { "shape", shape_ == Shape::Sphere ? "Sphere" : "AABB" },
      { "centerOffset", { centerOffset_.x, centerOffset_.y, centerOffset_.z } },
      { "halfExtents", { halfExtents_.x, halfExtents_.y, halfExtents_.z } },
      { "radius", radius_ },
      { "debugDraw", debugDraw_ }
   };
}

void TriggerVolumeComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   if (data.contains("targetObjectId") && data.at("targetObjectId").is_string()) {
      targetObjectId_ = data.at("targetObjectId").get<std::string>();
   }
   if (data.contains("shape") && data.at("shape").is_string()) {
      shape_ = data.at("shape").get<std::string>() == "Sphere" ? Shape::Sphere : Shape::Aabb;
   }

   auto readVector = [&data](const char* key, Vector3& value) {
      if (data.contains(key) && data.at(key).is_array() && data.at(key).size() == 3) {
         value = Vector3(
            data.at(key)[0].get<float>(),
            data.at(key)[1].get<float>(),
            data.at(key)[2].get<float>());
      }
   };
   readVector("centerOffset", centerOffset_);
   readVector("halfExtents", halfExtents_);
   if (data.contains("radius") && data.at("radius").is_number()) {
      radius_ = std::max(data.at("radius").get<float>(), 0.0f);
   }
   if (data.contains("debugDraw") && data.at("debugDraw").is_boolean()) {
      debugDraw_ = data.at("debugDraw").get<bool>();
   }
}

Object* TriggerVolumeComponent::ResolveTarget() const {
   return sceneWorld_ ? sceneWorld_->FindObjectById(targetObjectId_) : nullptr;
}

bool TriggerVolumeComponent::CalculateOverlap(const Object& target) const {
   if (!HasOwner()) {
      return false;
   }
   const auto* ownerTransform = GetOwner().GetComponent<TransformComponent>();
   const auto* targetTransform = target.GetComponent<TransformComponent>();
   if (!ownerTransform || !targetTransform) {
      return false;
   }

   const Vector3 center = ownerTransform->transform.translation + centerOffset_;
   const Vector3 targetPosition = targetTransform->transform.translation;
   // 現在は対象の原点を点として判定し、対象側の描画形状には依存させない。
   const Vector3 difference = targetPosition - center;
   if (shape_ == Shape::Sphere) {
      return difference.LengthSquared() <= radius_ * radius_;
   }

   return std::abs(difference.x) <= halfExtents_.x &&
      std::abs(difference.y) <= halfExtents_.y &&
      std::abs(difference.z) <= halfExtents_.z;
}

#ifdef USE_IMGUI
void TriggerVolumeComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGuiHelper::DrawInputString(
      "Target Object ID",
      targetObjectId_,
      ImGuiHelper::kDefaultTextBufferSize,
      kInspectorColumnWidth);
   const char* shapes[] = { "Sphere", "AABB" };
   int shapeIndex = shape_ == Shape::Sphere ? 0 : 1;
   if (ImGui::Combo("Shape", &shapeIndex, shapes, 2)) {
      shape_ = shapeIndex == 0 ? Shape::Sphere : Shape::Aabb;
   }
   ImGui::DragFloat3("Center Offset", &centerOffset_.x, 0.1f);
   if (shape_ == Shape::Sphere) {
      ImGui::DragFloat("Radius", &radius_, 0.1f, 0.0f, 10000.0f);
   } else {
      ImGui::DragFloat3("Half Extents", &halfExtents_.x, 0.1f, 0.0f, 10000.0f);
   }
   ImGui::Checkbox("Debug Draw", &debugDraw_);
   ImGui::Text("Inside: %s", isInside_ ? "true" : "false");
}
#endif

} // namespace GameEngine
