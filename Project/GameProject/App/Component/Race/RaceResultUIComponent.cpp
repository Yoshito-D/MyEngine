#include "RaceResultUIComponent.h"

#include "RaceManagerComponent.h"
#include "RaceTimeFormatting.h"
#include "Framework/EngineContext.h"
#include "Object/Component/UI/UITextComponent.h"
#include "Object/Object.h"
#include "Scene/SceneWorld.h"
#include <sstream>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void RaceResultUIComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   // シーン再読み込み後に破棄済みオブジェクトを参照しないよう、
   // 前回解決したポインターを捨てて保存済みIDから解決し直す。
   raceManager_ = nullptr;
   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
}

void RaceResultUIComponent::Update(float deltaTime) {
   (void)deltaTime;
   if (!raceManager_ || !HasOwner()) {
      return;
   }
   auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();
   if (!text) {
      return;
   }

   // このUITextはレース中もシーンに残るため、終了状態以外では内容を消しておく。
   // これによりリスタート後に前回のリザルトが一瞬残ることも防ぐ。
   if (raceManager_->GetState() != RaceManagerComponent::State::Finished) {
      text->SetText("");
      return;
   }

   text->SetText(BuildResultText());
   const auto& confirmAction =
      GameEngine::EngineContext::GetInputActionState("UI", "UI.Confirm", 0);
   if (confirmAction.triggered) {
      // 実際の遷移要求と二重要求の防止はRaceManagerへ集約し、
      // リザルトUIから自動遷移との競合を作らないようにする。
      raceManager_->RequestRestart();
   }
}

nlohmann::json RaceResultUIComponent::Serialize() const {
   // 実行時ポインターはシーンをまたいで有効ではないため、再解決用IDだけを永続化する。
   return nlohmann::json{ { "raceManagerId", raceManagerId_ } };
}

void RaceResultUIComponent::Deserialize(const nlohmann::json& data) {
   // 欠落または型不一致の値では現在の設定を維持し、旧形式や部分設定も受け入れる。
   if (data.is_object() && data.contains("raceManagerId") && data.at("raceManagerId").is_string()) {
      raceManagerId_ = data.at("raceManagerId").get<std::string>();
   }
}

std::string RaceResultUIComponent::BuildResultText() const {
   std::ostringstream stream;
   stream << "FINISH\nTIME " << FormatRaceTime(raceManager_->GetElapsedTime())
      << "\n\nBEST TIMES\n";

   // RaceManagerが保持する昇順の記録を、その並びを崩さず順位表示へ変換する。
   const auto& bestTimes = raceManager_->GetBestTimes();
   // 記録数にかかわらず固定枠を表示し、リザルトUIの高さが変動しないようにする。
   for (size_t index = 0; index < RaceManagerComponent::kBestTimeCount; ++index) {
      stream << index + 1 << ". ";
      if (index < bestTimes.size()) {
         stream << FormatRaceTime(bestTimes[index]);
      } else {
         stream << "--:--.---";
      }
      stream << '\n';
   }
   stream << "\n> RESTART\nA / SPACE";
   return stream.str();
}

#ifdef USE_IMGUI
void RaceResultUIComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Text("Race Manager ID: %s", raceManagerId_.c_str());
   ImGui::Text("Resolved: %s", raceManager_ ? "true" : "false");
}
#endif

} // namespace App
