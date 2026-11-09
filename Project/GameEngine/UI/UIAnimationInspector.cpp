#include "GameEngine/pch.h"
#include "GameEngine/UI/UIAnimationComponent.h"

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include "imgui.h"
#include <algorithm>

namespace GameEngine {
void UIAnimationComponent::DrawInspector() {
   if (!ImGui::CollapsingHeader(MakeObjectComponentHeaderLabel(kTypeName).c_str())) return;

   ImGui::PushID(this);
   ImGui::TextWrapped("Canvas配下で使用します。保存した配置に演出を重ね、子要素へも引き継ぎます。");

   if (ImGui::Button("画面の演出を設定")) {
      ApplyScreenPreset();
   }

   ImGui::SameLine();

   if (ImGui::Button("ボタンの演出を設定")) {
      ApplyButtonPreset();
   }

   constexpr const char* kEventLabels[] = {"表示", "非表示", "選択・ホバー", "選択解除", "決定", "常時"};
   auto eventIndex = static_cast<std::int32_t>(previewEvent_);

   if (ImGui::Combo("タイミング", &eventIndex, kEventLabels, static_cast<std::int32_t>(std::size(kEventLabels)))) {
      StopPreview();
      previewEvent_ = static_cast<UIAnimationEvent>(eventIndex);
   }

   auto settings = settings_;
   auto& clip = settings.clips[static_cast<size_t>(previewEvent_)];
   bool changed = ImGui::Checkbox("有効", &clip.enabled);
   constexpr const char* kPresetLabels[] = {"なし", "フェード", "左から", "右から", "上から", "下から", "ポップ",
      "強調", "押し込み", "脈動", "浮遊", "揺れ", "回転"};

   if (ImGui::BeginCombo("プリセット", "演出を選んで適用")) {
      for (size_t index = 0; index < std::size(kPresetLabels); ++index) {
         if (ImGui::Selectable(kPresetLabels[index])) {
            clip = MakePreset(static_cast<UIAnimationPreset>(index), previewEvent_);
            changed = true;
         }
      }

      ImGui::EndCombo();
   }

   changed |= ImGui::DragFloat("再生時間 (秒)", &clip.duration, 0.01f, 0.01f, 60.0f);
   changed |= ImGui::DragFloat("開始待機 (秒)", &clip.delay, 0.01f, 0.0f, 60.0f);
   auto easing = static_cast<std::int32_t>(clip.easing);

   if (ImGui::Combo("補間", &easing, "等速\0なめらか\0減速\0弾む\0")) {
      clip.easing = static_cast<UIEasingType>(easing);
      changed = true;
   }

   if (previewEvent_ == UIAnimationEvent::Focus || previewEvent_ == UIAnimationEvent::Idle) {
      auto playback = static_cast<std::int32_t>(clip.playback);

      if (ImGui::Combo("繰り返し", &playback, "一度\0ループ\0往復\0")) {
         clip.playback = static_cast<UIPlaybackMode>(playback);
         changed = true;
      }
   }

   if (ImGui::TreeNode("キーフレーム")) {
      size_t removeIndex = clip.keys.size();

      for (size_t index = 0; index < clip.keys.size(); ++index) {
         ImGui::PushID(static_cast<std::int32_t>(index));
         auto& key = clip.keys[index];
         ImGui::Separator();
         ImGui::Text("キー %zu", index + 1);
         float seconds = key.time * clip.duration;

         if (ImGui::DragFloat("時刻 (秒)", &seconds, 0.01f, 0.0f, clip.duration)) {
            key.time = clip.duration > 0.0f ? seconds / clip.duration : 0.0f;
            changed = true;
         }

         changed |= ImGui::DragFloat2("移動 (UI px)", &key.pose.offset.x, 1.0f);
         changed |= ImGui::DragFloat2("拡縮倍率", &key.pose.scale.x, 0.01f);
         changed |= ImGuiHelper::DragRadiansAsDegrees("回転 (deg)", key.pose.rotation, 1.0f);
         changed |= ImGui::SliderFloat("不透明度", &key.pose.opacity, 0.0f, 1.0f);

         if (clip.keys.size() > 2 && ImGui::Button("このキーを削除")) {
            removeIndex = index;
         }

         ImGui::PopID();
      }

      if (removeIndex < clip.keys.size()) {
         clip.keys.erase(clip.keys.begin() + static_cast<std::ptrdiff_t>(removeIndex));
         changed = true;
      }

      if (ImGui::Button("中間キーを追加")) {
         const float longestStart = [&] {
            size_t longest = 0;

            for (size_t index = 1; index + 1 < clip.keys.size(); ++index) {
               if (clip.keys[index + 1].time - clip.keys[index].time >
                   clip.keys[longest + 1].time - clip.keys[longest].time) {
                  longest = index;
               }
            }

            return (clip.keys[longest].time + clip.keys[longest + 1].time) * 0.5f;
         }();
         clip.keys.push_back({longestStart, Sample(clip, clip.delay + longestStart * clip.duration)});
         changed = true;
      }

      ImGui::TreePop();
   }

   if (changed) {
      const bool preview = previewActive_;
      const bool playing = previewPlaying_;
      const float seconds = previewTime_;
      Configure(settings);

      if (preview) {
         Preview(previewEvent_, seconds, playing);
      }
   }

   ImGui::Separator();

   if (ImGui::Button(previewPlaying_ ? "プレビューを一時停止" : "プレビューを再生")) {
      const auto& active = settings_.clips[static_cast<size_t>(previewEvent_)];
      const bool finished = EvaluateUIPlayback(previewTime_, active.delay, active.duration, active.playback).finished;
      Preview(previewEvent_, finished ? 0.0f : previewTime_, !previewPlaying_);
   }

   ImGui::SameLine();

   if (ImGui::Button("プレビュー解除")) {
      StopPreview();
   }

   const auto& active = settings_.clips[static_cast<size_t>(previewEvent_)];
   const float cycleDuration = active.duration * (active.playback == UIPlaybackMode::PingPong ? 2.0f : 1.0f);
   float seconds = std::min(previewTime_, active.delay + cycleDuration);

   if (ImGui::SliderFloat("再生位置 (秒)", &seconds, 0.0f, active.delay + cycleDuration)) {
      Preview(previewEvent_, seconds, false);
   }

   ImGui::PopID();
}
}
#endif
