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
   CaptureBaseVisualStates();
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
         constexpr int kOptionCount = 2;
         selectedOption_ = (selectedOption_ + navigationDirection + kOptionCount) % kOptionCount;
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
   ApplyStartReaction(static_cast<size_t>(selectedOption_));
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
      // 旧タイトルデータの単一遷移先は、通常ステージ側として引き継ぐ。
      stageScene_ = data.at("nextScene").get<std::string>();
   }
   if (data.contains("reactionDuration") && data.at("reactionDuration").is_number()) {
      reactionDuration_ = std::max(data.at("reactionDuration").get<float>(), 0.0001f);
   }
   if (data.contains("reactionEndScale") && data.at("reactionEndScale").is_number()) {
      reactionEndScale_ = std::max(data.at("reactionEndScale").get<float>(), 1.0f);
   }
}

bool TitleStartComponent::ResolveOptionVisuals(GameEngine::SceneWorld& sceneWorld) {
   const std::array<std::string, 2> objectIds = {
      tutorialOptionObjectId_,
      stageOptionObjectId_
   };
   for (size_t optionIndex = 0; optionIndex < objectIds.size(); ++optionIndex) {
      GameEngine::Object* optionObject = sceneWorld.FindObjectById(objectIds[optionIndex]);
      // 旧シーンやID変更中でも、チュートリアル側はオーナーから復元できるようにする。
      if (!optionObject && optionIndex == 0 && HasOwner()) {
         optionObject = &GetOwner();
      }
      if (!optionObject) {
         continue;
      }
      optionTexts_[optionIndex] = optionObject->GetComponent<GameEngine::UITextComponent>();
      optionTransforms_[optionIndex] = optionObject->GetComponent<GameEngine::TransformComponent>();
   }
   return std::all_of(optionTexts_.begin(), optionTexts_.end(), [](const auto* text) { return text != nullptr; }) &&
      std::all_of(optionTransforms_.begin(), optionTransforms_.end(), [](const auto* transform) { return transform != nullptr; });
}

bool TitleStartComponent::CaptureBaseVisualStates() {
   for (size_t optionIndex = 0; optionIndex < optionTexts_.size(); ++optionIndex) {
      const auto* text = optionTexts_[optionIndex];
      const auto* transform = optionTransforms_[optionIndex];
      if (!text || !transform) {
         return false;
      }
      baseOpacities_[optionIndex] = text->GetStyle().color.w;
      baseScales_[optionIndex] = transform->transform.scale;
   }

   hasBaseVisualStates_ = true;
   return true;
}

void TitleStartComponent::RefreshSelectionText() {
   if (!optionTexts_[0] || !optionTexts_[1]) {
      return;
   }
   optionTexts_[0]->SetText(
      std::string(selectedOption_ == 0 ? "> " : "  ") + "チュートリアルから あそぶ");
   optionTexts_[1]->SetText(
      std::string(selectedOption_ == 1 ? "> " : "  ") + "ステージから あそぶ");
}

const std::string& TitleStartComponent::GetSelectedSceneName() const {
   return selectedOption_ == 0 ? tutorialScene_ : stageScene_;
}

void TitleStartComponent::ApplyStartReaction(size_t optionIndex) {
   if (optionIndex >= optionTexts_.size() ||
      !optionTexts_[optionIndex] || !optionTransforms_[optionIndex]) {
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
