#include "TutorialProgressComponent.h"

#include "../Character/CharacterJump.h"
#include "../Character/CharacterLanding.h"
#include "../Gravity/PlanetSwitcher.h"
#include "../Vehicle/VehicleInputComponent.h"
#include "../Vehicle/VehicleLandingBoost.h"
#include "Object/Component/UI/UITextComponent.h"
#include "Object/Component/UI/UIAnimationTypes.h"
#include "Object/Object.h"
#include "Scene/BaseScene.h"
#include "Scene/SceneWorld.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void TutorialProgressComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   // シーン再読み込み前の参照を残さず、同じプレイヤーに属する一連の状態提供元を
   // 設定IDからまとめて解決する。案内先UITextだけはこのコンポーネントの所有者から取得する。
   characterJump_ = nullptr;
   characterLanding_ = nullptr;
   planetSwitcher_ = nullptr;
   vehicleInput_ = nullptr;
   landingBoost_ = nullptr;
   guideText_ = HasOwner()
      ? GetOwner().GetComponent<GameEngine::UITextComponent>()
      : nullptr;

   if (auto* playerObject = sceneWorld.FindObjectById(playerObjectId_)) {
      characterJump_ = playerObject->GetComponent<CharacterJump>();
      characterLanding_ = playerObject->GetComponent<CharacterLanding>();
      planetSwitcher_ = playerObject->GetComponent<PlanetSwitcher>();
      vehicleInput_ = playerObject->GetComponent<VehicleInputComponent>();
      landingBoost_ = playerObject->GetComponent<VehicleLandingBoost>();
   }
   // 惑星候補はシーン読み込み後に確定するため、この時点で設定値を実在範囲へ補正する。
   NormalizeTargetPlanetIndex();

   // 進行状況はシーン設定ではなくプレイ中だけの状態として扱い、再読み込み時は最初から開始する。
   phase_ = Phase::Steering;
   completionElapsed_ = 0.0f;
   usedPitch_ = false;
   usedRoll_ = false;
   // 現在の接地状態を基準にして、読み込み直後の接地を新規着地と誤判定しないようにする。
   wasGrounded_ = characterLanding_ ? characterLanding_->IsGrounded() : true;
   sceneChangeRequested_ = false;
   landingFeedback_.clear();
   displayedText_.clear();
   pendingText_.clear();
   // シーンで設定された元のアルファを最大不透明度として保持し、フェードで上書きしない。
   guideBaseOpacity_ = guideText_ ? guideText_->GetStyle().color.w : 1.0f;
   guideVisibility_ = 1.0f;
   guideFadeState_ = GuideFadeState::Visible;
   UpdateGuideText(0.0f);
}

void TutorialProgressComponent::Update(float deltaTime) {
   // いずれかの状態提供元が欠けた構成では一部条件だけで進行させず、
   // 誤った完了やシーン遷移を防ぐためチュートリアル全体を停止する。
   if (!guideText_ || !characterJump_ || !characterLanding_ || !planetSwitcher_ ||
      !vehicleInput_ || !landingBoost_) {
      return;
   }
   // 編集中に候補数が変わっても、比較に無効なインデックスを使わない。
   NormalizeTargetPlanetIndex();

   const bool isGrounded = characterLanding_->IsGrounded();
   const bool isAirborne = characterJump_->IsJumping();
   const bool landedThisFrame = isGrounded && !wasGrounded_;

   // 車両と同じ入力境界を通し、設定済みプレイヤースロットと無効状態を尊重する。
   const float steerInput = vehicleInput_->GetSteerInput();
   const float pitchInput = vehicleInput_->GetPitchInput();
   const float rollInput = vehicleInput_->GetRollInput();

   switch (phase_) {
      case Phase::Steering:
         // 小さなスティックドリフトを操作達成として扱わないようデッドゾーンを設ける。
         if (std::abs(steerInput) >= 0.35f) {
            SetPhase(Phase::Jump);
         }
         break;
      case Phase::Jump:
         if (isAirborne) {
            SetPhase(Phase::AirControl);
         }
         break;
      case Phase::AirControl:
         if (isAirborne) {
            // ピッチとロールは同一フレームで行う必要がないため、空中での達成を個別に保持する。
            usedPitch_ = usedPitch_ || std::abs(pitchInput) >= 0.35f;
            usedRoll_ = usedRoll_ || std::abs(rollInput) >= 0.35f;
            if (usedPitch_ && usedRoll_) {
               SetPhase(Phase::PlanetTransfer);
            }
         }
         break;
      case Phase::PlanetTransfer:
      case Phase::LandingPractice:
         // 接地中の毎フレームではなく空中から接地へ変わった瞬間だけ、直近の着地結果を評価する。
         if (landedThisFrame) {
            HandleLandingResult();
         }
         break;
      case Phase::Complete:
         completionElapsed_ += std::max(deltaTime, 0.0f);
         // 遷移要求は一度だけ発行し、遅延中は完了メッセージを読める時間を確保する。
         if (!sceneChangeRequested_ && !nextScene_.empty() && completionElapsed_ >= completionDelay_) {
            sceneChangeRequested_ = true;
            GameEngine::BaseScene::SetNextSceneName(nextScene_);
         }
         break;
      default:
         break;
   }

   wasGrounded_ = isGrounded;
   UpdateGuideText(deltaTime);
}

nlohmann::json TutorialProgressComponent::Serialize() const {
   // 実行時の達成状況は保存せず、再開時に最初から案内できるよう設定値だけを永続化する。
   // 目標惑星は現在の候補数に合わせた値を保存し、無効なインデックスをシーンへ残さない。
   return nlohmann::json{
      { "playerObjectId", playerObjectId_ },
      { "targetPlanetIndex", GetNormalizedTargetPlanetIndex() },
      { "nextScene", nextScene_ },
      { "completionDelay", completionDelay_ },
      { "guideFadeDuration", guideFadeDuration_ }
   };
}

void TutorialProgressComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   // 各設定を独立して検証し、欠落または型不一致なら既定値・現在値を維持する。
   if (data.contains("playerObjectId") && data.at("playerObjectId").is_string()) {
      playerObjectId_ = data.at("playerObjectId").get<std::string>();
   }
   if (data.contains("targetPlanetIndex") && data.at("targetPlanetIndex").is_number_integer()) {
      targetPlanetIndex_ = std::max(data.at("targetPlanetIndex").get<int>(), 0);
   }
   if (data.contains("nextScene") && data.at("nextScene").is_string()) {
      nextScene_ = data.at("nextScene").get<std::string>();
   }
   if (data.contains("completionDelay") && data.at("completionDelay").is_number()) {
      completionDelay_ = std::max(data.at("completionDelay").get<float>(), 0.0f);
   }
   if (data.contains("guideFadeDuration") && data.at("guideFadeDuration").is_number()) {
      // フェード時間は後段の除数になるため、0以下を許可しない。
      guideFadeDuration_ = std::max(data.at("guideFadeDuration").get<float>(), 0.0001f);
   }
}

void TutorialProgressComponent::SetPhase(Phase phase) {
   if (phase_ == phase) {
      return;
   }
   phase_ = phase;
   if (phase_ == Phase::Complete) {
      // 完了フェーズへ入った瞬間を遷移待ち時間の起点にする。
      completionElapsed_ = 0.0f;
   }
}

int TutorialProgressComponent::GetNormalizedTargetPlanetIndex() const {
   if (!planetSwitcher_) {
      // 参照解決前でも負値だけは除外し、シリアライズやインスペクターから安全に利用できるようにする。
      return std::max(targetPlanetIndex_, 0);
   }
   const int planetCount = planetSwitcher_->GetPlanetCount();
   return planetCount > 0
      ? std::clamp(targetPlanetIndex_, 0, planetCount - 1)
      : 0;
}

void TutorialProgressComponent::NormalizeTargetPlanetIndex() {
   targetPlanetIndex_ = GetNormalizedTargetPlanetIndex();
}

void TutorialProgressComponent::UpdateGuideText(float deltaTime) {
   if (!guideText_) {
      return;
   }
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

   const float visibilityStep =
      std::max(deltaTime, 0.0f) / std::max(guideFadeDuration_, 0.0001f);
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
   if (!guideText_) {
      return;
   }
   const float easedVisibility = GameEngine::EvaluateUIEasing(
      guideVisibility_,
      GameEngine::UIEasingType::EaseInOutSine);
   // シーン側で調整された基準アルファを保ったまま、遷移分の係数だけを掛ける。
   guideText_->SetOpacity(guideBaseOpacity_ * easedVisibility);
}

void TutorialProgressComponent::HandleLandingResult() {
   const int landedPlanetIndex = planetSwitcher_->GetCurrentPlanetIndex();
   // 惑星間移動の課題では着地品質より先に着地先を検証する。
   // 元の惑星へ戻った場合は最終課題のまま再挑戦させ、成功着地でも完了扱いにしない。
   if (phase_ == Phase::PlanetTransfer && landedPlanetIndex != targetPlanetIndex_) {
      landingFeedback_ =
         "元の惑星に戻りました。正面の目標惑星へ向けて、もう一度ジャンプしましょう。";
      return;
   }

   // VehicleLandingBoostが同じ着地イベントで確定した結果を使い、
   // 成功時だけ完了へ進め、未達時は具体的な修正点を示す練習フェーズへ移す。
   switch (landingBoost_->GetLastLandingResult()) {
      case LandingResult::Success:
         landingFeedback_.clear();
         SetPhase(Phase::Complete);
         break;
      case LandingResult::Normal:
         landingFeedback_ =
            "着地判定：NORMAL　機体の底を地表ともっと平行にすると SUCCESS です。";
         SetPhase(Phase::LandingPractice);
         break;
      case LandingResult::Failure:
         landingFeedback_ =
            "着地判定：FAILED　機体が大きく傾いています。姿勢を戻して再挑戦しましょう。";
         SetPhase(Phase::LandingPractice);
         break;
      default:
         break;
   }
}

std::string TutorialProgressComponent::BuildGuideText() const {
   switch (phase_) {
      case Phase::Steering:
         return
            "チュートリアル  1 / 4　走行と旋回\n"
            "機体は自動で前進します。A / D または左スティック左右で旋回してください。";
      case Phase::Jump:
         return
            "チュートリアル  2 / 4　ジャンプ\n"
            "正面の目標惑星へ向かい、SPACE または A ボタンでジャンプしてください。";
      case Phase::AirControl: {
         const std::string pitchStatus = usedPitch_ ? "OK" : "未操作";
         const std::string rollStatus = usedRoll_ ? "OK" : "未操作";
         return
            "チュートリアル  3 / 4　空中姿勢\n"
            "W / S・左スティック上下：ピッチ［" + pitchStatus +
            "］　Q / E・LB / RB：ロール［" + rollStatus + "］";
      }
      case Phase::PlanetTransfer: {
         const std::string retryText = landingFeedback_.empty()
            ? std::string()
            : "\n" + landingFeedback_;
         return
            "チュートリアル  4 / 4　惑星間ジャンプと着地\n"
            "目標の惑星へ着地します。機体の上方向を地表の外側へそろえると SUCCESS です。" +
            retryText;
      }
      case Phase::LandingPractice:
         return
            "チュートリアル  4 / 4　成功着地を体験\n" + landingFeedback_ +
            "\nSPACE / A で再ジャンプし、空中で姿勢を整えて着地してください。";
      case Phase::Complete:
         return
            "TUTORIAL COMPLETE！　着地成功\n"
            "機体の底と地表が平行な状態が SUCCESS です。ゲームを開始します。";
      default:
         return {};
   }
}

const char* TutorialProgressComponent::GetPhaseName() const {
   switch (phase_) {
      case Phase::Steering: return "Steering";
      case Phase::Jump: return "Jump";
      case Phase::AirControl: return "AirControl";
      case Phase::PlanetTransfer: return "PlanetTransfer";
      case Phase::LandingPractice: return "LandingPractice";
      case Phase::Complete: return "Complete";
      default: return "Unknown";
   }
}

#ifdef USE_IMGUI
void TutorialProgressComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Text("Player Object ID: %s", playerObjectId_.c_str());
   ImGui::Text("Next Scene: %s", nextScene_.c_str());
   ImGui::Text("Phase: %s", GetPhaseName());
   const int maxPlanetIndex = planetSwitcher_
      ? std::max(planetSwitcher_->GetPlanetCount() - 1, 0)
      : 0;
   if (ImGui::DragInt("Target Planet Index", &targetPlanetIndex_, 1.0f, 0, maxPlanetIndex)) {
      NormalizeTargetPlanetIndex();
   }
   ImGui::DragFloat("Completion Delay", &completionDelay_, 0.1f, 0.0f, 10.0f, "%.1f s");
   ImGui::DragFloat("Guide Fade Duration", &guideFadeDuration_, 0.01f, 0.01f, 1.0f, "%.2f s");
   ImGui::Text("Resolved: Jump=%s Landing=%s Switcher=%s Input=%s Boost=%s Text=%s",
      characterJump_ ? "true" : "false",
      characterLanding_ ? "true" : "false",
      planetSwitcher_ ? "true" : "false",
      vehicleInput_ ? "true" : "false",
      landingBoost_ ? "true" : "false",
      guideText_ ? "true" : "false");
}
#endif

} // namespace App
