#include "GameProject/App/Component/Race/UI/GamepadGuideTextComponent.h"

#include "GameProject/App/Component/Race/RaceManagerComponent.h"
#include "GameProject/App/Component/Character/CharacterJump.h"
#include "GameProject/App/Component/Vehicle/VehicleInputComponent.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/SceneWorld.h"

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {

/// @brief シーン設定のオブジェクトIDから表示条件に必要なコンポーネントを解決する
/// @param sceneWorld 検索対象のシーンワールド
void GamepadGuideTextComponent::OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) {
   // シーン再読み込み時に以前のシーンを指すポインターを残さないよう、
   // 新しい参照を検索する前に必ず解決状態を初期化する。
   raceManager_ = nullptr;
   characterJump_ = nullptr;
   vehicleInput_ = nullptr;

   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }

   if (auto* playerObject = sceneWorld.FindObjectById(playerObjectId_)) {
      characterJump_ = playerObject->GetComponent<CharacterJump>();
      vehicleInput_ = playerObject->GetComponent<VehicleInputComponent>();
   }
}

/// @brief 接続状態・レース状態・プレイヤーのジャンプ状態に応じて操作ガイドを更新する
/// @param deltaTime 前フレームからの経過時間。本コンポーネントでは状態参照のみのため使用しない
void GamepadGuideTextComponent::Update(float deltaTime) {
   (void)deltaTime;

   if (!HasOwner()) return;

   auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();

   if (!text) return;

   using RaceState = RaceManagerComponent::State;
   const auto raceState = raceManager_ ? raceManager_->GetState() : RaceState::Waiting;

   // 導入とリザルトはそれぞれ専用の案内を表示するため、走行用の説明を隠す。
   if (raceState == RaceState::Intro || raceState == RaceState::Finished) {
      text->SetText("");
      return;
   }

   // CharacterJumpを解決できない構成でもガイド自体は維持し、
   // 空中状態を確認できた場合に限って空中操作へ切り替える。
   const bool airborne = characterJump_ && characterJump_->IsJumping();

   // 走行入力と同じスロットの割り当てを毎フレーム参照し、接続と再割り当てを表示へ反映する。
   const auto controls = VehicleInputComponent::DescribeControlLabels(vehicleInput_);
   text->SetText(airborne ? controls.AirPrompts() : controls.GroundPrompts());
}

/// @brief 参照先を再解決するためのオブジェクトIDをJSONへ保存する
/// @return レース管理オブジェクトとプレイヤーオブジェクトのIDを格納したJSON
nlohmann::json GamepadGuideTextComponent::Serialize() const {
   return nlohmann::json{{"raceManagerId", raceManagerId_}, {"playerObjectId", playerObjectId_}};
}

/// @brief JSONに含まれる有効なオブジェクトID設定を読み込む
/// @param data コンポーネント設定を格納したJSON
void GamepadGuideTextComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) return;

   // 欠落または型不一致の項目では現在値を維持し、
   // 古いシーンデータや部分的な設定データも読み込めるようにする。
   if (data.contains("raceManagerId") && data.at("raceManagerId").is_string()) {
      raceManagerId_ = data.at("raceManagerId").get<std::string>();
   }

   if (data.contains("playerObjectId") && data.at("playerObjectId").is_string()) {
      playerObjectId_ = data.at("playerObjectId").get<std::string>();
   }
}

#ifdef USE_IMGUI

/// @brief 設定IDと実行時の参照解決状況をインスペクターへ表示する
void GamepadGuideTextComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!ImGui::CollapsingHeader(header.c_str())) return;

   GameEngine::EditorUI::ObjectReference("Race Manager", raceManagerId_, "RaceManagerComponent");
   GameEngine::EditorUI::ObjectReference("Player", playerObjectId_, "TransformComponent");
   ImGui::Text("Resolved: Race=%s Player Jump=%s", raceManager_ ? "true" : "false", characterJump_ ? "true" : "false");
}
#endif

} // namespace App
