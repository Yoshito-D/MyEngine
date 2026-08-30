#include "TitleStartComponent.h"

#include "Framework/EngineContext.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Component/UI/UITextComponent.h"
#include "Object/Object.h"
#include "Scene/BaseScene.h"
#include "Scene/SceneWorld.h"
#include "Utility/MathUtils.h"
#include <algorithm>
#include <string>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void TitleStartComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   selectedOption_ = 0;
   startRequested_ = false;
   navigationLatched_ = false;
   reactionElapsed_ = 0.0f;
   hasBaseVisualStates_ = false;
   optionTexts_.fill(nullptr);
   optionTransforms_.fill(nullptr);
   ResolveOptionVisuals(sceneWorld);
   hasBaseVisualStates_ = CaptureBaseVisualStates();
   SelectInitialOption();
   RefreshSelectionText();
}

void TitleStartComponent::Update(float deltaTime) {
   if (!hasBaseVisualStates_) {
      return;
   }

   if (!startRequested_) {
      const auto& navigationAction =
         GameEngine::EngineContext::GetInputActionState("UI", "UI.Navigate", 0);
      const int navigationDirection = navigationAction.value.x > 0.5f
         ? 1
         : (navigationAction.value.x < -0.5f ? -1 : 0);
      if (navigationDirection == 0) {
         navigationLatched_ = false;
      } else if (!navigationLatched_) {
         // 押し続けで選択が往復しないよう、軸が中立へ戻るまで次の移動を受け付けない。
         MoveSelection(navigationDirection);
         navigationLatched_ = true;
         RefreshSelectionText();
      }

      const auto& confirmAction =
         GameEngine::EngineContext::GetInputActionState("UI", "UI.Confirm", 0);
      const std::string& selectedScene = GetSelectedSceneName();
      if (!confirmAction.triggered || selectedScene.empty()) {
         return;
      }

      // 遷移要求を先に出し、次フレームから共通暗転とUIリアクションを同じ時間軸で重ねる。
      startRequested_ = true;
      reactionElapsed_ = 0.0f;
      GameEngine::BaseScene::SetNextSceneName(selectedScene);
      return;
   }

   reactionElapsed_ += std::max(deltaTime, 0.0f);
   ApplyStartReaction(static_cast<std::size_t>(selectedOption_));
}

nlohmann::json TitleStartComponent::Serialize() const {
   return nlohmann::json{
      { "tutorialOptionObjectId", tutorialOptionObjectId_ },
      { "stageOptionObjectId", stageOptionObjectId_ },
      { "tutorialScene", tutorialScene_ },
      { "stageScene", stageScene_ },
      { "reactionDuration", reactionDuration_ },
      { "reactionEndScale", reactionEndScale_ }
   };
}

void TitleStartComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   legacySingleOption_ = false;
   if (data.contains("tutorialOptionObjectId") && data.at("tutorialOptionObjectId").is_string()) {
      tutorialOptionObjectId_ = data.at("tutorialOptionObjectId").get<std::string>();
   }
   if (data.contains("stageOptionObjectId") && data.at("stageOptionObjectId").is_string()) {
      stageOptionObjectId_ = data.at("stageOptionObjectId").get<std::string>();
   }
   if (data.contains("tutorialScene") && data.at("tutorialScene").is_string()) {
      tutorialScene_ = data.at("tutorialScene").get<std::string>();
   }
   if (data.contains("stageScene") && data.at("stageScene").is_string()) {
      stageScene_ = data.at("stageScene").get<std::string>();
   } else if (data.contains("nextScene") && data.at("nextScene").is_string()) {
      // 旧タイトルのオーナー表示を単一のステージ選択肢として引き継ぐ。
      stageScene_ = data.at("nextScene").get<std::string>();
      if (!data.contains("stageOptionObjectId")) {
         stageOptionObjectId_ = HasOwner()
            ? GetOwner().GetEntityId()
            : tutorialOptionObjectId_;
      }
      tutorialOptionObjectId_.clear();
      legacySingleOption_ = true;
   }
   if (data.contains("reactionDuration") && data.at("reactionDuration").is_number()) {
      reactionDuration_ = std::max(data.at("reactionDuration").get<float>(), 0.0001f);
   }
   if (data.contains("reactionEndScale") && data.at("reactionEndScale").is_number()) {
      reactionEndScale_ = std::max(data.at("reactionEndScale").get<float>(), 1.0f);
   }
}

void TitleStartComponent::ResolveOptionVisuals(GameEngine::SceneWorld& sceneWorld) {
   const std::array<std::string, 2> objectIds = {
      tutorialOptionObjectId_,
      stageOptionObjectId_
   };
   std::array<GameEngine::Object*, 2> optionObjects = {};
   for (std::size_t optionIndex = 0; optionIndex < objectIds.size(); ++optionIndex) {
      optionObjects[optionIndex] = sceneWorld.FindObjectById(objectIds[optionIndex]);
   }

   if (HasOwner()) {
      GameEngine::Object* owner = &GetOwner();
      // nextScene形式ではオーナー自身が単一のステージ表示だった。
      if (legacySingleOption_ && !optionObjects[1]) {
         optionObjects[1] = owner;
      } else if (!optionObjects[0] && !tutorialOptionObjectId_.empty() && optionObjects[1] != owner) {
         // チュートリアル側のID変更中だけはオーナーから復元する。
         optionObjects[0] = owner;
      }
   }

   for (std::size_t optionIndex = 0; optionIndex < optionObjects.size(); ++optionIndex) {
      GameEngine::Object* optionObject = optionObjects[optionIndex];
      if (!optionObject) {
         continue;
      }
      optionTexts_[optionIndex] = optionObject->GetComponent<GameEngine::UITextComponent>();
      optionTransforms_[optionIndex] = optionObject->GetComponent<GameEngine::TransformComponent>();
   }
}

bool TitleStartComponent::CaptureBaseVisualStates() {
   bool capturedAny = false;
   for (std::size_t optionIndex = 0; optionIndex < optionTexts_.size(); ++optionIndex) {
      const auto* text = optionTexts_[optionIndex];
      const auto* transform = optionTransforms_[optionIndex];
      if (!text || !transform) {
         continue;
      }
      baseOpacities_[optionIndex] = text->GetStyle().color.w;
      baseScales_[optionIndex] = transform->transform.scale;
      capturedAny = true;
   }
   return capturedAny;
}

bool TitleStartComponent::IsOptionAvailable(std::size_t optionIndex) const {
   return optionIndex < optionTexts_.size() &&
      optionTexts_[optionIndex] != nullptr && optionTransforms_[optionIndex] != nullptr;
}

bool TitleStartComponent::SelectInitialOption() {
   for (std::size_t optionIndex = 0; optionIndex < optionTexts_.size(); ++optionIndex) {
      if (IsOptionAvailable(optionIndex)) {
         selectedOption_ = static_cast<int>(optionIndex);
         return true;
      }
   }
   return false;
}

void TitleStartComponent::MoveSelection(int direction) {
   constexpr int kOptionCount = 2;
   int candidate = selectedOption_;
   for (int attempt = 0; attempt < kOptionCount; ++attempt) {
      candidate = (candidate + direction + kOptionCount) % kOptionCount;
      if (IsOptionAvailable(static_cast<std::size_t>(candidate))) {
         selectedOption_ = candidate;
         return;
      }
   }
}

void TitleStartComponent::RefreshSelectionText() {
   // 旧シーンの単一プロンプトは、作者が設定した文言をそのまま保つ。
   if (legacySingleOption_) {
      return;
   }
   if (IsOptionAvailable(0)) {
      optionTexts_[0]->SetText(
         std::string(selectedOption_ == 0 ? "> " : "  ") + "チュートリアルから あそぶ");
   }
   if (IsOptionAvailable(1)) {
      optionTexts_[1]->SetText(
         std::string(selectedOption_ == 1 ? "> " : "  ") + "ステージから あそぶ");
   }
}

const std::string& TitleStartComponent::GetSelectedSceneName() const {
   static const std::string kEmptySceneName;
   if (!IsOptionAvailable(static_cast<std::size_t>(selectedOption_))) {
      return kEmptySceneName;
   }
   return selectedOption_ == 0 ? tutorialScene_ : stageScene_;
}

void TitleStartComponent::ApplyStartReaction(std::size_t optionIndex) {
   if (!IsOptionAvailable(optionIndex)) {
      return;
   }

   auto& text = *optionTexts_[optionIndex];
   auto& transform = *optionTransforms_[optionIndex];
   const auto& baseScale = baseScales_[optionIndex];
   // 毎フレーム基準値から計算し、スケールと透明度の補間誤差を蓄積させない。
   const float progress = std::clamp(reactionElapsed_ / reactionDuration_, 0.0f, 1.0f);
   const float scaleMultiplier =
      GameEngine::Easing::EaseOutCubic(1.0f, reactionEndScale_, progress);
   transform.transform.scale = {
      baseScale.x * scaleMultiplier,
      baseScale.y * scaleMultiplier,
      baseScale.z
   };
   text.SetOpacity(GameEngine::Easing::EaseInQuad(baseOpacities_[optionIndex], 0.0f, progress));
}

#ifdef USE_IMGUI
void TitleStartComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Text("Tutorial Option Object ID: %s", tutorialOptionObjectId_.c_str());
   ImGui::Text("Stage Option Object ID: %s", stageOptionObjectId_.c_str());
   ImGui::Text("Tutorial Scene: %s", tutorialScene_.c_str());
   ImGui::Text("Stage Scene: %s", stageScene_.c_str());
   ImGui::Text("Selected Option: %d", selectedOption_);
   ImGui::Text("Resolved Options: %s", hasBaseVisualStates_ ? "true" : "false");
   ImGui::Text("Start Requested: %s", startRequested_ ? "true" : "false");
   ImGui::DragFloat("Reaction Duration", &reactionDuration_, 0.01f, 0.01f, 1.0f, "%.2f s");
   ImGui::DragFloat("Reaction End Scale", &reactionEndScale_, 0.01f, 1.0f, 3.0f, "%.2f");
}
#endif

} // namespace App
