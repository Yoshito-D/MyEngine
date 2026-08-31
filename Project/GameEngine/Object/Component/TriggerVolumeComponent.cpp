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
// 翻訳単位の読込時にFactoryを登録し、JSON復元とInspector追加の双方が具象型を直接知らずに生成できるようにする。
// 戻り値自体は使わないが、kRegisteredの初期化が登録処理を確実に実行する。
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
   // 全Objectの復元後に通知されるSceneWorldを非所有で保持し、ID参照をデシリアライズ順に依存させない。
   // 再読込直後に旧シーンの接触状態からEnter/Exitが発生しないよう、一時状態もここで初期化する。
   sceneWorld_ = &sceneWorld;
   isInside_ = false;
   enteredThisFrame_ = false;
   exitedThisFrame_ = false;
}

void TriggerVolumeComponent::Update(float deltaTime) {
   (void)deltaTime;
   // Enter/Exitは状態ではなく1フレームだけのエッジなので、対象が消えたフレームも含め毎回先に落とす。
   enteredThisFrame_ = false;
   exitedThisFrame_ = false;

   // Scene内Objectは編集・再読込で差し替わり得るため、生ポインターをキャッシュせず安定IDから毎フレーム解決する。
   Object* target = ResolveTarget();
   const bool overlaps = target && CalculateOverlap(*target);
   // 前フレームの状態との差分から、1フレームだけ有効なEnter/Exitイベントを作る。
   enteredThisFrame_ = overlaps && !isInside_;
   exitedThisFrame_ = !overlaps && isInside_;
   isInside_ = overlaps;

   if (debugDraw_ && HasOwner()) {
      if (const auto* transform = GetOwner().GetComponent<TransformComponent>()) {
         // 判定と同じくOffsetはOwnerの平行移動へ直接加算し、回転・スケールは適用しない。
         const Vector3 center = transform->transform.translation + centerOffset_;
         // DrawSphereでAABBの厳密な輪郭は描けないため、最大半幅を形状サイズの目安として可視化する。
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
   // enum値ではなく名前を保存して列挙順の変更に耐えさせ、フレーム依存の接触状態は次回ロードへ持ち越さない。
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
   // 各項目を独立した部分更新として読み、旧データで欠けている設定は現在値を維持する。
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
      // 負の半径は幾何学的な意味を持たず、二乗すると正値として誤判定するため0へ制限する。
      radius_ = std::max(data.at("radius").get<float>(), 0.0f);
   }
   if (data.contains("debugDraw") && data.at("debugDraw").is_boolean()) {
      debugDraw_ = data.at("debugDraw").get<bool>();
   }
}

Object* TriggerVolumeComponent::ResolveTarget() const {
   // SceneWorldが未通知、IDが空、または対象が削除済みなら未解決として扱い、Update側でExitへ遷移させる。
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

   // 現仕様は両Objectのローカルtranslationを直接比較し、親行列・回転・スケールを合成しない。Offsetも同じ座標成分へ加算する。
   const Vector3 center = ownerTransform->transform.translation + centerOffset_;
   const Vector3 targetPosition = targetTransform->transform.translation;
   // 現在は対象の原点を点として判定し、対象側の描画形状には依存させない。
   const Vector3 difference = targetPosition - center;
   if (shape_ == Shape::Sphere) {
      // 平方根を取らず半径の2乗と比較し、境界上もInsideとして扱う。
      return difference.LengthSquared() <= radius_ * radius_;
   }

   // AABBはtranslationの各座標軸に平行で、halfExtentsの各成分が非負であることを前提とする。
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
