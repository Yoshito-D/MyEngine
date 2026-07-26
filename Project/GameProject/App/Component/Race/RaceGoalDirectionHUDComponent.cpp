#include "RaceGoalDirectionHUDComponent.h"

#include "../Gravity/GravityBody.h"
#include "RaceManagerComponent.h"
#include "Framework/EngineContext.h"
#include "Logger.h"
#include "Object/Component/RenderComponent.h"
#include "Object/Object.h"
#include "Object/Sprite/Sprite.h"
#include "Scene/Camera/Camera.h"
#include "Scene/SceneWorld.h"
#include "Utility/MathUtils/VectorOperations.h"
#include <algorithm>
#include <cmath>
#include <limits>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace {
constexpr float kDirectionEpsilon = 1.0e-6f;
constexpr float kDirectionEpsilonSquared = kDirectionEpsilon * kDirectionEpsilon;
constexpr float kProjectionExitDepth = 0.25f;
constexpr float kProjectionEnterDepth = 1.0f;
constexpr float kMaximumFollowDeltaTime = 1.0f / 15.0f;
constexpr float kMaximumPositionStep = 64.0f;
constexpr float kMaximumRotationStep = 0.35f;
constexpr float kTwoPi = 6.28318530717958647692f;
#ifdef USE_IMGUI
constexpr float kInspectorColumnWidth = 150.0f;
#endif

GameEngine::Vector3 GetWorldPosition(const GameEngine::Object& object) {
   const GameEngine::Matrix4x4 worldMatrix = object.GetWorldMatrix();
   return { worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };
}

GameEngine::Vector3 ProjectOntoPlane(
   const GameEngine::Vector3& direction,
   const GameEngine::Vector3& planeNormal
) {
   return direction - planeNormal * direction.Dot(planeNormal);
}

GameEngine::Vector2 ClampDirectionToScreenRect(
   const GameEngine::Vector2& direction,
   float maxHorizontalOffset,
   float maxVerticalOffset,
   bool forceToEdge
) {
   float scaleToEdge = std::numeric_limits<float>::infinity();
   if (std::abs(direction.x) > kDirectionEpsilon) {
      scaleToEdge = std::min(scaleToEdge, maxHorizontalOffset / std::abs(direction.x));
   }
   if (std::abs(direction.y) > kDirectionEpsilon) {
      scaleToEdge = std::min(scaleToEdge, maxVerticalOffset / std::abs(direction.y));
   }
   if (!std::isfinite(scaleToEdge)) {
      return { 0.0f, 0.0f };
   }

   const float appliedScale = forceToEdge ? scaleToEdge : std::min(scaleToEdge, 1.0f);
   return direction * appliedScale;
}

GameEngine::Vector2 MoveTowards(
   const GameEngine::Vector2& current,
   const GameEngine::Vector2& target,
   float maxDistance
) {
   const GameEngine::Vector2 displacement = target - current;
   const float distance = displacement.Length();
   if (distance <= maxDistance || distance < kDirectionEpsilon) {
      return target;
   }
   if (maxDistance <= 0.0f) {
      return current;
   }
   return current + displacement * (maxDistance / distance);
}

float MoveAngleTowards(float current, float target, float maxRadians) {
   const float shortestDelta = std::remainder(target - current, kTwoPi);
   if (std::abs(shortestDelta) <= maxRadians) {
      return target;
   }
   if (maxRadians <= 0.0f) {
      return current;
   }
   return std::remainder(current + std::copysign(maxRadians, shortestDelta), kTwoPi);
}

}

namespace App {

void RaceGoalDirectionHUDComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   raceManager_ = nullptr;
   playerObject_ = nullptr;
   goalObject_ = nullptr;
   gravityBody_ = nullptr;
   sprite_ = HasOwner() ? dynamic_cast<GameEngine::Sprite*>(&GetOwner()) : nullptr;

   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
   playerObject_ = sceneWorld.FindObjectById(playerObjectId_);
   goalObject_ = sceneWorld.FindObjectById(goalObjectId_);
   if (playerObject_) {
      gravityBody_ = playerObject_->GetComponent<GravityBody>();
   }

   if (!raceManager_) {
      Logger::Warning("Goal direction HUD race manager was not found: " + raceManagerId_, Logger::LogChannel::Game);
   }
   if (!playerObject_) {
      Logger::Warning("Goal direction HUD player was not found: " + playerObjectId_, Logger::LogChannel::Game);
   }
   if (!goalObject_) {
      Logger::Warning("Goal direction HUD goal was not found: " + goalObjectId_, Logger::LogChannel::Game);
   }
   if (!sprite_) {
      Logger::Warning("Goal direction HUD must be attached to a Sprite.", Logger::LogChannel::Game);
   }

   ResetFollowState();
   SetVisible(false);
}

void RaceGoalDirectionHUDComponent::Update(float deltaTime) {
   const bool shouldShow = raceManager_ &&
      raceManager_->GetState() == RaceManagerComponent::State::Running &&
      playerObject_ && goalObject_ && sprite_;
   if (!shouldShow) {
      SetVisible(false);
      ResetFollowState();
      return;
   }

   if (fixedBottomRight_ != lastFixedBottomRight_) {
      // アンカー基準が変わるため、切り替えたフレームだけ新しい配置へ初期化する。
      followInitialized_ = false;
      useProjectedPosition_ = false;
      lastFixedBottomRight_ = fixedBottomRight_;
   }

   HudTarget target;
   if (!TryCalculateTarget(target)) {
      SetVisible(false);
      return;
   }
   SetVisible(true);

   if (!followInitialized_) {
      currentPosition_ = target.position;
      currentRotation_ = target.rotation;
      followInitialized_ = true;
   } else {
      // 大きなフレーム時間でも一度に端から端へ移動しないよう、追従計算用の時間だけを制限する。
      const float followDeltaTime = std::clamp(deltaTime, 0.0f, kMaximumFollowDeltaTime);
      currentPosition_ = MoveTowards(
         currentPosition_,
         target.position,
         std::min(positionFollowSpeed_ * followDeltaTime, kMaximumPositionStep));
      currentRotation_ = MoveAngleTowards(
         currentRotation_,
         target.rotation,
         std::min(rotationFollowSpeed_ * followDeltaTime, kMaximumRotationStep));
   }

   sprite_->SetPosition(currentPosition_);
   sprite_->SetRotation(currentRotation_);
}

bool RaceGoalDirectionHUDComponent::TryCalculateTarget(HudTarget& target) {
   const auto* camera = GameEngine::EngineContext::GetActiveCamera();
   const auto* graphicsDevice = GameEngine::EngineContext::GetGraphicsDevice();
   if (!camera || !graphicsDevice ||
      graphicsDevice->GetBackBufferWidth() == 0 ||
      graphicsDevice->GetBackBufferHeight() == 0) {
      return false;
   }

   GameEngine::Vector3 surfaceUp = gravityBody_
      ? gravityBody_->GetTargetUpVector().Normalize()
      : GameEngine::Vector3{ 0.0f, 1.0f, 0.0f };
   if (surfaceUp.LengthSquared() < kDirectionEpsilonSquared) {
      surfaceUp = { 0.0f, 1.0f, 0.0f };
   }

   const GameEngine::Vector3 goalPosition = GetWorldPosition(*goalObject_);
   const GameEngine::Vector3 goalDirection = goalPosition - GetWorldPosition(*playerObject_);
   if (goalDirection.LengthSquared() < kDirectionEpsilonSquared) {
      return false;
   }
   const GameEngine::Vector3 flatGoalDirection = ProjectOntoPlane(goalDirection, surfaceUp);
   if (flatGoalDirection.LengthSquared() < kDirectionEpsilonSquared) {
      return false;
   }

   const GameEngine::Matrix4x4 viewMatrix = camera->GetViewMatrix();
   const GameEngine::Vector3 viewForward{
      viewMatrix.m[0][2], viewMatrix.m[1][2], viewMatrix.m[2][2]
   };
   const GameEngine::Vector3 viewRight{
      viewMatrix.m[0][0], viewMatrix.m[1][0], viewMatrix.m[2][0]
   };
   const GameEngine::Vector3 viewUp{
      viewMatrix.m[0][1], viewMatrix.m[1][1], viewMatrix.m[2][1]
   };

   GameEngine::Vector3 flatCameraForward = ProjectOntoPlane(viewForward, surfaceUp);
   if (flatCameraForward.LengthSquared() < kDirectionEpsilonSquared) {
      flatCameraForward = ProjectOntoPlane(viewUp, surfaceUp);
   }
   if (flatCameraForward.LengthSquared() < kDirectionEpsilonSquared) {
      return false;
   }
   flatCameraForward = flatCameraForward.Normalize();

   GameEngine::Vector3 flatCameraRight = surfaceUp.Cross(flatCameraForward).Normalize();
   if (flatCameraRight.Dot(viewRight) < 0.0f) {
      flatCameraRight = -flatCameraRight;
   }
   const GameEngine::Vector2 compassDirection{
      flatGoalDirection.Dot(flatCameraRight),
      flatGoalDirection.Dot(flatCameraForward)
   };

   const float halfScreenWidth = static_cast<float>(graphicsDevice->GetBackBufferWidth()) * 0.5f;
   const float halfScreenHeight = static_cast<float>(graphicsDevice->GetBackBufferHeight()) * 0.5f;
   const GameEngine::Vector2 spriteSize = sprite_->GetSize();
   const GameEngine::Vector2 spriteScale = sprite_->GetScale();
   const float spriteHalfDiagonal = 0.5f * std::sqrt(
      spriteSize.x * spriteScale.x * spriteSize.x * spriteScale.x +
      spriteSize.y * spriteScale.y * spriteSize.y * spriteScale.y);

   if (fixedBottomRight_) {
      sprite_->SetScreenAnchorPoint(GameEngine::Sprite::AnchorPoint::BottomRight);
      const float inset = spriteHalfDiagonal + edgePadding_;
      target.position = { -inset, inset };
      target.rotation = std::atan2(compassDirection.y, compassDirection.x);
      return std::isfinite(target.position.x) &&
         std::isfinite(target.position.y) &&
         std::isfinite(target.rotation);
   }

   sprite_->SetScreenAnchorPoint(GameEngine::Sprite::AnchorPoint::MiddleCenter);
   const float maxHorizontalOffset =
      std::max(halfScreenWidth - spriteHalfDiagonal - edgePadding_, 0.0f);
   const float maxVerticalOffset =
      std::max(halfScreenHeight - spriteHalfDiagonal - edgePadding_, 0.0f);
   if (maxHorizontalOffset <= 0.0f || maxVerticalOffset <= 0.0f) {
      return false;
   }

   const GameEngine::Vector4 clipPosition = GameEngine::TransformVectorByMatrix(
      { goalPosition.x, goalPosition.y, goalPosition.z, 1.0f },
      camera->GetViewProjectionMatrix());

   // カメラ面付近で投影と方位表示を毎フレーム往復しないよう、切り替え深度に幅を持たせる。
   if (useProjectedPosition_) {
      if (clipPosition.w < kProjectionExitDepth) {
         useProjectedPosition_ = false;
      }
   } else if (clipPosition.w > kProjectionEnterDepth) {
      useProjectedPosition_ = true;
   }

   GameEngine::Vector2 screenDirection = compassDirection;
   if (useProjectedPosition_) {
      screenDirection = {
         clipPosition.x / clipPosition.w * halfScreenWidth,
         clipPosition.y / clipPosition.w * halfScreenHeight
      };
      if (!std::isfinite(screenDirection.x) || !std::isfinite(screenDirection.y)) {
         useProjectedPosition_ = false;
         screenDirection = compassDirection;
      }
   }

   target.position = ClampDirectionToScreenRect(
      screenDirection,
      maxHorizontalOffset,
      maxVerticalOffset,
      !useProjectedPosition_);
   const GameEngine::Vector2 rotationDirection =
      screenDirection.Length() > kDirectionEpsilon ? screenDirection : compassDirection;
   target.rotation = std::atan2(rotationDirection.y, rotationDirection.x);
   return std::isfinite(target.position.x) &&
      std::isfinite(target.position.y) &&
      std::isfinite(target.rotation);
}

void RaceGoalDirectionHUDComponent::SetVisible(bool visible) {
   if (!sprite_) {
      return;
   }
   if (auto* renderComponent = sprite_->GetComponent<GameEngine::RenderComponent>()) {
      renderComponent->visible = visible;
   }
}

void RaceGoalDirectionHUDComponent::ResetFollowState() {
   followInitialized_ = false;
   useProjectedPosition_ = false;
   lastFixedBottomRight_ = fixedBottomRight_;
}

nlohmann::json RaceGoalDirectionHUDComponent::Serialize() const {
   return nlohmann::json{
      { "raceManagerId", raceManagerId_ },
      { "playerObjectId", playerObjectId_ },
      { "goalObjectId", goalObjectId_ },
      { "edgePadding", edgePadding_ },
      { "positionFollowSpeed", positionFollowSpeed_ },
      { "rotationFollowSpeed", rotationFollowSpeed_ },
      { "fixedBottomRight", fixedBottomRight_ }
   };
}

void RaceGoalDirectionHUDComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   if (data.contains("raceManagerId") && data.at("raceManagerId").is_string()) {
      raceManagerId_ = data.at("raceManagerId").get<std::string>();
   }
   if (data.contains("playerObjectId") && data.at("playerObjectId").is_string()) {
      playerObjectId_ = data.at("playerObjectId").get<std::string>();
   }
   if (data.contains("goalObjectId") && data.at("goalObjectId").is_string()) {
      goalObjectId_ = data.at("goalObjectId").get<std::string>();
   }
   if (data.contains("edgePadding") && data.at("edgePadding").is_number()) {
      edgePadding_ = std::max(data.at("edgePadding").get<float>(), 0.0f);
   }
   if (data.contains("positionFollowSpeed") && data.at("positionFollowSpeed").is_number()) {
      positionFollowSpeed_ = std::max(data.at("positionFollowSpeed").get<float>(), 0.0f);
   }
   if (data.contains("rotationFollowSpeed") && data.at("rotationFollowSpeed").is_number()) {
      rotationFollowSpeed_ = std::max(data.at("rotationFollowSpeed").get<float>(), 0.0f);
   }
   if (data.contains("fixedBottomRight") && data.at("fixedBottomRight").is_boolean()) {
      fixedBottomRight_ = data.at("fixedBottomRight").get<bool>();
   }
}

#ifdef USE_IMGUI
void RaceGoalDirectionHUDComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   GameEngine::ImGuiHelper::DrawInputString(
      "Race Manager ID",
      raceManagerId_,
      GameEngine::ImGuiHelper::kDefaultTextBufferSize,
      kInspectorColumnWidth);
   GameEngine::ImGuiHelper::DrawInputString(
      "Player Object ID",
      playerObjectId_,
      GameEngine::ImGuiHelper::kDefaultTextBufferSize,
      kInspectorColumnWidth);
   GameEngine::ImGuiHelper::DrawInputString(
      "Goal Object ID",
      goalObjectId_,
      GameEngine::ImGuiHelper::kDefaultTextBufferSize,
      kInspectorColumnWidth);
   ImGui::Checkbox(
      GameEngine::ImGuiHelper::Localize({ "右下固定モード", "Fixed Bottom-Right Mode" }),
      &fixedBottomRight_);
   ImGui::DragFloat("Edge Padding", &edgePadding_, 1.0f, 0.0f, 512.0f);
   ImGui::DragFloat("Position Follow Speed", &positionFollowSpeed_, 10.0f, 0.0f, 10000.0f);
   ImGui::DragFloat("Rotation Follow Speed", &rotationFollowSpeed_, 0.1f, 0.0f, 50.0f);
   ImGui::Text("Resolved: Race=%s Player=%s Goal=%s Sprite=%s",
      raceManager_ ? "true" : "false",
      playerObject_ ? "true" : "false",
      goalObject_ ? "true" : "false",
      sprite_ ? "true" : "false");
}
#endif

} // namespace App
