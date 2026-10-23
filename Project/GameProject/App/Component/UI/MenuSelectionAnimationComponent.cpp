#include "GameProject/App/Component/UI/MenuSelectionAnimationComponent.h"

#include "GameEngine/Math/Functions/EasingFunctions.h"
#include "GameEngine/Math/Functions/MathConstants.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Utility/JsonValue.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {

void MenuSelectionAnimationComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   for (auto& option : options_) {
      option.captured = false;
   }

   OnReferencesChanged(sceneWorld);
   Hide();
}

void MenuSelectionAnimationComponent::OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) {
   for (std::size_t index = 0; index < options_.size(); ++index) {
      auto& option = options_[index];
      auto* object = sceneWorld.FindObjectById(option.objectId);
      const auto* text = object ? object->GetComponent<GameEngine::UITextComponent>() : nullptr;
      const auto* transform = object ? object->GetComponent<GameEngine::TransformComponent>() : nullptr;
      bool duplicate = false;

      for (std::size_t previous = 0; previous < index; ++previous) {
         duplicate = duplicate || options_[previous].objectId == option.objectId;
      }

      if (!text || !transform || duplicate) {
         option.captured = false;
         continue;
      }

      // 参照の再解決だけで演出中の倍率を基準値に取り込まない。
      if (!option.captured) {
         option.baseScale = transform->GetLocalPose().scale;
         option.baseOpacity = text->GetStyle().color.w;
         option.captured = true;
      }
   }
}

void MenuSelectionAnimationComponent::Update(float deltaTime) {
   if (state_ == State::Hidden) return;

   // 通知元とこのコンポーネントの更新順序にかかわらず、演出の開始値を一度表示する。
   const float step = skipAdvance_ ? 0.0f : std::max(deltaTime, 0.0f);
   skipAdvance_ = false;

   if (state_ == State::Selecting) {
      ApplySelection(EvaluateSelectionScale(step));
   } else {
      elapsed_ = std::min(elapsed_ + step, reactionDuration_);
      ApplyReaction();
   }
}

bool MenuSelectionAnimationComponent::Select(std::int32_t optionIndex, bool animateSwitch) {
   if (optionIndex < 0 || !IsOptionAvailable(static_cast<std::size_t>(optionIndex))) {
      return false;
   }

   if (state_ == State::Selecting && selectedOption_ == optionIndex) {
      return true;
   }

   selectedOption_ = optionIndex;
   state_ = State::Selecting;
   elapsed_ = 0.0f;
   switching_ = animateSwitch;
   skipAdvance_ = true;
   ApplySelection(EvaluateSelectionScale(0.0f));
   return true;
}

bool MenuSelectionAnimationComponent::ConfirmSelection() {
   if (state_ != State::Selecting || selectedOption_ < 0 ||
       !IsOptionAvailable(static_cast<std::size_t>(selectedOption_))) {
      return false;
   }

   const auto* transform =
      FindOptionObject(static_cast<std::size_t>(selectedOption_))->GetComponent<GameEngine::TransformComponent>();
   reactionStartScale_ = transform->GetLocalPose().scale;
   state_ = State::Reacting;
   elapsed_ = 0.0f;
   skipAdvance_ = true;
   return true;
}

void MenuSelectionAnimationComponent::Hide() {
   state_ = State::Hidden;
   selectedOption_ = -1;
   elapsed_ = 0.0f;
   switching_ = false;
   skipAdvance_ = false;

   for (std::size_t index = 0; index < options_.size(); ++index) {
      auto* text = GetOptionText(index);

      if (!text) {
         continue;
      }

      text->SetText("");
      text->SetOpacity(options_[index].baseOpacity);
      auto* transform = FindOptionObject(index)->GetComponent<GameEngine::TransformComponent>();
      auto pose = transform->GetLocalPose();
      pose.scale = options_[index].baseScale;
      transform->ApplyLocalPose(pose);
   }
}

bool MenuSelectionAnimationComponent::IsOptionAvailable(std::size_t optionIndex) const {
   const auto* object = FindOptionObject(optionIndex);
   return object && options_[optionIndex].captured && object->GetComponent<GameEngine::UITextComponent>() &&
          object->GetComponent<GameEngine::TransformComponent>();
}

GameEngine::UITextComponent* MenuSelectionAnimationComponent::GetOptionText(std::size_t optionIndex) const {
   return IsOptionAvailable(optionIndex) ? FindOptionObject(optionIndex)->GetComponent<GameEngine::UITextComponent>()
                                         : nullptr;
}

GameEngine::Object* MenuSelectionAnimationComponent::FindOptionObject(std::size_t optionIndex) const {
   if (!HasOwner() || !GetOwner().GetWorld() || optionIndex >= options_.size() ||
       options_[optionIndex].objectId.empty()) {
      return nullptr;
   }

   // EntityやTextが削除されても古いポインターを保持しないよう、所有Worldの安定IDから参照する。
   return GetOwner().GetWorld()->FindObjectById(options_[optionIndex].objectId);
}

float MenuSelectionAnimationComponent::EvaluateSelectionScale(float deltaTime) {
   elapsed_ += deltaTime;

   if (switching_) {
      const float progress = std::clamp(elapsed_ / switchDuration_, 0.0f, 1.0f);
      const float scale = GameEngine::Easing::EaseOutCubic(switchScale_, 1.0f, progress);

      if (progress >= 1.0f) {
         switching_ = false;
         elapsed_ = 0.0f;
      }

      return scale;
   }

   elapsed_ = std::fmod(elapsed_, pulseDuration_);
   const float phase = elapsed_ / pulseDuration_;
   const float progress = 0.5f - 0.5f * std::cos(GameEngine::MathConstants::kTwoPi * phase);
   return GameEngine::Easing::EaseInOutSine(1.0f, pulseScale_, progress);
}

void MenuSelectionAnimationComponent::ApplySelection(float scale) {
   for (std::size_t index = 0; index < options_.size(); ++index) {
      auto* text = GetOptionText(index);

      if (!text) {
         continue;
      }

      auto* transform = FindOptionObject(index)->GetComponent<GameEngine::TransformComponent>();
      const auto& option = options_[index];
      const float multiplier =
         selectedOption_ >= 0 && index == static_cast<std::size_t>(selectedOption_) ? scale : 1.0f;
      auto pose = transform->GetLocalPose();
      // 毎回基準倍率から計算し、未選択側へ直前の選択演出を残さない。
      pose.scale = {option.baseScale.x * multiplier, option.baseScale.y * multiplier, option.baseScale.z};
      transform->ApplyLocalPose(pose);
      text->SetOpacity(option.baseOpacity);
   }
}

void MenuSelectionAnimationComponent::ApplyReaction() {
   if (selectedOption_ < 0) return;

   const auto index = static_cast<std::size_t>(selectedOption_);
   auto* text = GetOptionText(index);

   if (!text) return;

   auto* transform = FindOptionObject(index)->GetComponent<GameEngine::TransformComponent>();
   const auto& option = options_[index];
   const float progress = std::clamp(elapsed_ / reactionDuration_, 0.0f, 1.0f);
   const float scaleProgress = GameEngine::Easing::EaseOutCubic(0.0f, 1.0f, progress);
   auto pose = transform->GetLocalPose();
   // 決定時点の倍率を始点に固定し、反復補間による誤差の蓄積や基準倍率への跳ね戻りを防ぐ。
   pose.scale = {
      reactionStartScale_.x + (option.baseScale.x * reactionEndScale_ - reactionStartScale_.x) * scaleProgress,
      reactionStartScale_.y + (option.baseScale.y * reactionEndScale_ - reactionStartScale_.y) * scaleProgress,
      option.baseScale.z};
   text->SetOpacity(GameEngine::Easing::EaseInQuad(option.baseOpacity, 0.0f, progress));
   transform->ApplyLocalPose(pose);
}

nlohmann::json MenuSelectionAnimationComponent::Serialize() const {
   auto objectIds = nlohmann::json::array();

   for (const auto& option : options_) {
      objectIds.push_back(option.objectId);
   }

   return nlohmann::json{{"optionObjectIds", objectIds}, {"pulseDuration", pulseDuration_}, {"pulseScale", pulseScale_},
      {"switchDuration", switchDuration_}, {"switchScale", switchScale_}, {"reactionDuration", reactionDuration_},
      {"reactionEndScale", reactionEndScale_}};
}

void MenuSelectionAnimationComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) return;

   if (const auto it = data.find("optionObjectIds"); it != data.end() && it->is_array()) {
      std::vector<OptionVisual> options;

      for (const auto& objectId : *it) {
         OptionVisual option;
         option.objectId = objectId.is_string() ? objectId.get<std::string>() : "";
         const auto previous = std::find_if(options_.begin(), options_.end(),
            [&option](const OptionVisual& current) { return current.objectId == option.objectId; });

         if (previous != options_.end()) {
            option = *previous;
         }

         options.push_back(std::move(option));
      }

      const bool unchanged =
         options.size() == options_.size() &&
         std::equal(options.begin(), options.end(), options_.begin(),
            [](const OptionVisual& left, const OptionVisual& right) { return left.objectId == right.objectId; });

      if (!unchanged) {
         Hide();
         options_ = std::move(options);
      }
   }

   pulseDuration_ = GameEngine::JsonValue::ReadFloat(data, "pulseDuration", pulseDuration_);
   pulseScale_ = GameEngine::JsonValue::ReadFloat(data, "pulseScale", pulseScale_);
   switchDuration_ = GameEngine::JsonValue::ReadFloat(data, "switchDuration", switchDuration_);
   switchScale_ = GameEngine::JsonValue::ReadFloat(data, "switchScale", switchScale_);
   reactionDuration_ = GameEngine::JsonValue::ReadFloat(data, "reactionDuration", reactionDuration_);
   reactionEndScale_ = GameEngine::JsonValue::ReadFloat(data, "reactionEndScale", reactionEndScale_);
   ValidateSettings();
}

void MenuSelectionAnimationComponent::ValidateSettings() {
   // 時間は除数として正値を保証し、外部JSON由来の非有限値を演出計算へ流さない。
   pulseDuration_ = std::isfinite(pulseDuration_) ? std::max(pulseDuration_, 0.0001f) : 0.9f;
   pulseScale_ = std::isfinite(pulseScale_) ? std::max(pulseScale_, 1.0f) : 1.1f;
   switchDuration_ = std::isfinite(switchDuration_) ? std::max(switchDuration_, 0.0001f) : 0.16f;
   switchScale_ = std::isfinite(switchScale_) ? std::clamp(switchScale_, 0.0f, 1.0f) : 0.82f;
   reactionDuration_ = std::isfinite(reactionDuration_) ? std::max(reactionDuration_, 0.0001f) : 0.4f;
   reactionEndScale_ = std::isfinite(reactionEndScale_) ? std::max(reactionEndScale_, 1.0f) : 1.2f;
}

#ifdef USE_IMGUI
void MenuSelectionAnimationComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!ImGui::CollapsingHeader(header.c_str())) return;

   for (std::size_t index = 0; index < options_.size(); ++index) {
      const std::string label = "Option " + std::to_string(index + 1);
      std::string objectId = options_[index].objectId;

      if (GameEngine::EditorUI::ObjectReference(label.c_str(), objectId, "UITextComponent")) {
         Hide();
         options_[index].objectId = std::move(objectId);
         options_[index].captured = false;
      }
   }

   if (ImGui::Button("Add Option")) {
      options_.emplace_back();
      GameEngine::EditorUI::MarkChanged();
   }

   if (!options_.empty()) {
      ImGui::SameLine();

      if (ImGui::Button("Remove Last Option")) {
         Hide();
         options_.pop_back();
         GameEngine::EditorUI::MarkChanged();
      }
   }

   ImGui::DragFloat("Pulse Duration", &pulseDuration_, 0.01f, 0.01f, 3.0f, "%.2f s");
   ImGui::DragFloat("Pulse Scale", &pulseScale_, 0.01f, 1.0f, 2.0f, "%.2f");
   ImGui::DragFloat("Switch Duration", &switchDuration_, 0.01f, 0.01f, 1.0f, "%.2f s");
   ImGui::DragFloat("Switch Scale", &switchScale_, 0.01f, 0.0f, 1.0f, "%.2f");
   ImGui::DragFloat("Reaction Duration", &reactionDuration_, 0.01f, 0.01f, 1.0f, "%.2f s");
   ImGui::DragFloat("Reaction End Scale", &reactionEndScale_, 0.01f, 1.0f, 3.0f, "%.2f");
   ValidateSettings();
}
#endif

} // namespace App
