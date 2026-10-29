#include "GameProject/App/Component/Tutorial/TutorialProgressComponent.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Component/Collision/TriggerVolumeComponent.h"
#include "GameEngine/Object/Component/Rendering/RenderComponent.h"
#include "GameEngine/Object/Component/UI/UIAnimationTypes.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Utility/Logger.h"
#include "GameProject/App/Component/Character/CharacterJump.h"
#include "GameProject/App/Component/Character/CharacterLanding.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"
#include "GameProject/App/Component/Gravity/SphericalGravityAttractor.h"
#include "GameProject/App/Component/Race/UI/RaceGoalDirectionHUDComponent.h"
#include "GameProject/App/Component/Race/UI/RaceGoalDistanceTextComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleController.h"
#include "GameProject/App/Component/Vehicle/VehicleDrift.h"
#include "GameProject/App/Component/Vehicle/VehicleInputComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleLandingBoost.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {

void TutorialProgressComponent::OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) {
   playerObject_ = sceneWorld.FindObjectById(playerObjectId_);
   targetPlanet_ = sceneWorld.FindObjectById(targetPlanetObjectId_);
   goalObject_ = sceneWorld.FindObjectById(goalObjectId_);
   characterJump_ = playerObject_ ? playerObject_->GetComponent<CharacterJump>() : nullptr;
   characterLanding_ = playerObject_ ? playerObject_->GetComponent<CharacterLanding>() : nullptr;
   gravityBody_ = playerObject_ ? playerObject_->GetComponent<GravityBody>() : nullptr;
   planetSwitcher_ = playerObject_ ? playerObject_->GetComponent<PlanetSwitcher>() : nullptr;
   vehicleController_ = playerObject_ ? playerObject_->GetComponent<VehicleController>() : nullptr;
   vehicleInput_ = playerObject_ ? playerObject_->GetComponent<VehicleInputComponent>() : nullptr;
   vehicleDrift_ = playerObject_ ? playerObject_->GetComponent<VehicleDrift>() : nullptr;
   landingBoost_ = playerObject_ ? playerObject_->GetComponent<VehicleLandingBoost>() : nullptr;
   guideText_ = HasOwner() ? GetOwner().GetComponent<GameEngine::UITextComponent>() : nullptr;
   auto* feedback = sceneWorld.FindObjectById(feedbackObjectId_);
   feedbackText_ = feedback ? feedback->GetComponent<GameEngine::UITextComponent>() : nullptr;
   auto* distance = sceneWorld.FindObjectById(goalDistanceObjectId_);
   goalDistance_ = distance ? distance->GetComponent<RaceGoalDistanceTextComponent>() : nullptr;
   auto* direction = sceneWorld.FindObjectById(goalDirectionObjectId_);
   goalDirection_ = direction ? direction->GetComponent<RaceGoalDirectionHUDComponent>() : nullptr;
}

void TutorialProgressComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   OnReferencesChanged(sceneWorld);
   controllerEnabledWhenPlaying_ = vehicleController_ && vehicleController_->IsEnabled();
   gravityEnabledWhenPlaying_ = gravityBody_ && gravityBody_->IsEnabled();
   miniTurboCount_ = 0;
   lastMiniTurboFireCount_ = vehicleDrift_ ? vehicleDrift_->GetMiniTurboFireCount() : 0;
   wasDrifting_ = false;
   wasAirborne_ = characterJump_ && characterJump_->IsJumping();
   jumpStarted_ = false;
   sceneChangeRequested_ = false;
   completionElapsed_ = 0.0f;
   runtimeInitialized_ = false;
   landingFeedback_.clear();
   driftFeedback_.clear();
   driftFeedbackRemaining_ = 0.0f;
   displayedText_.clear();
   pendingText_.clear();
   guideBaseOpacity_ = guideText_ ? guideText_->GetStyle().color.w : 1.0f;
   guideVisibility_ = 1.0f;
   guideFadeState_ = GuideFadeState::Visible;
   SetTargetPlanetEnabled(false);
   phase_ = Phase::DrivingIntro;
   if (vehicleInput_) {
      vehicleInput_->SetJumpEnabled(false);
   }

   SetGoalGuidanceEnabled(false);
   UpdateGuideText(0.0f);
   UpdateFeedback(0.0f);

   if (!HasRequiredReferences()) {
      Logger::Warning("Tutorial player, planet, goal, or HUD references are missing.", Logger::LogChannel::Game);
   }
}

bool TutorialProgressComponent::HasRequiredReferences() const {
   return guideText_ && feedbackText_ && playerObject_ && targetPlanet_ && goalObject_ && characterJump_ &&
          characterLanding_ && gravityBody_ && planetSwitcher_ && vehicleController_ && vehicleInput_ &&
          vehicleDrift_ && landingBoost_ && goalDistance_ && goalDirection_ &&
          playerObject_->GetComponent<GameEngine::TransformComponent>() &&
          targetPlanet_->GetComponent<GameEngine::TransformComponent>() &&
          targetPlanet_->GetComponent<SphericalGravityAttractor>() &&
          goalObject_->GetComponent<GameEngine::TransformComponent>() &&
          goalObject_->GetComponent<GameEngine::TriggerVolumeComponent>();
}

bool TutorialProgressComponent::IsGamepadConnected() const {
   return vehicleInput_ && GameEngine::EngineContext::IsGamePadConnected(vehicleInput_->DescribeSettings().playerSlot);
}

bool TutorialProgressComponent::IsOnTargetPlanet() const {
   if (!planetSwitcher_ || !characterLanding_ || !characterJump_ || characterJump_->IsJumping() ||
       !characterLanding_->IsGrounded()) {
      return false;
   }

   GameEngine::Model* receiver = nullptr;
   GameEngine::Vector3 center{};
   float radius = 0.0f;
   return planetSwitcher_->TryGetLandingPlanet(receiver, center, radius) && receiver == targetPlanet_;
}

void TutorialProgressComponent::Update(float deltaTime) {
   if (!runtimeInitialized_) {
      // 編集用のシーンに実行時の有効状態の変更を混入させない。
      runtimeInitialized_ = true;
      SetPhase(Phase::DrivingIntro);
   }

   if (!HasRequiredReferences()) {
      SetPlayerPaused(true);
      SetGoalGuidanceEnabled(false);

      if (guideText_) {
         guideText_->SetText(
            "チュートリアルの参照設定が不足しています。プレイヤー・惑星・ゴール・UIを確認してください。");
      }

      return;
   }

   const bool isAirborne = characterJump_->IsJumping();
   const bool tookOff = isAirborne && !wasAirborne_;
   const bool landed = !isAirborne && wasAirborne_ && characterLanding_->IsGrounded();
   const bool confirm =
      vehicleInput_->IsConfirmTriggered() && guideFadeState_ == GuideFadeState::Visible && pendingText_.empty();

   switch (phase_) {
      case Phase::DrivingIntro:
         if (confirm) {
            SetPhase(Phase::DriftIntro);
         }

         break;
      case Phase::DriftIntro:
         if (confirm) {
            lastMiniTurboFireCount_ = vehicleDrift_->GetMiniTurboFireCount();
            SetPhase(Phase::DriftPractice);
         }

         break;
      case Phase::DriftPractice: {
         const auto fireCount = vehicleDrift_->GetMiniTurboFireCount();

         if (fireCount > lastMiniTurboFireCount_) {
            const auto remaining = static_cast<std::uint64_t>(kRequiredMiniTurbos - miniTurboCount_);
            miniTurboCount_ += static_cast<std::int32_t>(std::min(fireCount - lastMiniTurboFireCount_, remaining));
            driftFeedback_ = "ミニターボ発動！　速度メーターの上昇を確認しましょう。";
            driftFeedbackRemaining_ = 1.5f;
         } else if (wasDrifting_ && !vehicleDrift_->IsDrifting()) {
            driftFeedback_ = "もう少し長くドリフトしてから、ハンドルを戻しましょう。";
            driftFeedbackRemaining_ = 1.5f;
         }

         lastMiniTurboFireCount_ = fireCount;

         if (miniTurboCount_ >= kRequiredMiniTurbos) {
            SetPhase(Phase::JumpIntro);
         }

         break;
      }
      case Phase::JumpIntro:
         if (confirm) {
            jumpStarted_ = false;
            SetPhase(Phase::JumpPractice);
         }

         break;
      case Phase::JumpPractice:
         jumpStarted_ = jumpStarted_ || tookOff;

         if (jumpStarted_ && landed) {
            HandleLandingResult();
            SetPhase(Phase::AirControlIntro);
         }

         break;
      case Phase::AirControlIntro:
         if (confirm && RevealTargetPlanet()) {
            SetPhase(Phase::PlanetTransfer);
         }

         break;
      case Phase::PlanetTransfer:
      case Phase::Goal:
         if (landed) {
            HandleLandingResult();

            if (!IsOnTargetPlanet()) {
               landingFeedback_ += "\n元の惑星に戻りました。矢印の方向へ、もう一度ジャンプしましょう。";
               SetPhase(Phase::PlanetTransfer);
            }
         }

         if (IsOnTargetPlanet()) {
            if (phase_ != Phase::Goal) {
               SetPhase(Phase::Goal);
            }

            // 滞在状態で判定し、着地とトリガー侵入の更新順による取りこぼしを避ける。
            if (goalObject_->GetComponent<GameEngine::TriggerVolumeComponent>()->IsInside()) {
               SetPhase(Phase::Complete);
            }
         }

         break;
      case Phase::Complete:
         completionElapsed_ += std::max(deltaTime, 0.0f);

         // クリア文を読む時間を確保し、遷移要求は一度だけ発行する。
         if (!sceneChangeRequested_ && !nextScene_.empty() && completionElapsed_ >= completionDelay_) {
            sceneChangeRequested_ = true;
            GameEngine::EngineContext::ChangeScene(nextScene_);
         }

         break;
   }

   wasAirborne_ = isAirborne;
   wasDrifting_ = vehicleDrift_->IsDrifting();
   UpdateGuideText(deltaTime);
   UpdateFeedback(deltaTime);
}

void TutorialProgressComponent::SetPhase(Phase phase) {
   phase_ = phase;

   if (phase_ == Phase::Complete) {
      completionElapsed_ = 0.0f;
   }

   // 説明中も自動走行を続け、ゴール到達後のクリア表示で停止する。
   SetPlayerPaused(phase_ == Phase::Complete);

   if (vehicleInput_) {
      vehicleInput_->SetJumpEnabled(
         phase_ == Phase::JumpPractice || phase_ == Phase::PlanetTransfer || phase_ == Phase::Goal);
   }

   SetGoalGuidanceEnabled(phase_ == Phase::PlanetTransfer || phase_ == Phase::Goal);
}

void TutorialProgressComponent::SetPlayerPaused(bool paused) {
   // クリア表示中や参照不足時は、位置を保ったまま走行制御と物理を止める。
   if (gravityBody_) {
      if (paused) {
         gravityBody_->StopMotion();
      }

      gravityBody_->SetEnabled(!paused && gravityEnabledWhenPlaying_);
   }

   if (vehicleController_) {
      vehicleController_->SetEnabled(!paused && controllerEnabledWhenPlaying_);
   }
}

void TutorialProgressComponent::SetTargetPlanetEnabled(bool enabled) {
   if (!targetPlanet_) return;

   if (auto* gravity = targetPlanet_->GetComponent<SphericalGravityAttractor>()) {
      gravity->SetEnabled(enabled);
   }

   if (auto* render = targetPlanet_->GetComponent<GameEngine::RenderComponent>()) {
      auto settings = render->DescribeSettings();
      settings.visible = enabled;
      render->Configure(settings);
   }
}

void TutorialProgressComponent::SetGoalGuidanceEnabled(bool enabled) {
   if (goalDistance_) {
      goalDistance_->SetGuidanceEnabled(enabled);
   }

   if (goalDirection_) {
      goalDirection_->SetGuidanceEnabled(enabled);
   }

   if (goalObject_) {
      if (auto* render = goalObject_->GetComponent<GameEngine::RenderComponent>()) {
         auto settings = render->DescribeSettings();
         // クリア時は誘導と到達判定だけを止め、ゴール本体はシーン遷移まで表示する。
         settings.visible = enabled || phase_ == Phase::Complete;
         render->Configure(settings);
      }

      if (auto* trigger = goalObject_->GetComponent<GameEngine::TriggerVolumeComponent>()) {
         trigger->SetEnabled(enabled);
      }
   }
}

bool TutorialProgressComponent::RevealTargetPlanet() {
   auto* playerTransform = playerObject_->GetComponent<GameEngine::TransformComponent>();
   auto* targetTransform = targetPlanet_->GetComponent<GameEngine::TransformComponent>();
   auto* goalTransform = goalObject_->GetComponent<GameEngine::TransformComponent>();
   GameEngine::Vector3 sourceCenter{};
   float sourceRadius = 0.0f;

   if (!playerTransform || !targetTransform || !goalTransform ||
       !planetSwitcher_->TryGetLandingPlanet(sourceCenter, sourceRadius)) {
      return false;
   }

   const auto playerPose = playerTransform->GetLocalPose();
   const auto up = (playerPose.translation - sourceCenter).Normalize();
   auto forward = GameEngine::RotateVector({0.0f, 0.0f, 1.0f}, playerPose.GetActiveQuaternion());
   forward = forward - up * forward.Dot(up);

   if (forward.LengthSquared() < 1.0e-6f) {
      return false;
   }

   forward = forward.Normalize();
   auto targetPose = targetTransform->GetLocalPose();
   const auto scale = targetPose.scale;
   const float targetRadius = 0.5f * std::max({std::abs(scale.x), std::abs(scale.y), std::abs(scale.z)});
   const float minimumCenterDistance = sourceRadius + targetRadius + 10.0f;
   const float minimumForwardDistance =
      std::sqrt(std::max(minimumCenterDistance * minimumCenterDistance - sourceRadius * sourceRadius, 0.0f));
   const float forwardDistance = std::max(targetRadius + transferSurfaceGap_, minimumForwardDistance);

   // 長時間説明を読んだ場合や周回後でも、練習開始時の正面に出現させる。
   // 惑星同士は最低10units離し、元の地表へ重なって出現しないようにする。
   targetPose.translation = sourceCenter + up * sourceRadius + forward * forwardDistance;
   targetTransform->ApplyLocalPose(targetPose);
   const auto goalNormal = (up * 0.6f - forward * 0.8f).Normalize();
   auto goalPose = goalTransform->GetLocalPose();
   // TriggerVolumeは親行列を合成しないため、惑星とゴールをともにルートのワールド座標で配置する。
   goalPose.translation = targetPose.translation + goalNormal * targetRadius;
   // 旗の支柱の+Yを地表の外向き法線へ合わせ、地中へ埋めず地表から垂直に立てる。
   goalPose.SetRotationQuaternion(GameEngine::LookRotation(up * 0.8f + forward * 0.6f, goalNormal));
   goalTransform->ApplyLocalPose(goalPose);
   landingFeedback_.clear();
   SetTargetPlanetEnabled(true);
   return true;
}

nlohmann::json TutorialProgressComponent::Serialize() const {
   return nlohmann::json{{"playerObjectId", playerObjectId_}, {"targetPlanetObjectId", targetPlanetObjectId_},
      {"goalObjectId", goalObjectId_}, {"feedbackObjectId", feedbackObjectId_},
      {"goalDistanceObjectId", goalDistanceObjectId_}, {"goalDirectionObjectId", goalDirectionObjectId_},
      {"nextScene", nextScene_}, {"completionDelay", completionDelay_}, {"guideFadeDuration", guideFadeDuration_},
      {"transferSurfaceGap", transferSurfaceGap_}};
}

nlohmann::json TutorialProgressComponent::GetSharedParameters() const {
   return ExtractSharedParameters({"playerObjectId", "targetPlanetObjectId", "goalObjectId", "feedbackObjectId",
      "goalDistanceObjectId", "goalDirectionObjectId", "nextScene"});
}

void TutorialProgressComponent::Deserialize(const nlohmann::json& data) {
   ReadParameters(data);
}

void TutorialProgressComponent::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data);
}

void TutorialProgressComponent::ReadParameters(const nlohmann::json& data) {
   if (!data.is_object()) return;

   auto readId = [&data](const char* key, std::string& value) {
      if (data.contains(key) && data.at(key).is_string()) {
         value = data.at(key).get<std::string>();
      }
   };
   readId("playerObjectId", playerObjectId_);
   readId("targetPlanetObjectId", targetPlanetObjectId_);
   readId("goalObjectId", goalObjectId_);
   readId("feedbackObjectId", feedbackObjectId_);
   readId("goalDistanceObjectId", goalDistanceObjectId_);
   readId("goalDirectionObjectId", goalDirectionObjectId_);
   readId("nextScene", nextScene_);

   if (data.contains("completionDelay") && data.at("completionDelay").is_number()) {
      const float delay = data.at("completionDelay").get<float>();
      completionDelay_ = std::isfinite(delay) ? std::max(delay, 0.0f) : 2.5f;
   }

   if (data.contains("guideFadeDuration") && data.at("guideFadeDuration").is_number()) {
      const float duration = data.at("guideFadeDuration").get<float>();
      guideFadeDuration_ = std::isfinite(duration) ? std::max(duration, 0.0001f) : 0.2f;
   }

   if (data.contains("transferSurfaceGap") && data.at("transferSurfaceGap").is_number()) {
      const float gap = data.at("transferSurfaceGap").get<float>();
      transferSurfaceGap_ = std::isfinite(gap) ? std::max(gap, 10.0f) : 50.0f;
   }
}

void TutorialProgressComponent::UpdateGuideText(float deltaTime) {
   if (!guideText_) return;

   const std::string guide = BuildGuideText();

   if (displayedText_.empty()) {
      // 初回表示は空文字からのフェードを挟まず、読み込み直後から案内を読める状態にする。
      guideText_->SetText(guide);
      displayedText_ = guide;
      pendingText_.clear();
      guideVisibility_ = 1.0f;
      guideFadeState_ = GuideFadeState::Visible;
      ApplyGuideOpacity();
      return;
   }

   if (guide != displayedText_) {
      // フェード中に案内内容が再更新された場合は、最新の文面だけを表示対象にする。
      pendingText_ = guide;

      if (guideFadeState_ != GuideFadeState::FadingOut) {
         guideFadeState_ = GuideFadeState::FadingOut;
      }
   } else if (guideFadeState_ == GuideFadeState::FadingOut) {
      // 文面が元へ戻った場合も現在の透明度から滑らかに復帰させる。
      pendingText_.clear();
      guideFadeState_ = GuideFadeState::FadingIn;
   }

   const float visibilityStep = std::max(deltaTime, 0.0f) / std::max(guideFadeDuration_, 0.0001f);

   if (guideFadeState_ == GuideFadeState::FadingOut) {
      guideVisibility_ = std::max(guideVisibility_ - visibilityStep, 0.0f);

      if (guideVisibility_ <= 0.0f) {
         // 文面の差し替えを完全透明時に限定し、異なる案内が途中で瞬間的に切り替わるのを隠す。
         guideText_->SetText(pendingText_);
         displayedText_ = std::move(pendingText_);
         pendingText_.clear();
         guideFadeState_ = GuideFadeState::FadingIn;
      }
   } else if (guideFadeState_ == GuideFadeState::FadingIn) {
      guideVisibility_ = std::min(guideVisibility_ + visibilityStep, 1.0f);

      if (guideVisibility_ >= 1.0f) {
         guideFadeState_ = GuideFadeState::Visible;
      }
   }

   ApplyGuideOpacity();
}

void TutorialProgressComponent::ApplyGuideOpacity() {
   if (!guideText_) return;

   const float easedVisibility =
      GameEngine::EvaluateUIEasing(guideVisibility_, GameEngine::UIEasingType::EaseInOutSine);

   // シーン側で調整された基準アルファを保ったまま、遷移分の係数だけを掛ける。
   guideText_->SetOpacity(guideBaseOpacity_ * easedVisibility);
}

void TutorialProgressComponent::HandleLandingResult() {
   switch (landingBoost_->GetLastLandingResult()) {
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

void TutorialProgressComponent::UpdateFeedback(float deltaTime) {
   if (!feedbackText_) return;

   driftFeedbackRemaining_ = std::max(0.0f, driftFeedbackRemaining_ - std::max(deltaTime, 0.0f));
   const bool gamepadConnected = IsGamepadConnected();
   std::string feedback;

   if (phase_ == Phase::DriftPractice) {
      if (vehicleDrift_->CanFireMiniTurbo()) {
         feedback = gamepadConnected ? "ターボ準備OK！　左スティックを中央に戻すと発動！"
                                     : "ターボ準備OK！　A / D を離すと発動！";
      } else if (vehicleDrift_->IsDrifting()) {
         feedback = "ドリフト中…　同じ方向へ曲がり続けてターボをためましょう。";
      } else if (driftFeedbackRemaining_ > 0.0f) {
         feedback = driftFeedback_;
      } else {
         feedback = gamepadConnected ? "左スティックを左右どちらかに倒し続けると、ドリフトが始まります。"
                                     : "A / D を押し続けると、ドリフトが始まります。";
      }
   } else if (phase_ == Phase::JumpPractice) {
      feedback = characterJump_->IsJumping() ? "ジャンプ中！　この惑星に着地してみましょう。" : "ジャンプ 0 / 1";
   } else if (phase_ == Phase::AirControlIntro) {
      feedback = "ジャンプ 1 / 1　" + landingFeedback_;
   } else if (phase_ == Phase::PlanetTransfer || phase_ == Phase::Goal) {
      if (characterJump_->IsJumping()) {
         const auto* transform = playerObject_->GetComponent<GameEngine::TransformComponent>();
         const auto localUp =
            GameEngine::RotateVector({0.0f, 1.0f, 0.0f}, transform->GetLocalPose().GetActiveQuaternion());
         const bool aligned = localUp.Dot(gravityBody_->GetTargetUpVector()) >= 0.95f;
         if (aligned) {
            feedback = "姿勢OK！　車体の底と地表を平行に保ちましょう。";
         } else {
            feedback = gamepadConnected ? "左スティック上下・LB / RB で姿勢を地表に合わせましょう。"
                                        : "W / S・Q / E で姿勢を地表に合わせましょう。";
         }
      } else {
         feedback = landingFeedback_;
      }
   }

   feedbackText_->SetText(feedback);
}

std::string TutorialProgressComponent::BuildGuideText() const {
   const bool gamepadConnected = IsGamepadConnected();
   const std::string jumpButton = gamepadConnected ? "A ボタン" : "SPACE キー";
   const std::string next = "\n" + jumpButton + "：次へ";

   switch (phase_) {
      case Phase::DrivingIntro:
         return "チュートリアル  1 / 8　自動走行と加速\n"
                "地上では自動で前進し、徐々に最高速度へ近づきます。\n"
                "練習中は、左下の速度メーターを確認しましょう。" +
                next;
      case Phase::DriftIntro:
         return "チュートリアル  2 / 8　ドリフトとミニターボ\n" +
                std::string(gamepadConnected ? "左スティックを左右どちらかに倒し続けると、ドリフトします。\n"
                                             : "A / D を押し続けると、ドリフトします。\n") +
                "ターボがたまったらハンドルを戻すと発動！　最高速度へ早く到達できます。" + next;
      case Phase::DriftPractice:
         return "チュートリアル  3 / 8　ドリフトを体験しよう\n"
                "ドリフトを続けてからハンドルを戻し、ミニターボを3回発動しましょう。\n"
                "ミニターボ " +
                std::to_string(miniTurboCount_) + " / 3";
      case Phase::JumpIntro:
         return "チュートリアル  4 / 8　ジャンプ\n"
                "ミニターボ 3 / 3 達成！　次は " +
                jumpButton +
                "でジャンプします。\n"
                "速度が速いほど遠くへ飛べます。" +
                next;
      case Phase::JumpPractice:
         return "チュートリアル  5 / 8　ジャンプを体験しよう\n"
                "一度離してから、" +
                jumpButton +
                "でジャンプしましょう。\n"
                "最初の惑星へ着地すると、次の説明へ進みます。";
      case Phase::AirControlIntro:
         return "チュートリアル  6 / 8　空中の姿勢と着地\n" +
                std::string(gamepadConnected ? "左スティック上下：前後の傾き　LB / RB：左右の傾き\n"
                                             : "W / S：前後の傾き　Q / E：左右の傾き\n") +
                "車体の底を地表と平行にして着地すると速度を維持でき、大きく傾くと減速します。" + next;
      case Phase::PlanetTransfer:
         return "チュートリアル  7 / 8　二つ目の惑星へ！\n"
                "正面に惑星が出現しました。矢印の方向へ " +
                jumpButton +
                "でジャンプ！\n"
                "空中で姿勢を調整し、車体の底を地表に合わせて着地しましょう。";
      case Phase::Goal:
         return "チュートリアル  8 / 8　ゴールへ向かおう\n"
                "二つ目の惑星に到着！　矢印と残り距離を見ながら、赤い旗を目指しましょう。";
      case Phase::Complete:
         return "TUTORIAL COMPLETE！\n"
                "ゴールに到着しました。ドリフト・ジャンプ・姿勢操作を使ってレースへ！\n"
                "まもなくゲームを開始します。";
   }

   return {};
}

const char* TutorialProgressComponent::GetPhaseName() const {
   switch (phase_) {
      case Phase::DrivingIntro:
         return "DrivingIntro";
      case Phase::DriftIntro:
         return "DriftIntro";
      case Phase::DriftPractice:
         return "DriftPractice";
      case Phase::JumpIntro:
         return "JumpIntro";
      case Phase::JumpPractice:
         return "JumpPractice";
      case Phase::AirControlIntro:
         return "AirControlIntro";
      case Phase::PlanetTransfer:
         return "PlanetTransfer";
      case Phase::Goal:
         return "Goal";
      case Phase::Complete:
         return "Complete";
   }

   return "Unknown";
}

#ifdef USE_IMGUI
void TutorialProgressComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   GameEngine::EditorUI::ObjectReference("Player", playerObjectId_, "TransformComponent");
   GameEngine::EditorUI::ObjectReference("Target Planet", targetPlanetObjectId_, "SphericalGravityAttractor");
   GameEngine::EditorUI::ObjectReference("Goal", goalObjectId_, "TriggerVolumeComponent");
   GameEngine::EditorUI::ObjectReference("Feedback Text", feedbackObjectId_, "UITextComponent");
   GameEngine::EditorUI::ObjectReference("Goal Distance", goalDistanceObjectId_, "RaceGoalDistanceTextComponent");
   GameEngine::EditorUI::ObjectReference("Goal Direction", goalDirectionObjectId_, "RaceGoalDirectionHUDComponent");
   GameEngine::EditorUI::SceneReference("Next Scene", nextScene_);
   ImGui::DragFloat("Completion Delay", &completionDelay_, 0.1f, 0.0f, 10.0f, "%.1f s");
   ImGui::DragFloat("Guide Fade Duration", &guideFadeDuration_, 0.01f, 0.01f, 1.0f, "%.2f s");
   ImGui::DragFloat("Transfer Surface Gap", &transferSurfaceGap_, 0.5f, 10.0f, 150.0f);
   ImGui::Text("Phase: %s  Mini Turbo: %d / %d", GetPhaseName(), miniTurboCount_, kRequiredMiniTurbos);
   ImGui::Text("References: %s", HasRequiredReferences() ? "Ready" : "Missing");
}
#endif

} // namespace App
