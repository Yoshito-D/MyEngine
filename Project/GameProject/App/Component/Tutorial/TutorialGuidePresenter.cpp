#include "GameProject/App/Component/Tutorial/TutorialGuidePresenter.h"
#include "GameProject/App/Component/Vehicle/VehicleLandingBoost.h"
#include "GameEngine/Object/Component/UI/UIAnimationTypes.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Object.h"
#include <algorithm>
#include <utility>

namespace App {
void TutorialGuidePresenter::Reset(float baseOpacity, float feedbackBaseOpacity) {
   *this = {};
   guideBaseOpacity_ = baseOpacity;
   feedbackBaseOpacity_ = feedbackBaseOpacity;
}

void TutorialGuidePresenter::UpdateGuideText(const TutorialGuideState& state, GameEngine::UITextComponent* guideText_,
   GameEngine::UITextComponent* promptText, GameEngine::UITextComponent* feedbackText, float deltaTime,
   float guideFadeDuration_) {
   if (!guideText_) return;

   if (!displayedPhase_) {
      // 初回はフェードを待たず、読み込み直後から案内を読める状態にする。
      displayedPhase_ = state.phase;
   }

   if (*displayedPhase_ != state.phase) {
      guideFadeState_ = GuideFadeState::FadingOut;
   } else if (guideFadeState_ == GuideFadeState::FadingOut) {
      // フェード中に課題が戻ったときは、現在の透明度から復帰する。
      guideFadeState_ = GuideFadeState::FadingIn;
   }

   const float visibilityStep = std::max(deltaTime, 0.0f) / std::max(guideFadeDuration_, 0.0001f);

   if (guideFadeState_ == GuideFadeState::FadingOut) {
      guideVisibility_ = std::max(guideVisibility_ - visibilityStep, 0.0f);

      if (guideVisibility_ <= 0.0f) {
         // 文面の差し替えを完全透明時に限定し、異なる案内が途中で瞬間的に切り替わるのを隠す。
         displayedPhase_ = state.phase;
         guideFadeState_ = GuideFadeState::FadingIn;
      }
   } else if (guideFadeState_ == GuideFadeState::FadingIn) {
      guideVisibility_ = std::min(guideVisibility_ + visibilityStep, 1.0f);

      if (guideVisibility_ >= 1.0f) {
         guideFadeState_ = GuideFadeState::Visible;
      }
   }

   // 操作の再割当・接続状態・達成回数の変化は、課題を切り替えず表示へ反映する。
   if (*displayedPhase_ == state.phase) {
      auto guide = BuildGuideText(state);
      guideText_->SetText(std::move(guide.body));

      if (promptText) {
         promptText->SetText(std::move(guide.prompt));
      }
   }

   ApplyGuideOpacity(guideText_, promptText, feedbackText);
}

void TutorialGuidePresenter::UpdateLayout(GameEngine::UITextComponent* guideText,
   GameEngine::UITextComponent* promptText, GameEngine::UITextComponent* feedbackText) {
   if (!guideText || !guideText->HasOwner()) return;

   const auto* guideTransform = guideText->GetOwner().GetComponent<GameEngine::TransformComponent>();

   if (!guideTransform) return;

   // 固定座標では長い説明の折り返しで重なるため、描画と同じ計測結果から次の表示位置を決める。
   float nextY = guideTransform->GetLocalPose().translation.y +
      GameEngine::EngineContext::MeasureText(guideText->GetText(), guideText->GetStyle()).y + 24.0f;

   for (auto* text : {promptText, feedbackText}) {
      if (!text || !text->HasOwner()) {
         continue;
      }

      if (auto* transform = text->GetOwner().GetComponent<GameEngine::TransformComponent>()) {
         auto pose = transform->GetLocalPose();
         pose.translation.y = nextY;
         transform->ApplyLocalPose(pose);
      }

      if (!text->GetText().empty()) {
         nextY += GameEngine::EngineContext::MeasureText(text->GetText(), text->GetStyle()).y + 24.0f;
      }
   }
}

void TutorialGuidePresenter::ApplyGuideOpacity(GameEngine::UITextComponent* guideText_,
   GameEngine::UITextComponent* promptText, GameEngine::UITextComponent* feedbackText) {
   if (!guideText_) return;

   const float easedVisibility =
      GameEngine::EvaluateUIEasing(guideVisibility_, GameEngine::UIEasingType::EaseInOutSine);

   // シーン側で調整された基準アルファを保ったまま、遷移分の係数だけを掛ける。
   guideText_->SetOpacity(guideBaseOpacity_ * easedVisibility);

   if (promptText) {
      promptText->SetOpacity(guideBaseOpacity_ * easedVisibility);
   }

   if (feedbackText) {
      feedbackText->SetOpacity(feedbackBaseOpacity_ * easedVisibility);
   }
}

void TutorialGuidePresenter::ShowLandingResult(LandingResult result) {
   switch (result) {
      case LandingResult::Success:
         landingFeedback_ = "着地成功！　地表に姿勢を合わせて、速度を維持できました。";
         break;
      case LandingResult::Normal:
         landingFeedback_ = "着地できました。車体の底を地表と平行にすると、よりきれいに着地できます。";
         break;
      case LandingResult::Failure:
         landingFeedback_ = "車体が傾いていたため減速しました。空中で姿勢を整えて着地しましょう。";
         break;
   }
}

void TutorialGuidePresenter::UpdateFeedback(
   const TutorialGuideState& state, GameEngine::UITextComponent* feedbackText_, float deltaTime) {
   if (!feedbackText_) return;

   driftFeedbackRemaining_ = std::max(0.0f, driftFeedbackRemaining_ - std::max(deltaTime, 0.0f));

   // 次の課題の補足文が古い本文の下へ先に現れないよう、文面と配置の切り替えを完全透明時まで待つ。
   if (guideFadeState_ == GuideFadeState::FadingOut) return;

   std::string feedback;

   if (state.phase == Phase::DriftPractice) {
      if (state.canFireMiniTurbo) {
         feedback = state.controls.usesDriftButton ? "ターボ準備OK！　ドリフト操作を離すと発動！"
                                                  : "ターボ準備OK！　旋回入力を戻すと発動！";
      } else if (state.isDrifting) {
         feedback = state.controls.usesDriftButton ? "ドリフト中…　操作を押し続けてターボをためましょう。"
                                                  : "ドリフト中…　同じ方向へ曲がり続けてターボをためましょう。";
      } else if (driftFeedbackRemaining_ > 0.0f) {
         feedback = driftFeedback_;
      } else {
         feedback = state.controls.usesDriftButton ? "ドリフト操作を押しながら曲がりましょう。"
                                                  : "同じ方向へ曲がり続けると、ドリフトが始まります。";
      }
   } else if (state.phase == Phase::JumpPractice) {
      feedback = state.isAirborne ? "ジャンプ中！　この惑星に着地してみましょう。" : "ジャンプ 0 / 1";
   } else if (state.phase == Phase::AirControlIntro) {
      feedback = landingFeedback_;
   } else if (state.phase == Phase::AirControlPractice) {
      if (state.airRotationPerformed) {
         feedback = "姿勢を整えて、最初の惑星へ着地しましょう。";
      } else if (state.isAirborne) {
         feedback = "ピッチやロールの操作で、機体を回転させましょう。";
      } else {
         feedback = "空中回転 0 / 1　ジャンプして、空中で機体を傾けてみましょう。";
      }
   } else if (state.phase == Phase::PlanetTransfer || state.phase == Phase::Goal) {
      if (state.isAirborne) {
         if (state.aligned) {
            feedback = "姿勢OK！　車体の底と地表を平行に保ちましょう。";
         } else {
            feedback = "ピッチやロールの操作で、姿勢を地表に合わせましょう。";

            if (const auto prompts = state.controls.AirPrompts(); !prompts.empty()) {
               feedback += '\n';
               feedback += prompts;
            }
         }
      } else {
         feedback = landingFeedback_;
      }
   }

   feedbackText_->SetText(feedback);
}

TutorialGuidePresenter::GuideText TutorialGuidePresenter::BuildGuideText(const TutorialGuideState& state) {
   const auto& controls = state.controls;
   const auto next = GameEngine::FormatInputPrompt(controls.confirm, "次へ");
   const auto asLine = [](std::string prompt) {
      if (!prompt.empty()) {
         prompt += '\n';
      }

      return prompt;
   };

   switch (state.phase) {
      case Phase::DrivingIntro:
         return {"地上では自動で前進し、徐々に最高速度へ近づきます。\n"
                 "練習中は、左下の速度メーターを確認しましょう。", next};
      case Phase::DriftIntro: {
         const std::string intro = controls.usesDriftButton
                                      ? "ドリフト操作を押しながら曲がると、ドリフトします。\n"
                                      : "同じ方向へ曲がり続けると、ドリフトします。\n";
         const std::string release = controls.usesDriftButton ? "ドリフト操作を離す" : "旋回入力を戻す";
         return {intro + asLine(controls.DriftPrompt()) +
                    "ターボがたまったら" + release + "と発動！　最高速度へ早く到達できます。", next};
      }
      case Phase::DriftPractice: {
         const std::string release = controls.usesDriftButton ? "ドリフト操作を離し" : "旋回入力を戻し";
         const auto required = std::to_string(state.requiredMiniTurbos);
         return {"ドリフトを続けてから" + release + "、ミニターボを" + required + "回発動しましょう。\n" +
                    asLine(controls.DriftPrompt()) + "ミニターボ " + std::to_string(state.miniTurboCount) + " / " +
                    required, {}};
      }
      case Phase::JumpIntro:
         return {"次はジャンプを練習します。\n" + asLine(controls.JumpPrompt()) +
                    "速度が速いほど遠くへ飛べます。", next};
      case Phase::JumpPractice:
         return {"ジャンプしましょう。\n" + asLine(controls.JumpPrompt()) +
                    "最初の惑星へ着地すると、次の説明へ進みます。", {}};
      case Phase::AirControlIntro:
         return {"空中では機体を前後・左右に回転できます。\n" + asLine(controls.AirPrompts()) +
                    "車体の底を地表と平行にして着地すると速度を維持でき、大きく傾くと減速します。", next};
      case Phase::AirControlPractice:
         return {"ジャンプし、空中で機体を回転させてみましょう。\n" + asLine(controls.JumpPrompt()) +
                    asLine(controls.AirPrompts()) +
                    "機体を傾けてから最初の惑星へ着地すると、次へ進みます。", {}};
      case Phase::GoalGuidanceIntro:
         return {"画面右下のミニマップで、二つ目の惑星にあるゴールと距離を確認しましょう。\n" +
                    GameEngine::FormatInputPrompt(controls.view, "ミニマップの回転"), next};
      case Phase::PlanetTransfer:
         return {"二つ目の惑星に向けてジャンプしましょう。\n" + asLine(controls.JumpPrompt()) +
                    "空中で姿勢を調整し、車体の底を地表に合わせて着地しましょう。", {}};
      case Phase::Goal:
         return {"二つ目の惑星に到着！　ミニマップを見ながら、赤い旗を目指しましょう。", {}};
      case Phase::Complete:
         return {"ゴールに到着しました。ドリフト・ジャンプ・姿勢操作を使ってレースへ！\n"
                 "まもなくゲームを開始します。", {}};
   }

   return {};
}

void TutorialGuidePresenter::ShowDriftResult(bool fired) {
   driftFeedback_ = fired ? "ミニターボ発動！　速度メーターの上昇を確認しましょう。"
                          : "もう少し長くドリフトしてから、旋回入力を戻しましょう。";
   driftFeedbackRemaining_ = 1.5f;
}

void TutorialGuidePresenter::ShowReturnToSource() {
   landingFeedback_ += "\n元の惑星に戻りました。ミニマップを見て、二つ目の惑星へもう一度ジャンプしましょう。";
}
}
