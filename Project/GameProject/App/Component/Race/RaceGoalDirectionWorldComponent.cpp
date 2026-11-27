#include "GameProject/App/Component/Race/RaceGoalDirectionWorldComponent.h"

#include "GameEngine/Assets/Model/ModelAsset.h"
#include "GameEngine/Math/Functions/MatrixOperations.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include "GameEngine/Math/Functions/VectorOperations.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Component/Rendering/MeshComponent.h"
#include "GameEngine/Object/Component/Rendering/RenderComponent.h"
#include "GameEngine/Object/Component/UI/UIModelComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/Camera/Camera.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Utility/Logger.h"
#include "GameProject/App/Component/Race/RaceManagerComponent.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace {

constexpr float kMinGoalDistanceSquared = 1.0e-12f; ///< 方向を定義するための距離の下限の二乗（m²）。
constexpr GameEngine::Vector3 kLocalCameraUp{0.f, 1.f, 0.f};
constexpr float kScreenHeightGapRatio = 0.01f; ///< 車体と矢印の間隔の画面高に対する比率。

GameEngine::Vector3 GetTranslation(const GameEngine::Matrix4x4& world) {
   return {world.m[3][0], world.m[3][1], world.m[3][2]};
}

const GameEngine::ModelAsset* GetModelAsset(const GameEngine::Object& object) {
   const auto* mesh = object.GetComponent<GameEngine::MeshComponent>();
   return mesh ? mesh->GetModelAsset() : nullptr;
}

template <class Visit>
bool VisitProjectedBounds(const GameEngine::ModelAsset& model, const GameEngine::Matrix4x4& objectToClip,
   Visit visit) {
   const auto& box = model.GetLocalBounds();
   const auto meshToClip = model.HasSkinningData() ? objectToClip : model.GetRootNode().localMatrix * objectToClip;
   constexpr auto kBoxAxes = std::array{1u, 2u, 4u};
   std::array<GameEngine::Vector4, 8> corners{};
   bool valid = false;
   const auto emit = [&](const GameEngine::Vector4& point) {
      if (point.w > 0.f && std::isfinite(point.w) && std::isfinite(point.y)) {
         visit(point);
         valid = true;
      }
   };

   for (std::size_t i = 0; i < corners.size(); ++i) {
      corners[i] = GameEngine::TransformVectorByMatrix(
         {(i & kBoxAxes[0]) ? box.max.x : box.min.x, (i & kBoxAxes[1]) ? box.max.y : box.min.y,
            (i & kBoxAxes[2]) ? box.max.z : box.min.z, 1.f}, meshToClip);

      if (corners[i].z >= 0.f) {
         emit(corners[i]);
      }
   }

   // 近平面をまたぐ場合は交点も調べ、透視除算で画面上の範囲を過小評価しない。
   for (const auto axis : kBoxAxes) {
      for (std::size_t i = 0; i < corners.size(); ++i) {
         if (i & axis) continue;

         const auto& a = corners[i];
         const auto& b = corners[i | axis];

         if ((a.z < 0.f) != (b.z < 0.f)) {
            emit(a + (b - a) * (a.z / (a.z - b.z)));
         }
      }
   }

   return valid;
}

std::optional<float> GetAdditionalHeight(const GameEngine::ModelAsset& vehicle, const GameEngine::ModelAsset& arrow,
   const GameEngine::Matrix4x4& viewProjection, const GameEngine::Matrix4x4& vehicleWorld,
   const GameEngine::Transform& arrowPose, const GameEngine::Vector3& cameraUp) {
   float vehicleTop = -std::numeric_limits<float>::infinity();
   const bool vehicleVisible = VisitProjectedBounds(vehicle, vehicleWorld * viewProjection,
      [&](const auto& point) { vehicleTop = std::max(vehicleTop, point.y / point.w); });
   const float targetY = vehicleTop + 2.f * kScreenHeightGapRatio;

   if (!vehicleVisible || !std::isfinite(targetY) || targetY >= 1.f) return std::nullopt;

   // カメラの上へ移動しても奥行きとwは変化しないため、全頂点で必要な高さの最大値を使う。
   float requiredRise = 0.f;
   const bool arrowVisible = VisitProjectedBounds(arrow, GameEngine::MakeAffineMatrix(arrowPose) * viewProjection,
      [&](const auto& point) { requiredRise = std::max(requiredRise, targetY * point.w - point.y); });
   const float risePerMeter =
      GameEngine::TransformVectorByMatrix({cameraUp.x, cameraUp.y, cameraUp.z, 0.f}, viewProjection).y;

   if (!arrowVisible || !std::isfinite(risePerMeter) || risePerMeter <= 0.f) return std::nullopt;

   return requiredRise / risePerMeter;
}

} // namespace

namespace App {

void RaceGoalDirectionWorldComponent::OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) {
   player_ = sceneWorld.FindObjectById(playerId_);
   goal_ = sceneWorld.FindObjectById(goalId_);
   const auto* manager = raceManagerId_.empty() ? nullptr : sceneWorld.FindObjectById(raceManagerId_);
   raceManager_ = manager ? manager->GetComponent<RaceManagerComponent>() : nullptr;
}

bool RaceGoalDirectionWorldComponent::ShouldShow() const {
   if (!IsEnabled() || !guidanceEnabled_) return false;

   if (raceManagerId_.empty()) return true;

   if (!raceManager_) {
      Logger::Fail("Goal direction arrow: missing race manager " + raceManagerId_, Logger::LogChannel::Game);
   }

   const auto state = raceManager_->GetState();
   return state == RaceManagerComponent::State::Countdown || state == RaceManagerComponent::State::Running;
}

void RaceGoalDirectionWorldComponent::Present(const GameEngine::Camera* camera) {
   if (!camera || !ShouldShow()) {
      Hide();
      return;
   }

   auto& arrow = GetOwner();
   auto* transform = arrow.GetComponent<GameEngine::TransformComponent>();
   auto* render = arrow.GetComponent<GameEngine::RenderComponent>();
   auto* uiModel = arrow.GetComponent<GameEngine::UIModelComponent>();

   if (!transform || !render || !uiModel || !arrow.GetParentEntityId().empty()) {
      Logger::Fail("Goal direction arrow: world placement requires an unparented model with Transform, Render and "
                   "UIModel: " + arrow.GetEntityId(), Logger::LogChannel::Game);
   }

   const auto* playerTransform = player_ ? player_->GetComponent<GameEngine::TransformComponent>() : nullptr;

   if (!playerTransform || !goal_ || !goal_->GetComponent<GameEngine::TransformComponent>()) {
      Logger::Fail("Goal direction arrow: invalid player or goal reference (" + playerId_ + ", " + goalId_ + ")",
         Logger::LogChannel::Game);
   }

   const auto* vehicleModel = GetModelAsset(*player_);
   const auto* arrowModel = GetModelAsset(arrow);

   if (!vehicleModel || !arrowModel) {
      Logger::Fail("Goal direction arrow: missing vehicle or arrow model (" + playerId_ + ", " + arrow.GetEntityId() +
         ")", Logger::LogChannel::Game);
   }

   const auto playerWorld = playerTransform->ComposeWorldMatrix(playerTransform->HasWorldMatrixOverride()
      ? playerTransform->GetWorldMatrixOverride() : GameEngine::MakeAffineMatrix(playerTransform->GetLocalPose()));
   const auto playerPosition = GetTranslation(playerWorld);
   const auto directionToGoal = GetTranslation(goal_->GetWorldMatrix()) - playerPosition;
   const float distanceSquared = directionToGoal.LengthSquared();
   // 実際のビュー行列を使い、カメラのロールやビュー行列の上書きも上方向へ反映する。
   const auto cameraToWorld = camera->GetViewMatrix().Inverse();
   const auto cameraUp = GameEngine::TransformNormal(kLocalCameraUp, cameraToWorld).Normalize();
   const float cameraUpLengthSquared = cameraUp.LengthSquared();

   if (!std::isfinite(distanceSquared) || !std::isfinite(cameraUpLengthSquared) || cameraUpLengthSquared <= 0.f) {
      Hide();
      Logger::Error("Goal direction arrow: invalid vehicle, goal or camera pose (" + playerId_ + ", " + goalId_ + ")",
         Logger::LogChannel::Game);
      return;
   }

   if (distanceSquared <= kMinGoalDistanceSquared) {
      Hide();
      return;
   }

   const auto rotation = GameEngine::LookRotation(directionToGoal / std::sqrt(distanceSquared), cameraUp);
   const auto center = playerPosition + cameraUp * heightAboveVehicle_;
   const auto pivot = uiModel->DescribeSettings().localPivot * modelScale_;

   // カメラ空間で車体と矢印中心の横位置と奥行きを揃え、ゴール方向へ回転しても中心の位置を保つ。
   GameEngine::Transform pose;
   pose.scale = {modelScale_, modelScale_, modelScale_};
   pose.translation = center - GameEngine::RotateVector(pivot, rotation);
   pose.SetRotationQuaternion(rotation);

   const auto additionalHeight = GetAdditionalHeight(*vehicleModel, *arrowModel, camera->GetViewProjectionMatrix(),
      playerWorld, pose, cameraUp);

   if (!additionalHeight) {
      Hide();
      return;
   }

   pose.translation += cameraUp * *additionalHeight;

   if (!transform->ApplyLocalPose(pose)) {
      Hide();
      Logger::Error("Goal direction arrow: could not apply world pose to " + arrow.GetEntityId(),
         Logger::LogChannel::Game);
      return;
   }

   uiModel->SetEnabled(false);
   auto settings = render->DescribeSettings();
   settings.renderSpace = GameEngine::RenderComponent::RenderSpace::World;
   settings.applyPostProcess = true;
   settings.visible = true;
   render->Configure(settings);
}

void RaceGoalDirectionWorldComponent::SetGuidanceEnabled(bool enabled) {
   guidanceEnabled_ = enabled;
}

void RaceGoalDirectionWorldComponent::OnDisable() {
   Hide();
}

void RaceGoalDirectionWorldComponent::Hide() {
   auto* render = HasOwner() ? GetOwner().GetComponent<GameEngine::RenderComponent>() : nullptr;

   if (!render) return;

   auto settings = render->DescribeSettings();

   if (settings.visible) {
      settings.visible = false;
      render->Configure(settings);
   }
}

nlohmann::json RaceGoalDirectionWorldComponent::Serialize() const {
   return {{"playerObjectId", playerId_}, {"goalObjectId", goalId_}, {"raceManagerId", raceManagerId_},
      {"heightAboveVehicleMeters", heightAboveVehicle_}, {"modelScale", modelScale_}};
}

void RaceGoalDirectionWorldComponent::Deserialize(const nlohmann::json& data) {
   try {
      auto playerId = data.at("playerObjectId").get<std::string>();
      auto goalId = data.at("goalObjectId").get<std::string>();
      auto raceManagerId = data.at("raceManagerId").get<std::string>();
      const float height = data.at("heightAboveVehicleMeters").get<float>();
      const float scale = data.at("modelScale").get<float>();

      if (playerId.empty() || goalId.empty() || !std::isfinite(height) || height <= 0.f ||
          !std::isfinite(scale) || scale <= 0.f) {
         Logger::Fail(
            "Goal direction arrow: invalid playerObjectId, goalObjectId, heightAboveVehicleMeters or modelScale",
            Logger::LogChannel::Game);
      }

      playerId_ = std::move(playerId);
      goalId_ = std::move(goalId);
      raceManagerId_ = std::move(raceManagerId);
      heightAboveVehicle_ = height;
      modelScale_ = scale;
   } catch (const nlohmann::json::exception& error) {
      Logger::Fail(std::string("Goal direction arrow: invalid settings: ") + error.what(), Logger::LogChannel::Game);
   }
}

} // namespace App
