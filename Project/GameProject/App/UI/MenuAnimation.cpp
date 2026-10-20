#include "GameEngine/pch.h"
#include <cstdint>
#include "GameProject/App/UI/MenuAnimation.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Math/Functions/EasingFunctions.h"
#include "GameEngine/Math/Functions/MathConstants.h"
#include <algorithm>
#include <cmath>

namespace App {

void MenuSelectionAnimation::Reset(bool switching) {
   elapsed_ = 0.0f;
   switchElapsed_ = 0.0f;
   switching_ = switching;
}

float MenuSelectionAnimation::Evaluate(
   float deltaTime, float pulseDuration, float pulseScale, float switchDuration, float switchScale) {
   if (switching_) {
      switchElapsed_ += std::max(deltaTime, 0.0f);
      const float progress = std::clamp(switchElapsed_ / std::max(switchDuration, 0.0001f), 0.0f, 1.0f);
      const float scale = GameEngine::Easing::EaseOutCubic(switchScale, 1.0f, progress);

      if (progress >= 1.0f) {
         switching_ = false;
         elapsed_ = 0.0f;
      }

      return scale;
   }

   elapsed_ += std::max(deltaTime, 0.0f);
   const float duration = std::max(pulseDuration, 0.0001f);
   const float phase = std::fmod(elapsed_, duration) / duration;
   const float progress = 0.5f - 0.5f * std::cos(GameEngine::MathConstants::kTwoPi * phase);
   return GameEngine::Easing::EaseInOutSine(1.0f, pulseScale, progress);
}

void UpdateMenuSelection(std::span<GameEngine::UITextComponent* const> texts,
   std::span<GameEngine::TransformComponent* const> transforms, std::span<const GameEngine::Vector3> scales,
   std::int32_t selected, MenuSelectionAnimation& animation, float deltaTime, float pulseDuration, float pulseScale,
   float switchDuration, float switchScale) {
   if (texts.size() != transforms.size() || scales.size() != transforms.size()) return;

   const bool available =
      selected >= 0 && static_cast<size_t>(selected) < transforms.size() && texts[selected] && transforms[selected];
   const float selectedScale =
      available ? animation.Evaluate(deltaTime, pulseDuration, pulseScale, switchDuration, switchScale) : 1.0f;

   for (size_t index = 0; index < transforms.size(); ++index) {
      if (!texts[index] || !transforms[index]) {
         continue;
      }

      const float scale = index == static_cast<size_t>(selected) ? selectedScale : 1.0f;
      auto pose = transforms[index]->GetLocalPose();
      pose.scale = {scales[index].x * scale, scales[index].y * scale, scales[index].z};
      transforms[index]->ApplyLocalPose(pose);
   }
}

void ApplyMenuReaction(GameEngine::UITextComponent& text, GameEngine::TransformComponent& transform,
   const GameEngine::Vector3& baseScale, const GameEngine::Vector3& startScale, float opacity, float elapsed,
   float duration, float endScale) {
   const float progress = std::clamp(elapsed / std::max(duration, 0.0001f), 0.0f, 1.0f);
   const float scaleProgress = GameEngine::Easing::EaseOutCubic(0.0f, 1.0f, progress);
   auto pose = transform.GetLocalPose();
   pose.scale = {startScale.x + (baseScale.x * endScale - startScale.x) * scaleProgress,
      startScale.y + (baseScale.y * endScale - startScale.y) * scaleProgress, baseScale.z};
   text.SetOpacity(GameEngine::Easing::EaseInQuad(opacity, 0.0f, progress));
   transform.ApplyLocalPose(pose);
}

} // namespace App
