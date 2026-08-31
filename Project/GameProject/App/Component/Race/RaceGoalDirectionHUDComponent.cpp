#include "RaceGoalDirectionHUDComponent.h"

#include "RaceManagerComponent.h"
#include "Framework/EngineContext.h"
#include "Logger.h"
#include "Object/Component/RenderComponent.h"
#include "Object/Component/UI/UIModelComponent.h"
#include "Object/Model/Model.h"
#include "Object/Object.h"
#include "Scene/Camera/Camera.h"
#include "Scene/SceneWorld.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace {
constexpr float kDirectionEpsilonSquared = 1.0e-12f;
constexpr float kPi = 3.14159265358979323846f;
#ifdef USE_IMGUI
constexpr float kInspectorColumnWidth = 150.0f;
#endif

GameEngine::Vector3 GetWorldPosition(const GameEngine::Object& object) {
   // Object の行列規約では最終行の xyz がワールド平行移動成分となる。
   // ローカル Transform ではなく合成済み行列を使い、親子付けされた対象にも追従する。
   const GameEngine::Matrix4x4 worldMatrix = object.GetWorldMatrix();
   return { worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };
}

GameEngine::Quaternion MakeArrowRotation(const GameEngine::Vector3& normalizedDirection) {
   constexpr GameEngine::Vector3 kModelForward{ 0.0f, 0.0f, 1.0f };
   // 浮動小数点誤差で acos の定義域を外れないよう、内積を正規化範囲へ制限する。
   const float directionDot = std::clamp(kModelForward.Dot(normalizedDirection), -1.0f, 1.0f);
   if (directionDot > 1.0f - 1.0e-6f) {
      return GameEngine::Quaternion::Identity();
   }
   if (directionDot < -1.0f + 1.0e-6f) {
      // 真後ろでは外積から回転軸を決められないため、モデルの上方向を明示して 180 度回転させる。
      return GameEngine::MakeRotateAxisAngleQuaternion(
         { 0.0f, 1.0f, 0.0f },
         kPi);
   }

   // 一般ケースでは forward と目標方向の外積を最短回転軸、内積の逆余弦を回転角として使う。
   const GameEngine::Vector3 rotationAxis = kModelForward.Cross(normalizedDirection).Normalize();
   return GameEngine::MakeRotateAxisAngleQuaternion(rotationAxis, std::acos(directionDot));
}

}

namespace App {

void RaceGoalDirectionHUDComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   // 保存された安定 ID を実体ポインターへ解決するのは、全オブジェクトの復元が済んだこの時点で行う。
   // HUD 自身は Model を所有者とし、UIModelComponent が画面空間で描画する構成を必須とする。
   raceManager_ = nullptr;
   playerObject_ = nullptr;
   goalObject_ = nullptr;
   arrowModel_ = HasOwner() ? dynamic_cast<GameEngine::Model*>(&GetOwner()) : nullptr;
   uiModelComponent_ = arrowModel_ ? arrowModel_->GetComponent<GameEngine::UIModelComponent>() : nullptr;

   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
   playerObject_ = sceneWorld.FindObjectById(playerObjectId_);
   goalObject_ = sceneWorld.FindObjectById(goalObjectId_);
   if (!raceManager_) {
      Logger::Warning("Goal direction arrow race manager was not found: " + raceManagerId_, Logger::LogChannel::Game);
   }
   if (!playerObject_) {
      Logger::Warning("Goal direction arrow player was not found: " + playerObjectId_, Logger::LogChannel::Game);
   }
   if (!goalObject_) {
      Logger::Warning("Goal direction arrow goal was not found: " + goalObjectId_, Logger::LogChannel::Game);
   }
   if (!arrowModel_) {
      Logger::Warning("Goal direction arrow must be attached to a Model.", Logger::LogChannel::Game);
   }
   if (!uiModelComponent_) {
      Logger::Warning("Goal direction arrow requires a UIModelComponent.", Logger::LogChannel::Game);
   }

   SetVisible(false);
}

void RaceGoalDirectionHUDComponent::Update(float deltaTime) {
   (void)deltaTime;
   // レース進行中かつ全依存先が解決済みの場合だけ表示する。
   // 不完全なシーンでも前フレームの矢印が残らないよう、失敗経路では必ず非表示へ戻す。
   const bool shouldShow = raceManager_ &&
      raceManager_->GetState() == RaceManagerComponent::State::Running &&
      playerObject_ && goalObject_ && arrowModel_ && uiModelComponent_;
   if (!shouldShow) {
      SetVisible(false);
      return;
   }

   const auto* camera = GameEngine::EngineContext::GetActiveCamera();
   if (!camera) {
      SetVisible(false);
      return;
   }

   const GameEngine::Vector3 playerPosition = GetWorldPosition(*playerObject_);
   const GameEngine::Vector3 directionToGoal = GetWorldPosition(*goalObject_) - playerPosition;
   if (directionToGoal.LengthSquared() < kDirectionEpsilonSquared) {
      // 始点と終点がほぼ同じ場合は方向が定義できず、正規化すると数値誤差を増幅するため描画しない。
      SetVisible(false);
      return;
   }

   // UI専用カメラは回転しないため、ワールド方向をゲームカメラ空間へ移して見た目を一致させる。
   const GameEngine::Vector3 normalizedGoalDirection = directionToGoal.Normalize();
   // w=0 の方向ベクトルとして変換し、ビュー行列のカメラ平行移動を矢印の向きへ混ぜない。
   const GameEngine::Vector4 viewDirection = GameEngine::TransformVectorByMatrix(
      { normalizedGoalDirection.x, normalizedGoalDirection.y, normalizedGoalDirection.z, 0.0f },
      camera->GetViewMatrix());
   const GameEngine::Vector3 uiGoalDirection{ viewDirection.x, viewDirection.y, viewDirection.z };
   if (uiGoalDirection.LengthSquared() < kDirectionEpsilonSquared) {
      SetVisible(false);
      return;
   }
   arrowModel_->SetRotationQuaternion(MakeArrowRotation(uiGoalDirection.Normalize()));
   SetVisible(true);
}

void RaceGoalDirectionHUDComponent::SetVisible(bool visible) {
   if (!arrowModel_) {
      return;
   }
   // コンポーネント自体は更新を続け、描画参加だけを切り替えることでレース状態やカメラ条件を次フレームにも再評価できる。
   if (auto* renderComponent = arrowModel_->GetComponent<GameEngine::RenderComponent>()) {
      renderComponent->visible = visible;
   }
}

nlohmann::json RaceGoalDirectionHUDComponent::Serialize() const {
   return nlohmann::json{
      { "raceManagerId", raceManagerId_ },
      { "playerObjectId", playerObjectId_ },
      { "goalObjectId", goalObjectId_ }
   };
}

void RaceGoalDirectionHUDComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   // キーごとの部分更新にして、旧シーンで未保存の参照 ID はクラス既定値のまま利用する。
   if (data.contains("raceManagerId") && data.at("raceManagerId").is_string()) {
      raceManagerId_ = data.at("raceManagerId").get<std::string>();
   }
   if (data.contains("playerObjectId") && data.at("playerObjectId").is_string()) {
      playerObjectId_ = data.at("playerObjectId").get<std::string>();
   }
   if (data.contains("goalObjectId") && data.at("goalObjectId").is_string()) {
      goalObjectId_ = data.at("goalObjectId").get<std::string>();
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
   ImGui::Text("Resolved: Race=%s Player=%s Goal=%s Model=%s UI Model=%s",
      raceManager_ ? "true" : "false",
      playerObject_ ? "true" : "false",
      goalObject_ ? "true" : "false",
      arrowModel_ ? "true" : "false",
      uiModelComponent_ ? "true" : "false");
}
#endif

} // namespace App
