#include "pch.h"
#include "UIModelComponent.h"

#include "Component/ComponentRegistry.h"
#include "Component/RenderComponent.h"
#include "Component/TransformComponent.h"
#include "Object.h"
#include "Scene/Camera/Camera.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "imgui.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace {
// 3D Model を UI として扱う付加コンポーネント。通常の Sprite／Text には適用しないため Model に限定する。
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::UIModelComponent::kTypeName,
   [](GameEngine::Object& object) -> GameEngine::IObjectComponent* {
      return object.AddComponent<GameEngine::UIModelComponent>();
   },
   GameEngine::UIModelComponent::kDisplayName,
   GameEngine::ToObjectTypeMask(GameEngine::ObjectType::Model));

const char* ToProjectionTypeName(GameEngine::UIModelComponent::ProjectionType projectionType) {
   // 列挙値ではなく名前を永続化し、JSON を手編集しやすくするとともに値順の変更から保存形式を守る。
   return projectionType == GameEngine::UIModelComponent::ProjectionType::Perspective
      ? "Perspective"
      : "Orthographic";
}

const char* ToAnchorPointName(GameEngine::UIModelComponent::AnchorPoint anchorPoint) {
   using AnchorPoint = GameEngine::UIModelComponent::AnchorPoint;
   switch (anchorPoint) {
      case AnchorPoint::TopLeft: return "TopLeft";
      case AnchorPoint::TopCenter: return "TopCenter";
      case AnchorPoint::TopRight: return "TopRight";
      case AnchorPoint::MiddleLeft: return "MiddleLeft";
      case AnchorPoint::MiddleRight: return "MiddleRight";
      case AnchorPoint::BottomLeft: return "BottomLeft";
      case AnchorPoint::BottomCenter: return "BottomCenter";
      case AnchorPoint::BottomRight: return "BottomRight";
      case AnchorPoint::MiddleCenter:
      default: return "MiddleCenter";
   }
}

GameEngine::Vector2 GetAnchorDirection(GameEngine::UIModelComponent::AnchorPoint anchorPoint) {
   // 画面中心を原点とするため、各アンカーを -1／0／1 の方向係数へ変換する。
   // 実際の位置は解像度の半分を掛けて算出し、解像度変更にも追従させる。
   using AnchorPoint = GameEngine::UIModelComponent::AnchorPoint;
   switch (anchorPoint) {
      case AnchorPoint::TopLeft: return { -1.0f, 1.0f };
      case AnchorPoint::TopCenter: return { 0.0f, 1.0f };
      case AnchorPoint::TopRight: return { 1.0f, 1.0f };
      case AnchorPoint::MiddleLeft: return { -1.0f, 0.0f };
      case AnchorPoint::MiddleRight: return { 1.0f, 0.0f };
      case AnchorPoint::BottomLeft: return { -1.0f, -1.0f };
      case AnchorPoint::BottomCenter: return { 0.0f, -1.0f };
      case AnchorPoint::BottomRight: return { 1.0f, -1.0f };
      case AnchorPoint::MiddleCenter:
      default: return { 0.0f, 0.0f };
   }
}
}

namespace GameEngine {

const char* UIModelComponent::GetTypeName() const {
   return kTypeName;
}

void UIModelComponent::OnAttach() {
   if (auto* renderComponent = GetOwner().GetComponent<RenderComponent>()) {
      // UI モデルは専用カメラの画面空間パスへ送り、ゲーム画面向けポストエフェクトの影響を受けないようにする。
      renderComponent->renderSpace = RenderComponent::RenderSpace::Screen;
      renderComponent->applyPostProcess = false;
   }
}

void UIModelComponent::ApplyLayout(const Camera& camera, uint32_t screenWidth, uint32_t screenHeight) {
   auto* transformComponent = GetOwner().GetComponent<TransformComponent>();
   if (!transformComponent || screenWidth == 0 || screenHeight == 0) {
      return;
   }

   // 画面サイズが変わるたびにアンカーを再計算し、固定ピクセルのオフセットだけを設定値として保持する。
   const Vector2 anchorDirection = GetAnchorDirection(anchorPoint);
   const float width = static_cast<float>(screenWidth);
   const float height = static_cast<float>(screenHeight);
   // クリップ面と同じ深度では数値誤差で欠けるため、両面から小さな余白を取った可視範囲へ収める。
   const float safeDepth = std::clamp(depth, camera.GetNearClip() + 0.001f, camera.GetFarClip() - 0.001f);
   Vector3 anchorPosition{};

   if (projectionType == ProjectionType::Perspective) {
      const float halfHeight = safeDepth * std::tan(camera.GetFovY() * 0.5f);
      const float halfWidth = halfHeight * camera.GetAspectRatio();
      // 基準UI領域が視野内に収まる一様倍率で、アンカーとオフセットを同じ比率のまま投影する。
      const float worldUnitsPerUiPixel = std::min(
         2.0f * halfWidth / width,
         2.0f * halfHeight / height);
      anchorPosition = {
         (anchorDirection.x * width * 0.5f + screenOffset.x) * worldUnitsPerUiPixel,
         (anchorDirection.y * height * 0.5f + screenOffset.y) * worldUnitsPerUiPixel,
         safeDepth
      };
   } else {
      // 平行投影では 1 UI ピクセルを 1 カメラ空間単位として扱い、解像度ベースの配置をそのまま使う。
      anchorPosition = {
         anchorDirection.x * width * 0.5f + screenOffset.x,
         anchorDirection.y * height * 0.5f + screenOffset.y,
         safeDepth
      };
   }

   Transform& transform = transformComponent->transform;
   const Vector3 scaledPivot{
      localPivot.x * transform.scale.x,
      localPivot.y * transform.scale.y,
      localPivot.z * transform.scale.z
   };
   // ピボットの回転後位置を引き、モデルの向きに依らず同じアンカーへ揃える。
   const Vector3 rotatedPivot = RotateVector(scaledPivot, transform.GetActiveQuaternion());
   transform.translation = anchorPosition - rotatedPivot;
}

nlohmann::json UIModelComponent::Serialize() const {
   return nlohmann::json{
      { "projectionType", ToProjectionTypeName(projectionType) },
      { "anchorPoint", ToAnchorPointName(anchorPoint) },
      { "screenOffset", { screenOffset.x, screenOffset.y } },
      { "localPivot", { localPivot.x, localPivot.y, localPivot.z } },
      { "depth", depth }
   };
}

void UIModelComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   // キー単位の部分復元により、追加前の古いシーンではクラス既定値をそのまま利用する。
   if (data.contains("projectionType") && data.at("projectionType").is_string()) {
      projectionType = data.at("projectionType").get<std::string>() == "Perspective"
         ? ProjectionType::Perspective
         : ProjectionType::Orthographic;
   }
   if (data.contains("anchorPoint") && data.at("anchorPoint").is_string()) {
      const std::string value = data.at("anchorPoint").get<std::string>();
      if (value == "TopLeft") anchorPoint = AnchorPoint::TopLeft;
      else if (value == "TopCenter") anchorPoint = AnchorPoint::TopCenter;
      else if (value == "TopRight") anchorPoint = AnchorPoint::TopRight;
      else if (value == "MiddleLeft") anchorPoint = AnchorPoint::MiddleLeft;
      else if (value == "MiddleRight") anchorPoint = AnchorPoint::MiddleRight;
      else if (value == "BottomLeft") anchorPoint = AnchorPoint::BottomLeft;
      else if (value == "BottomCenter") anchorPoint = AnchorPoint::BottomCenter;
      else if (value == "BottomRight") anchorPoint = AnchorPoint::BottomRight;
      else anchorPoint = AnchorPoint::MiddleCenter;
   }
   if (data.contains("screenOffset") && data.at("screenOffset").is_array() && data.at("screenOffset").size() >= 2) {
      screenOffset = { data.at("screenOffset")[0].get<float>(), data.at("screenOffset")[1].get<float>() };
   }
   if (data.contains("localPivot") && data.at("localPivot").is_array() && data.at("localPivot").size() >= 3) {
      localPivot = {
         data.at("localPivot")[0].get<float>(),
         data.at("localPivot")[1].get<float>(),
         data.at("localPivot")[2].get<float>()
      };
   }
   if (data.contains("depth") && data.at("depth").is_number()) {
      // 実際の near／far への制限はカメラ依存なので ApplyLayout に任せ、ここでは正の距離だけ保証する。
      depth = std::max(data.at("depth").get<float>(), 0.001f);
   }
}

#ifdef USE_IMGUI
void UIModelComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }

   int projectionIndex = static_cast<int>(projectionType);
   const char* projectionItems[] = {
      ImGuiHelper::Localize({ "平行投影", "Orthographic" }),
      ImGuiHelper::Localize({ "透視投影", "Perspective" })
   };
   if (ImGui::Combo(ImGuiHelper::Localize({ "投影方式", "Projection" }), &projectionIndex, projectionItems, 2)) {
      projectionType = static_cast<ProjectionType>(projectionIndex);
   }

   int anchorIndex = static_cast<int>(anchorPoint);
   const char* anchorItems[] = {
      "Top Left", "Top Center", "Top Right",
      "Middle Left", "Middle Center", "Middle Right",
      "Bottom Left", "Bottom Center", "Bottom Right"
   };
   if (ImGui::Combo(ImGuiHelper::Localize({ "アンカー", "Anchor" }), &anchorIndex, anchorItems, 9)) {
      anchorPoint = static_cast<AnchorPoint>(anchorIndex);
   }
   ImGui::DragFloat2(ImGuiHelper::Localize({ "画面オフセット", "Screen Offset" }), &screenOffset.x, 1.0f);
   ImGui::DragFloat3(ImGuiHelper::Localize({ "ローカルピボット", "Local Pivot" }), &localPivot.x, 0.01f);
   ImGui::DragFloat(ImGuiHelper::Localize({ "奥行き", "Depth" }), &depth, 0.01f, 0.001f, 100.0f);
}
#endif

} // namespace GameEngine
