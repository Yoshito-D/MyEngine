#include "GameProject/App/Component/Race/UI/RaceGoalDirectionHUDComponent.h"

#include "GameEngine/Assets/Model/ModelAsset.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Math/Functions/MatrixOperations.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include "GameEngine/Math/Functions/VectorOperations.h"
#include "GameEngine/Object/Component/Rendering/MeshComponent.h"
#include "GameEngine/Object/Component/Rendering/RenderComponent.h"
#include "GameEngine/Object/Component/UI/UIModelComponent.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Scene/Camera/Camera.h"
#include "GameEngine/Utility/Logger.h"
#include <algorithm>
#include <cmath>

namespace {

constexpr float kDirectionEpsilonSquared = 1.0e-12f; ///< 方向を定義できる変位の下限の二乗（m²）。
constexpr float kCaptionGapRatio = 0.25f;            ///< 文字と矢印の間隔の行高に対する比率。

float MeasureArrowDiameter(const GameEngine::ModelAsset& asset, const GameEngine::Vector3& pivot) {
   const auto& bounds = asset.GetLocalBounds();
   const auto meshToModel = asset.HasSkinningData() ? GameEngine::MakeIdentity4x4() : asset.GetRootNode().localMatrix;
   float radiusSquared = 0.f;

   // UIの回転中心から全ての境界頂点を囲む球を求め、回転中も文字との間隔を保つ。
   for (const float x : {bounds.min.x, bounds.max.x}) {
      for (const float y : {bounds.min.y, bounds.max.y}) {
         for (const float z : {bounds.min.z, bounds.max.z}) {
            const auto corner = GameEngine::TransformPosition(meshToModel, {x, y, z});
            radiusSquared = std::max(radiusSquared, (corner - pivot).LengthSquared());
         }
      }
   }

   return 2.f * std::sqrt(radiusSquared);
}

} // namespace

namespace App {

void RaceGoalDirectionHUDComponent::ShowBesideDistance(const GameEngine::Camera* camera,
   const GameEngine::Vector3& directionToGoal, GameEngine::Vector2 captionOffset, std::string_view caption,
   const GameEngine::TextStyle& style) {
   if (!IsEnabled() || !camera || directionToGoal.LengthSquared() < kDirectionEpsilonSquared) {
      Hide();
      return;
   }

   auto* arrow = HasOwner() ? dynamic_cast<GameEngine::Model*>(&GetOwner()) : nullptr;
   auto* uiModel = arrow ? arrow->GetComponent<GameEngine::UIModelComponent>() : nullptr;
   auto* render = arrow ? arrow->GetComponent<GameEngine::RenderComponent>() : nullptr;
   const auto* mesh = arrow ? arrow->GetComponent<GameEngine::MeshComponent>() : nullptr;
   const auto* asset = mesh ? mesh->GetModelAsset() : nullptr;

   if (!uiModel || !render || !asset) {
      Logger::Fail("Goal direction HUD requires a loaded model with UIModelComponent and RenderComponent",
         Logger::LogChannel::Game);
   }

   // UIカメラは回転しないので、平行移動を除いたゲームカメラ空間で矢印の向きを求める。
   const auto direction = GameEngine::TransformNormal(directionToGoal.Normalize(), camera->GetViewMatrix()).Normalize();
   const auto textSize = GameEngine::EngineContext::MeasureText(caption, style);

   if (direction.LengthSquared() <= 0.f || textSize.y <= 0.f) {
      Hide();
      return;
   }

   auto layout = uiModel->DescribeSettings();
   const float diameter = MeasureArrowDiameter(*asset, layout.localPivot);

   if (!std::isfinite(diameter) || diameter <= 0.f) {
      Logger::Fail("Goal direction HUD: invalid model bounds for " + arrow->GetEntityId(), Logger::LogChannel::Game);
   }

   // モデルを包む球の直径を行高に合わせ、矢印の向きが変わっても表示倍率を一定に保つ。
   const float scale = textSize.y / diameter;
   arrow->SetRotationQuaternion(GameEngine::LookRotation(direction));
   arrow->SetScale({scale, scale, scale});

   layout.projectionType = GameEngine::UIModelComponent::ProjectionType::Orthographic;
   layout.anchorPoint = GameEngine::UIModelComponent::AnchorPoint::BottomRight;
   layout.screenOffset = {captionOffset.x - textSize.x * 0.5f - textSize.y * (0.5f + kCaptionGapRatio),
      -captionOffset.y + textSize.y * 0.5f};
   layout.depth = textSize.y;
   uiModel->Configure(layout);
   uiModel->SetEnabled(true);
   auto renderSettings = render->DescribeSettings();
   renderSettings.renderSpace = GameEngine::RenderComponent::RenderSpace::Screen;
   renderSettings.applyPostProcess = false;
   renderSettings.visible = true;
   render->Configure(renderSettings);
}

void RaceGoalDirectionHUDComponent::Hide() {
   SetVisible(false);
}

void RaceGoalDirectionHUDComponent::OnDisable() {
   Hide();
}

nlohmann::json RaceGoalDirectionHUDComponent::Serialize() const {
   return nlohmann::json::object();
}

void RaceGoalDirectionHUDComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object() || !data.empty()) {
      Logger::Fail("Goal direction HUD expects an empty settings object", Logger::LogChannel::Game);
   }
}

void RaceGoalDirectionHUDComponent::SetVisible(bool visible) {
   auto* render = HasOwner() ? GetOwner().GetComponent<GameEngine::RenderComponent>() : nullptr;

   if (!render) return;

   auto settings = render->DescribeSettings();

   if (settings.visible != visible) {
      settings.visible = visible;
      render->Configure(settings);
   }
}

} // namespace App
