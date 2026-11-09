#include "GameEngine/pch.h"
#include "GameEngine/UI/UIAnimationComponent.h"
#include "GameEngine/Object/Component/Base/ComponentRegistry.h"
#include "GameEngine/Object/Object.h"
#include <algorithm>
#include <cmath>
#include <string_view>

namespace {
using namespace GameEngine;
constexpr float kMinimumDurationSeconds = 0.0001f;
constexpr float kDefaultReturnSeconds = 0.12f;
constexpr float kSlideDistancePixels = 64.0f;
constexpr float kFloatDistancePixels = 8.0f;
constexpr float kShakeDistancePixels = 10.0f;
constexpr float kSelectedScale = 1.06f;
constexpr float kPressedScale = 0.92f;
constexpr float kPopScale = 0.88f;
constexpr std::array<const char*, UIAnimationComponent::kEventCount> kEventNames{
   "show", "hide", "focus", "unfocus", "click", "idle"};

const bool kRegistered = ComponentRegistry::GetInstance().RegisterFactory(
   UIAnimationComponent::kTypeName,
   [](Object& object) -> IObjectComponent* { return object.AddComponent<UIAnimationComponent>(); },
   UIAnimationComponent::kDisplayName, ObjectType::Generic | ObjectType::Sprite | ObjectType::UIText);

float FiniteOr(float value, float fallback) {
   return std::isfinite(value) ? value : fallback;
}

UIAnimationPose Blend(const UIAnimationPose& from, const UIAnimationPose& to, float t) {
   return {{from.offset.x + (to.offset.x - from.offset.x) * t, from.offset.y + (to.offset.y - from.offset.y) * t},
      {from.scale.x + (to.scale.x - from.scale.x) * t, from.scale.y + (to.scale.y - from.scale.y) * t},
      from.rotation + (to.rotation - from.rotation) * t, from.opacity + (to.opacity - from.opacity) * t};
}

UIAnimationPose Combine(UIAnimationPose result, const UIAnimationPose& value) {
   result.offset.x += value.offset.x;
   result.offset.y += value.offset.y;
   result.scale.x *= value.scale.x;
   result.scale.y *= value.scale.y;
   result.rotation += value.rotation;
   result.opacity = std::clamp(result.opacity * value.opacity, 0.0f, 1.0f);
   return result;
}

bool CanRepeat(UIAnimationEvent event) {
   return event == UIAnimationEvent::Focus || event == UIAnimationEvent::Idle;
}
}

namespace GameEngine {
UIAnimationComponent::UIAnimationComponent() {
   ApplyScreenPreset();
}

void UIAnimationComponent::Configure(const Settings& settings) {
   settings_ = settings;

   for (size_t index = 0; index < settings_.clips.size(); ++index) {
      auto& clip = settings_.clips[index];
      clip.duration = std::max(FiniteOr(clip.duration, 0.2f), kMinimumDurationSeconds);
      clip.delay = std::max(FiniteOr(clip.delay, 0.0f), 0.0f);

      if (clip.easing < UIEasingType::Linear || clip.easing > UIEasingType::EaseOutBack) {
         clip.easing = UIEasingType::Linear;
      }

      if (!CanRepeat(static_cast<UIAnimationEvent>(index)) || clip.playback < UIPlaybackMode::Once ||
          clip.playback > UIPlaybackMode::PingPong) {
         clip.playback = UIPlaybackMode::Once;
      }

      for (auto& key : clip.keys) {
         key.time = std::clamp(FiniteOr(key.time, 0.0f), 0.0f, 1.0f);
         key.pose.offset = {FiniteOr(key.pose.offset.x, 0.0f), FiniteOr(key.pose.offset.y, 0.0f)};
         key.pose.scale = {FiniteOr(key.pose.scale.x, 1.0f), FiniteOr(key.pose.scale.y, 1.0f)};
         key.pose.rotation = FiniteOr(key.pose.rotation, 0.0f);
         key.pose.opacity = std::clamp(FiniteOr(key.pose.opacity, 1.0f), 0.0f, 1.0f);
      }

      std::stable_sort(
         clip.keys.begin(), clip.keys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
      std::vector<UIAnimationKeyframe> keys;

      for (const auto& key : clip.keys) {
         if (!keys.empty() && keys.back().time == key.time) {
            keys.back() = key;
         } else {
            keys.push_back(key);
         }
      }

      if (keys.empty()) {
         keys.push_back({0.0f, {}});
      }

      if (keys.front().time > 0.0f) {
         keys.insert(keys.begin(), {0.0f, keys.front().pose});
      }

      if (keys.back().time < 1.0f) {
         keys.push_back({1.0f, keys.back().pose});
      }

      clip.keys = std::move(keys);
   }

   Reset();
}

UIAnimationClip UIAnimationComponent::MakePreset(UIAnimationPreset preset, UIAnimationEvent event) {
   UIAnimationClip clip;
   clip.enabled = preset > UIAnimationPreset::None && preset < UIAnimationPreset::Count;
   const bool leaving = event == UIAnimationEvent::Hide || event == UIAnimationEvent::Unfocus;
   UIAnimationPose altered;

   switch (preset) {
      case UIAnimationPreset::Fade:
         altered.opacity = 0.0f;
         break;
      case UIAnimationPreset::SlideLeft:
      case UIAnimationPreset::SlideRight:
         altered.offset.x = preset == UIAnimationPreset::SlideLeft ? -kSlideDistancePixels : kSlideDistancePixels;
         altered.opacity = 0.0f;
         break;
      case UIAnimationPreset::SlideUp:
      case UIAnimationPreset::SlideDown:
         altered.offset.y = preset == UIAnimationPreset::SlideUp ? -kSlideDistancePixels : kSlideDistancePixels;
         altered.opacity = 0.0f;
         break;
      case UIAnimationPreset::Pop:
         altered.scale = {kPopScale, kPopScale};
         altered.opacity = 0.0f;
         clip.easing = UIEasingType::EaseOutBack;
         break;
      case UIAnimationPreset::Emphasis:
         altered.scale = {kSelectedScale, kSelectedScale};
         clip.keys = leaving ? std::vector<UIAnimationKeyframe>{{0.0f, altered}, {1.0f, {}}}
                             : std::vector<UIAnimationKeyframe>{{0.0f, {}}, {1.0f, altered}};
         return clip;
      case UIAnimationPreset::Press:
         altered.scale = {kPressedScale, kPressedScale};
         clip.duration = 0.16f;
         clip.keys = {{0.0f, {}}, {0.35f, altered}, {1.0f, {}}};
         return clip;
      case UIAnimationPreset::Pulse:
         altered.scale = {kSelectedScale, kSelectedScale};
         clip.duration = 0.65f;
         clip.playback = CanRepeat(event) ? UIPlaybackMode::PingPong : UIPlaybackMode::Once;
         clip.easing = UIEasingType::EaseInOutSine;
         clip.keys = {{0.0f, {}}, {1.0f, altered}};
         return clip;
      case UIAnimationPreset::Float:
         altered.offset.y = -kFloatDistancePixels;
         clip.duration = 0.8f;
         clip.playback = CanRepeat(event) ? UIPlaybackMode::PingPong : UIPlaybackMode::Once;
         clip.easing = UIEasingType::EaseInOutSine;
         clip.keys = {{0.0f, {}}, {1.0f, altered}};
         return clip;
      case UIAnimationPreset::Shake: {
         auto left = altered;
         left.offset.x = -kShakeDistancePixels;
         altered.offset.x = kShakeDistancePixels;
         clip.easing = UIEasingType::Linear;
         clip.keys = {{0.0f, {}}, {0.2f, left}, {0.4f, altered}, {0.6f, left}, {0.8f, altered}, {1.0f, {}}};
         return clip;
      }
      case UIAnimationPreset::Spin:
         altered.rotation = MathConstants::kTwoPi;
         clip.duration = 1.5f;
         clip.easing = UIEasingType::Linear;
         clip.playback = CanRepeat(event) ? UIPlaybackMode::Loop : UIPlaybackMode::Once;
         clip.keys = {{0.0f, {}}, {1.0f, altered}};
         return clip;
      default:
         break;
   }

   clip.keys = leaving ? std::vector<UIAnimationKeyframe>{{0.0f, {}}, {1.0f, altered}}
                       : std::vector<UIAnimationKeyframe>{{0.0f, altered}, {1.0f, {}}};
   return clip;
}

void UIAnimationComponent::ApplyScreenPreset() {
   Settings settings;
   settings.clips[static_cast<size_t>(UIAnimationEvent::Show)] =
      MakePreset(UIAnimationPreset::Pop, UIAnimationEvent::Show);
   settings.clips[static_cast<size_t>(UIAnimationEvent::Hide)] =
      MakePreset(UIAnimationPreset::Fade, UIAnimationEvent::Hide);
   Configure(settings);
}

void UIAnimationComponent::ApplyButtonPreset() {
   Settings settings;
   settings.clips[static_cast<size_t>(UIAnimationEvent::Focus)] =
      MakePreset(UIAnimationPreset::Emphasis, UIAnimationEvent::Focus);
   settings.clips[static_cast<size_t>(UIAnimationEvent::Unfocus)] =
      MakePreset(UIAnimationPreset::Emphasis, UIAnimationEvent::Unfocus);
   settings.clips[static_cast<size_t>(UIAnimationEvent::Click)] =
      MakePreset(UIAnimationPreset::Press, UIAnimationEvent::Click);
   Configure(settings);
}

UIAnimationComponent::Channel UIAnimationComponent::GetChannel(UIAnimationEvent event) {
   switch (event) {
      case UIAnimationEvent::Show:
      case UIAnimationEvent::Hide:
         return Channel::Visibility;
      case UIAnimationEvent::Focus:
      case UIAnimationEvent::Unfocus:
         return Channel::Selection;
      case UIAnimationEvent::Click:
         return Channel::Feedback;
      default:
         return Channel::Ambient;
   }
}

UIAnimationPose UIAnimationComponent::Sample(const UIAnimationClip& clip, float elapsed) {
   const float progress = EvaluateUIPlayback(elapsed, clip.delay, clip.duration, clip.playback).progress;
   const auto next = std::upper_bound(clip.keys.begin(), clip.keys.end(), progress,
      [](float time, const UIAnimationKeyframe& key) { return time < key.time; });

   if (next == clip.keys.begin()) return next->pose;
   if (next == clip.keys.end()) return clip.keys.back().pose;

   const auto& previous = *std::prev(next);
   const float segment = (progress - previous.time) / (next->time - previous.time);
   auto pose = Blend(previous.pose, next->pose, EvaluateUIEasing(segment, clip.easing));
   pose.opacity = std::clamp(pose.opacity, 0.0f, 1.0f);
   return pose;
}

void UIAnimationComponent::Advance(Player& player, float deltaTime) {
   if (!player.playing) return;

   player.elapsed += std::max(FiniteOr(deltaTime, 0.0f), 0.0f);
   player.pose = Sample(player.clip, player.elapsed);

   // 切替時の差だけを最初の片道で解消し、途中の再選択・閉じ直しでも見た目を連続させる。
   if (player.compensateStart) {
      const auto& first = player.clip.keys.front().pose;
      const float remaining =
         1.0f - std::clamp((player.elapsed - player.clip.delay) / player.clip.duration, 0.0f, 1.0f);
      player.pose.offset.x += (player.start.offset.x - first.offset.x) * remaining;
      player.pose.offset.y += (player.start.offset.y - first.offset.y) * remaining;
      player.pose.scale.x += (player.start.scale.x - first.scale.x) * remaining;
      player.pose.scale.y += (player.start.scale.y - first.scale.y) * remaining;
      player.pose.rotation += (player.start.rotation - first.rotation) * remaining;
      player.pose.opacity =
         std::clamp(player.pose.opacity + (player.start.opacity - first.opacity) * remaining, 0.0f, 1.0f);
   }

   if (EvaluateUIPlayback(player.elapsed, player.clip.delay, player.clip.duration, player.clip.playback).finished) {
      player.playing = false;

      if (player.event == UIAnimationEvent::Click) {
         player.pose = {};
      }
   }
}

bool UIAnimationComponent::Start(UIAnimationEvent event, bool fromCurrent) {
   const auto index = static_cast<size_t>(event);

   if (index >= settings_.clips.size() || !settings_.clips[index].enabled) return false;

   auto& player = players_[static_cast<size_t>(GetChannel(event))];
   const auto current = player.pose;
   player = {};
   player.event = event;
   player.clip = settings_.clips[index];
   player.start = current;
   player.compensateStart = fromCurrent;
   player.playing = true;
   Advance(player, 0.0f);
   return true;
}

bool UIAnimationComponent::Play(UIAnimationEvent event) {
   return Start(event);
}

void UIAnimationComponent::Reset() {
   players_ = {};
   selected_ = hiding_ = false;
   StopPreview();
}

UIAnimationPose UIAnimationComponent::GetPose(bool editing) const {
   if (editing) {
      return previewActive_ ? Sample(settings_.clips[static_cast<size_t>(previewEvent_)], previewTime_)
                            : UIAnimationPose{};
   }

   UIAnimationPose pose;

   for (const auto& player : players_) {
      pose = Combine(pose, player.pose);
   }

   return pose;
}

bool UIAnimationComponent::IsPlaying(UIAnimationEvent event) const {
   if (event >= UIAnimationEvent::Count) return false;

   const auto& player = players_[static_cast<size_t>(GetChannel(event))];
   return player.event == event && player.playing;
}

void UIAnimationComponent::NotifyShown() {
   const bool fromCurrent = hiding_;
   const auto current = players_[static_cast<size_t>(Channel::Visibility)].pose;
   Reset();

   if (fromCurrent) {
      players_[static_cast<size_t>(Channel::Visibility)].pose = current;
   }

   Start(UIAnimationEvent::Show, fromCurrent);
   Start(UIAnimationEvent::Idle);
}

void UIAnimationComponent::NotifyHiding() {
   if (hiding_) return;

   hiding_ = true;
   Start(UIAnimationEvent::Hide, true);
}

void UIAnimationComponent::SetSelected(bool selected) {
   if (selected_ == selected || hiding_) return;

   selected_ = selected;

   if (!Start(selected ? UIAnimationEvent::Focus : UIAnimationEvent::Unfocus, true) && !selected) {
      // 選択解除の設定がなくても、選択時の拡大を残さず基準姿勢まで戻す。
      auto& player = players_[static_cast<size_t>(Channel::Selection)];
      const auto current = player.pose;
      player = {};
      player.event = UIAnimationEvent::Unfocus;
      player.clip.duration = kDefaultReturnSeconds;
      player.clip.keys = {{0.0f, current}, {1.0f, {}}};
      player.pose = current;
      player.playing = true;
   }
}

void UIAnimationComponent::OnAttach() {
   NotifyShown();
}

void UIAnimationComponent::OnEnable() {
   NotifyShown();
}

void UIAnimationComponent::OnDisable() {
   Reset();
}

void UIAnimationComponent::OnSceneLoaded(SceneWorld&) {
   NotifyShown();
}

void UIAnimationComponent::Update(float deltaTime) {
   for (auto& player : players_) {
      Advance(player, deltaTime);
   }
}

void UIAnimationComponent::Preview(UIAnimationEvent event, float seconds, bool playing) {
   if (static_cast<size_t>(event) >= settings_.clips.size()) return;

   previewEvent_ = event;
   previewTime_ = std::max(FiniteOr(seconds, 0.0f), 0.0f);
   previewActive_ = settings_.clips[static_cast<size_t>(event)].enabled;
   previewPlaying_ = playing && previewActive_;
}

void UIAnimationComponent::UpdatePreview(float deltaTime) {
   if (!previewActive_ || !previewPlaying_) return;

   previewTime_ += std::max(FiniteOr(deltaTime, 0.0f), 0.0f);
   const auto& clip = settings_.clips[static_cast<size_t>(previewEvent_)];

   if (EvaluateUIPlayback(previewTime_, clip.delay, clip.duration, clip.playback).finished) {
      previewTime_ = clip.delay + clip.duration;
      previewPlaying_ = false;
   }
}

void UIAnimationComponent::StopPreview() {
   previewActive_ = previewPlaying_ = false;
   previewTime_ = 0.0f;
}

nlohmann::json UIAnimationComponent::Serialize() const {
   auto data = nlohmann::json::object();

   for (size_t index = 0; index < settings_.clips.size(); ++index) {
      const auto& clip = settings_.clips[index];
      auto keys = nlohmann::json::array();

      for (const auto& key : clip.keys) {
         keys.push_back({{"time", key.time}, {"offset", {key.pose.offset.x, key.pose.offset.y}},
            {"scale", {key.pose.scale.x, key.pose.scale.y}}, {"rotation", key.pose.rotation},
            {"opacity", key.pose.opacity}});
      }

      data[kEventNames[index]] = {{"enabled", clip.enabled}, {"duration", clip.duration}, {"delay", clip.delay},
         {"easing", static_cast<std::int32_t>(clip.easing)}, {"playback", static_cast<std::int32_t>(clip.playback)},
         {"keys", keys}};
   }

   return data;
}

void UIAnimationComponent::Deserialize(const nlohmann::json& data) {
   Settings settings;

   for (size_t index = 0; index < settings.clips.size(); ++index) {
      if (!data.contains(kEventNames[index])) continue;

      const auto& source = data.at(kEventNames[index]);
      auto& clip = settings.clips[index];
      clip.enabled = source.value("enabled", clip.enabled);
      clip.duration = source.value("duration", clip.duration);
      clip.delay = source.value("delay", clip.delay);
      clip.easing = static_cast<UIEasingType>(source.value("easing", static_cast<std::int32_t>(clip.easing)));
      clip.playback = static_cast<UIPlaybackMode>(source.value("playback", static_cast<std::int32_t>(clip.playback)));

      if (source.contains("keys")) {
         clip.keys.clear();

         for (const auto& entry : source.at("keys")) {
            const auto offset = entry.value("offset", std::array<float, 2>{0.0f, 0.0f});
            const auto scale = entry.value("scale", std::array<float, 2>{1.0f, 1.0f});
            clip.keys.push_back(
               {entry.value("time", 0.0f), {{offset[0], offset[1]}, {scale[0], scale[1]}, entry.value("rotation", 0.0f),
                                              entry.value("opacity", 1.0f)}});
         }
      }
   }

   Configure(settings);
}
}
