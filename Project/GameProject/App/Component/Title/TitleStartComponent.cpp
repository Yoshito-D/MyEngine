#include "TitleStartComponent.h"

#include "Framework/EngineContext.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Component/UI/UITextComponent.h"
#include "Object/Object.h"
#include "Scene/BaseScene.h"
#include "Utility/MathUtils.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void TitleStartComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   (void)sceneWorld;
   startRequested_ = false;
   reactionElapsed_ = 0.0f;
   hasBaseVisualState_ = false;
   CaptureBaseVisualState();
}

void TitleStartComponent::Update(float deltaTime) {
   if (!HasOwner()) {
      return;
   }
   auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!text || !transform || (!hasBaseVisualState_ && !CaptureBaseVisualState())) {
      return;
   }

   if (!startRequested_) {
      const auto& confirmAction =
         GameEngine::EngineContext::GetInputActionState("UI", "UI.Confirm", 0);
      if (!confirmAction.triggered || nextScene_.empty()) {
         return;
      }

      // 遷移要求を先に出し、次フレームから共通暗転とUIリアクションを同じ時間軸で重ねる。
      startRequested_ = true;
      reactionElapsed_ = 0.0f;
      GameEngine::BaseScene::SetNextSceneName(nextScene_);
      return;
   }

   reactionElapsed_ += std::max(deltaTime, 0.0f);
   ApplyStartReaction(*text, *transform);
}

nlohmann::json TitleStartComponent::Serialize() const {
   return nlohmann::json{
      { "nextScene", nextScene_ },
      { "reactionDuration", reactionDuration_ },
      { "reactionEndScale", reactionEndScale_ }
   };
}

void TitleStartComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   if (data.contains("nextScene") && data.at("nextScene").is_string()) {
      nextScene_ = data.at("nextScene").get<std::string>();
   }
   if (data.contains("reactionDuration") && data.at("reactionDuration").is_number()) {
      reactionDuration_ = std::max(data.at("reactionDuration").get<float>(), 0.0001f);
   }
   if (data.contains("reactionEndScale") && data.at("reactionEndScale").is_number()) {
      reactionEndScale_ = std::max(data.at("reactionEndScale").get<float>(), 1.0f);
   }
}

bool TitleStartComponent::CaptureBaseVisualState() {
   if (!HasOwner()) {
      return false;
   }
   const auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();
   const auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!text || !transform) {
      return false;
   }

   baseOpacity_ = text->GetStyle().color.w;
   baseScale_ = transform->transform.scale;
   hasBaseVisualState_ = true;
   return true;
}

void TitleStartComponent::ApplyStartReaction(
   GameEngine::UITextComponent& text,
   GameEngine::TransformComponent& transform) {
   const float progress = std::clamp(reactionElapsed_ / reactionDuration_, 0.0f, 1.0f);
   const float scaleMultiplier =
      GameEngine::Easing::EaseOutCubic(1.0f, reactionEndScale_, progress);
   transform.transform.scale = {
      baseScale_.x * scaleMultiplier,
      baseScale_.y * scaleMultiplier,
      baseScale_.z
   };
   text.SetOpacity(GameEngine::Easing::EaseInQuad(baseOpacity_, 0.0f, progress));
}

#ifdef USE_IMGUI
void TitleStartComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Text("Next Scene: %s", nextScene_.c_str());
   ImGui::Text("Start Requested: %s", startRequested_ ? "true" : "false");
   ImGui::DragFloat("Reaction Duration", &reactionDuration_, 0.01f, 0.01f, 1.0f, "%.2f s");
   ImGui::DragFloat("Reaction End Scale", &reactionEndScale_, 0.01f, 1.0f, 3.0f, "%.2f");
}
#endif

} // namespace App
