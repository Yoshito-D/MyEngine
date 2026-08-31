#include "GamepadGuideTextComponent.h"

#include "RaceManagerComponent.h"
#include "../Character/CharacterJump.h"
#include "Object/Component/UI/UITextComponent.h"
#include "Object/Object.h"
#include "Scene/SceneWorld.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {
namespace {

/// @brief 地上走行中に表示する基本操作ガイド
constexpr const char* kGroundGuideText =
   "左スティック：旋回 / ドリフト\n"
   "A：ジャンプ";

/// @brief ジャンプ中に表示する空中姿勢制御の操作ガイド
constexpr const char* kAirGuideText =
   "左スティック上下：ピッチ\n"
   "LB / RB：ロール";

} // namespace

/// @brief シーン設定のオブジェクトIDから表示条件に必要なコンポーネントを解決する
/// @param sceneWorld 検索対象のシーンワールド
void GamepadGuideTextComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   // シーン再読み込み時に以前のシーンを指すポインターを残さないよう、
   // 新しい参照を検索する前に必ず解決状態を初期化する。
   raceManager_ = nullptr;
   characterJump_ = nullptr;
   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
   if (auto* playerObject = sceneWorld.FindObjectById(playerObjectId_)) {
      characterJump_ = playerObject->GetComponent<CharacterJump>();
   }
}

/// @brief レース状態とプレイヤーのジャンプ状態に応じて操作ガイドを更新する
/// @param deltaTime 前フレームからの経過時間。本コンポーネントでは状態参照のみのため使用しない
void GamepadGuideTextComponent::Update(float deltaTime) {
   (void)deltaTime;
   if (!HasOwner()) {
      return;
   }
   auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();
   if (!text) {
      return;
   }

   const bool isShowingResult = raceManager_ &&
      raceManager_->GetState() == RaceManagerComponent::State::Finished;
   // リザルト画面では走行中の入力説明が誤解を招くため、
   // ジャンプ状態よりもレース終了判定を優先して表示を消す。
   if (isShowingResult) {
      text->SetText("");
      return;
   }

   // CharacterJumpを解決できない構成でもガイド自体は維持し、
   // 空中状態を確認できた場合に限って空中操作へ切り替える。
   text->SetText(characterJump_ && characterJump_->IsJumping()
      ? kAirGuideText
      : kGroundGuideText);
}

/// @brief 参照先を再解決するためのオブジェクトIDをJSONへ保存する
/// @return レース管理オブジェクトとプレイヤーオブジェクトのIDを格納したJSON
nlohmann::json GamepadGuideTextComponent::Serialize() const {
   return nlohmann::json{
      { "raceManagerId", raceManagerId_ },
      { "playerObjectId", playerObjectId_ }
   };
}

/// @brief JSONに含まれる有効なオブジェクトID設定を読み込む
/// @param data コンポーネント設定を格納したJSON
void GamepadGuideTextComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
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
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Text("Race Manager ID: %s", raceManagerId_.c_str());
   ImGui::Text("Player Object ID: %s", playerObjectId_.c_str());
   ImGui::Text("Resolved: Race=%s Player Jump=%s",
      raceManager_ ? "true" : "false",
      characterJump_ ? "true" : "false");
}
#endif

} // namespace App
