#include "RaceGoalDistanceTextComponent.h"

#include "RaceManagerComponent.h"
#include "Object/Component/UI/UITextComponent.h"
#include "Object/Object.h"
#include "Scene/SceneWorld.h"
#include <algorithm>
#include <cmath>
#include <format>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace {

#ifdef USE_IMGUI
constexpr float kInspectorColumnWidth = 150.0f;
#endif

GameEngine::Vector3 GetWorldPosition(const GameEngine::Object& object) {
   // 親子階層を含む最終位置を使うため、ローカルTransformではなくワールド行列から取り出す。
   const GameEngine::Matrix4x4 worldMatrix = object.GetWorldMatrix();
   return { worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };
}

} // namespace

namespace App {

void RaceGoalDistanceTextComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   // シーンをまたいで無効になる参照を先に破棄し、それぞれの保存済みIDから独立して解決する。
   // 一部を解決できない場合はUpdate側の表示条件で安全に非表示へ退避する。
   raceManager_ = nullptr;
   playerObject_ = nullptr;
   goalObject_ = nullptr;

   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
   playerObject_ = sceneWorld.FindObjectById(playerObjectId_);
   goalObject_ = sceneWorld.FindObjectById(goalObjectId_);

   // 再読み込み直後に前シーンの距離文字列が残らないよう、状態評価前はいったん空表示にする。
   if (HasOwner()) {
      if (auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>()) {
         text->SetText("");
      }
   }
}

void RaceGoalDistanceTextComponent::Update(float deltaTime) {
   (void)deltaTime;
   if (!HasOwner()) {
      return;
   }
   auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();
   if (!text) {
      return;
   }

   const bool shouldShow = raceManager_ &&
      raceManager_->GetState() == RaceManagerComponent::State::Running &&
      playerObject_ && goalObject_;
   // 待機・カウントダウン・結果中は専用UIと競合しないよう距離表示を消す。
   if (!shouldShow) {
      text->SetText("");
      return;
   }

   // コース上の経路長ではなく、両オブジェクト間のワールド空間上の直線距離を
   // シーン構成に依存しない簡潔な残距離指標として使用する。
   const float distanceMeters =
      (GetWorldPosition(*goalObject_) - GetWorldPosition(*playerObject_)).Length();
   // 不正なTransform値を文字列化してUIへ伝播させず、そのフレームだけ非表示にする。
   if (!std::isfinite(distanceMeters)) {
      text->SetText("");
      return;
   }
   // 小数距離はformatの丸めに任せ、HUDでは読み取りやすい整数メートルへ統一する。
   text->SetText(std::format("{:.0f}m", std::max(distanceMeters, 0.0f)));
}

nlohmann::json RaceGoalDistanceTextComponent::Serialize() const {
   // 実行時ポインターはシーン再構築後に無効となるため、再解決に必要なIDだけを保存する。
   return nlohmann::json{
      { "raceManagerId", raceManagerId_ },
      { "playerObjectId", playerObjectId_ },
      { "goalObjectId", goalObjectId_ }
   };
}

void RaceGoalDistanceTextComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   // 参照IDは個別に読み込み、欠落・型不一致の項目では既定値または現在値を維持する。
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
void RaceGoalDistanceTextComponent::DrawInspector() {
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
   ImGui::Text("Resolved: Race=%s Player=%s Goal=%s",
      raceManager_ ? "true" : "false",
      playerObject_ ? "true" : "false",
      goalObject_ ? "true" : "false");
}
#endif

} // namespace App
