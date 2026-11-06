#include "GameProject/App/Component/Race/RaceManagerComponent.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include <cstdint>

#include "GameEngine/Utility/JsonValue.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Scene/Camera/Core/CinemachineBrain.h"
#include <algorithm>
#include <array>
#include <cmath>

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace {
constexpr std::array<const char*, 4> kCameraBlendEasingNames = {"Linear", "EaseIn", "EaseOut", "EaseInOut"};
}

namespace App {

RaceManagerComponent::RaceManagerComponent() {
   ConfigureStates();
}

void RaceManagerComponent::ConfigureStates() {
   stateMachine_.AddState(State::Waiting, {.onEnter = [this]() {
      presentation_.SetCameraMotionPaused(
         GameEngine::EngineContext::GetActiveBrain(), false, startCameraBlendSeconds_, startCameraBlendEasing_);
      presentation_.SetPlayerLocked(false);
   }});
   stateMachine_.AddState(State::Intro,
      {.onEnter =
            [this]() {
               if (!presentation_.BeginIntro(GameEngine::EngineContext::GetActiveBrain(), introSettings_)) {
                  stateMachine_.RequestState(State::Countdown);
               }
            },
         .onUpdate = [this](float seconds) { UpdateIntro(seconds); },
         .onExit =
            [this]() {
               presentation_.CancelIntro(GameEngine::EngineContext::GetActiveBrain());
               GameEngine::EngineContext::SetCameraTransitionOpacity(0.0f);
            }});
   stateMachine_.AddState(State::Countdown, {.onEnter = [this]() { EnterCountdown(); },
                                               .onUpdate =
                                                  [this](float seconds) {
                                                     countdownRemaining_ -= seconds;
                                                     if (countdownRemaining_ <= 0.0f) {
                                                        showStartBannerOnEntry_ = true;
                                                        stateMachine_.RequestState(State::Running);
                                                     }
                                                  }});
   stateMachine_.AddState(State::Running, {.onEnter = [this]() { EnterRunning(); },
                                             .onUpdate =
                                                [this](float seconds) {
                                                   elapsedTime_ += seconds;
                                                   startBannerRemaining_ =
                                                      std::max(startBannerRemaining_ - seconds, 0.0f);
                                                }});
   stateMachine_.AddState(State::Finished,
      {.onEnter = [this]() { EnterFinished(); }, .onUpdate = [this](float seconds) { UpdateFinished(seconds); }});
   stateMachine_.SetTransitionRule(State::Waiting, {State::Running});
   stateMachine_.SetTransitionRule(State::Intro, {State::Countdown});
   stateMachine_.SetTransitionRule(State::Countdown, {State::Running});
   stateMachine_.SetTransitionRule(State::Running, {State::Finished});
   stateMachine_.SetTransitionRule(State::Finished, {});
}

void RaceManagerComponent::Update(float deltaTime) {
   if (!runtimeInitialized_) {
      // Edit中のシーン読込ではPlayerを変更せず、最初のランタイム更新でのみレース用ロックを開始する。
      runtimeInitialized_ = true;
      Restart();
      return;
   }

   stateMachine_.Update(deltaTime);

   if (restartAfterUpdate_) {
      restartAfterUpdate_ = false;
      Restart();
      return;
   }

   // 状態に入ったフレームでは次の時計を進めず、カウントダウンを走行時間へ混ぜない。
   stateMachine_.ResolveRequests();
}

void RaceManagerComponent::UpdateIntro(float deltaTime) {
   if (!introSettings_.enabled || startMode_ != StartMode::Countdown) {
      restartAfterUpdate_ = true;
      return;
   }

   // 車両入力はロック中なので、UIアクションを直接参照する。入場前の決定は持ち越さない。
   const auto& confirm = GameEngine::EngineContext::GetInputActionState("UI", "UI.Confirm", 0);

   if (!confirm.held) {
      introSkipArmed_ = true;
   }

   auto* brain = GameEngine::EngineContext::GetActiveBrain();
   const auto frame = presentation_.UpdateIntro(
      brain, deltaTime, introSkipArmed_ && confirm.triggered, GameEngine::EngineContext::IsSceneTransitioning());
   GameEngine::EngineContext::SetCameraTransitionOpacity(frame.opacity);

   if (frame.switchToCountdown && useCountdownCameraPose_) {
      presentation_.ApplyCountdownCameraPose(
         brain, countdownCameraPosition_, countdownCameraRotation_, countdownCameraFov_);
   }

   if (frame.completed) {
      stateMachine_.RequestState(State::Countdown);
   }
}

void RaceManagerComponent::UpdateFinished(float deltaTime) {
   // リスタートを選んだ場合は自動遷移より優先し、二重のシーン要求を防ぐ。
   if (!restartRequested_ && !nextScene_.empty() && !sceneChangeRequested_) {
      finishElapsed_ += deltaTime;

      if (finishElapsed_ >= finishDelay_) {
         GameEngine::EngineContext::ChangeScene(nextScene_);
         sceneChangeRequested_ = true;
      }
   }
}

void RaceManagerComponent::OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) {
   presentation_.Resolve(sceneWorld, playerObjectId_);
}

void RaceManagerComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   OnReferencesChanged(sceneWorld);
   runtimeInitialized_ = false;
   presentation_.CaptureInitialState();
   records_.LoadBestTimes(recordFile_, recordKey_);
   Restart();
}

void RaceManagerComponent::OnDisable() {
   if (!runtimeInitialized_) return;

   auto* brain = GameEngine::EngineContext::GetActiveBrain();
   presentation_.CancelIntro(brain);
   presentation_.RestoreEffects();
   presentation_.SetPlayerLocked(false);
   presentation_.SetCameraMotionPaused(brain, false, 0.0f, startCameraBlendEasing_);
   GameEngine::EngineContext::SetCameraTransitionOpacity(0.0f);
   stateMachine_.Stop();
   runtimeInitialized_ = false;
   previewState_ = State::Waiting;
}

void RaceManagerComponent::NotifyStart() {
   if (GetState() != State::Waiting) return;

   showStartBannerOnEntry_ = true;
   stateMachine_.RequestState(State::Running);
   stateMachine_.ResolveRequests();
}

void RaceManagerComponent::NotifyCheckpoint(size_t checkpointIndex) {
   // 次に期待する番号だけを受理し、逆走やゲートの往復で進捗を飛ばさない。
   if (GetState() == State::Running && checkpointIndex == nextCheckpointIndex_ && checkpointIndex < checkpointCount_) {
      ++nextCheckpointIndex_;
   }
}

void RaceManagerComponent::NotifyFinish() {
   if (GetState() != State::Running || !CanFinish()) return;

   stateMachine_.RequestState(State::Finished);
   stateMachine_.ResolveRequests();
}

void RaceManagerComponent::EnterFinished() {
   startBannerRemaining_ = 0.0f;
   finishElapsed_ = 0.0f;
   records_.AddBestTime(elapsedTime_);
   records_.SaveBestTimes(recordFile_, recordKey_);

   if (lockPlayerOnFinish_) {
      presentation_.SetPlayerLocked(true);
   }

   presentation_.ShowFinish(finishCameraId_);
}

void RaceManagerComponent::NotifyStartFinish() {
   if (GetState() == State::Waiting) {
      // 共用ゲートへの最初の進入はスタートとして扱う。
      NotifyStart();
      return;
   }

   if (GetState() == State::Running && startGateExited_) {
      // スタート後に一度ゲート外へ出るまで、同じ重なりをゴールとして受理しない。
      NotifyFinish();
   }
}

void RaceManagerComponent::NotifyStartGateExit() {
   if (GetState() == State::Running) {
      startGateExited_ = true;
   }
}

void RaceManagerComponent::Restart() {
   // 再開始は通常の遷移規則と分け、新しいセッションとして任意の開始方式へ戻す。
   stateMachine_.Stop();
   elapsedTime_ = 0.0f;
   countdownRemaining_ = countdownSeconds_;
   startBannerRemaining_ = 0.0f;
   nextCheckpointIndex_ = 0;
   startGateExited_ = false;
   finishElapsed_ = 0.0f;
   sceneChangeRequested_ = false;
   restartRequested_ = false;
   restartAfterUpdate_ = false;
   introSkipArmed_ = false;
   showStartBannerOnEntry_ = startMode_ != StartMode::Immediate;

   // 編集時の読み込みは状態の準備だけに留め、カメラやプレイヤーへ実行時の変更を加えない。
   previewState_ = startMode_ == StartMode::Countdown ? State::Countdown : State::Waiting;

   if (!runtimeInitialized_) return;

   presentation_.CancelIntro(GameEngine::EngineContext::GetActiveBrain());
   GameEngine::EngineContext::SetCameraTransitionOpacity(0.0f);
   presentation_.RestoreEffects();
   const State initial = startMode_ == StartMode::Immediate ? State::Running
                         : startMode_ == StartMode::Countdown
                            ? (introSettings_.enabled ? State::Intro : State::Countdown)
                            : State::Waiting;
   stateMachine_.Start(initial);

   // 導入カメラの不足時も同じフレームでCountdownへフォールバックする。
   stateMachine_.ResolveRequests();
}

bool RaceManagerComponent::RequestRestart() {
   if (GetState() != State::Finished || restartRequested_ || restartScene_.empty()) {
      return false;
   }

   restartRequested_ = true;
   GameEngine::EngineContext::ChangeScene(restartScene_);
   return true;
}

nlohmann::json RaceManagerComponent::Serialize() const {
   const char* startModeName = "Countdown";

   if (startMode_ == StartMode::Immediate) {
      startModeName = "Immediate";
   } else if (startMode_ == StartMode::Gate) {
      startModeName = "Gate";
   }

   return nlohmann::json{{"startMode", startModeName}, {"countdownSeconds", countdownSeconds_},
      {"intro", introSettings_.Serialize()}, {"useCountdownCameraPose", useCountdownCameraPose_},
      {"countdownCameraPosition", GameEngine::JsonValue::WriteVector3(countdownCameraPosition_)},
      {"countdownCameraRotation", GameEngine::JsonValue::WriteVector3(countdownCameraRotation_)},
      {"countdownCameraFov", countdownCameraFov_}, {"startCameraBlendSeconds", startCameraBlendSeconds_},
      {"startCameraBlendEasing", kCameraBlendEasingNames[static_cast<size_t>(startCameraBlendEasing_)]},
      {"startTextDuration", startTextDuration_}, {"checkpointCount", checkpointCount_},
      {"playerObjectId", playerObjectId_}, {"finishCameraId", finishCameraId_}, {"restartScene", restartScene_},
      {"recordKey", recordKey_}, {"recordFile", recordFile_}, {"lockPlayerDuringCountdown", lockPlayerDuringCountdown_},
      {"lockPlayerOnFinish", lockPlayerOnFinish_}, {"nextScene", nextScene_}, {"finishDelay", finishDelay_}};
}

nlohmann::json RaceManagerComponent::GetSharedParameters() const {
   return ExtractSharedParameters(
      {"playerObjectId", "finishCameraId", "restartScene", "nextScene", "recordKey", "recordFile", "intro"});
}

void RaceManagerComponent::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void RaceManagerComponent::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void RaceManagerComponent::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   if (!data.is_object()) return;

   if (data.contains("intro")) {
      introSettings_.Deserialize(data.at("intro"));
   }

   useCountdownCameraPose_ = GameEngine::JsonValue::ReadBool(data, "useCountdownCameraPose", useCountdownCameraPose_);

   if (data.contains("countdownCameraPosition")) {
      countdownCameraPosition_ =
         GameEngine::JsonValue::ReadVector3(data.at("countdownCameraPosition"), countdownCameraPosition_);
   }

   if (data.contains("countdownCameraRotation")) {
      countdownCameraRotation_ =
         GameEngine::JsonValue::ReadVector3(data.at("countdownCameraRotation"), countdownCameraRotation_);
   }

   countdownCameraFov_ = std::clamp(
      GameEngine::JsonValue::ReadFloat(data, "countdownCameraFov", countdownCameraFov_), 0.017453292f, 3.12413936f);
   startCameraBlendSeconds_ =
      std::max(GameEngine::JsonValue::ReadFloat(data, "startCameraBlendSeconds", startCameraBlendSeconds_), 0.0f);

   if (data.contains("startCameraBlendEasing") && data.at("startCameraBlendEasing").is_string()) {
      for (size_t index = 0; index < kCameraBlendEasingNames.size(); ++index) {
         if (data.at("startCameraBlendEasing") == kCameraBlendEasingNames[index]) {
            startCameraBlendEasing_ = static_cast<GameEngine::CinemachineBrain::BlendEasing>(index);
            break;
         }
      }
   }

   if (data.contains("startMode") && data.at("startMode").is_string()) {
      const std::string startMode = data.at("startMode").get<std::string>();

      if (startMode == "Immediate") {
         startMode_ = StartMode::Immediate;
      } else if (startMode == "Gate") {
         startMode_ = StartMode::Gate;
      } else {
         startMode_ = StartMode::Countdown;
      }
   }

   if (data.contains("countdownSeconds") && data.at("countdownSeconds").is_number()) {
      countdownSeconds_ = std::max(data.at("countdownSeconds").get<float>(), 0.0f);
   }

   if (data.contains("startTextDuration") && data.at("startTextDuration").is_number()) {
      startTextDuration_ = std::max(data.at("startTextDuration").get<float>(), 0.0f);
   }

   if (data.contains("checkpointCount") && data.at("checkpointCount").is_number_unsigned()) {
      checkpointCount_ = data.at("checkpointCount").get<size_t>();
   }

   if (data.contains("playerObjectId") && data.at("playerObjectId").is_string()) {
      playerObjectId_ = data.at("playerObjectId").get<std::string>();
   }

   if (data.contains("finishCameraId") && data.at("finishCameraId").is_string()) {
      finishCameraId_ = data.at("finishCameraId").get<std::string>();
   }

   if (data.contains("restartScene") && data.at("restartScene").is_string()) {
      restartScene_ = data.at("restartScene").get<std::string>();
   }

   if (data.contains("recordKey") && data.at("recordKey").is_string()) {
      recordKey_ = data.at("recordKey").get<std::string>();
   }

   if (data.contains("recordFile") && data.at("recordFile").is_string()) {
      recordFile_ = data.at("recordFile").get<std::string>();
   }

   if (data.contains("lockPlayerDuringCountdown") && data.at("lockPlayerDuringCountdown").is_boolean()) {
      lockPlayerDuringCountdown_ = data.at("lockPlayerDuringCountdown").get<bool>();
   }

   if (data.contains("lockPlayerOnFinish") && data.at("lockPlayerOnFinish").is_boolean()) {
      lockPlayerOnFinish_ = data.at("lockPlayerOnFinish").get<bool>();
   }

   if (data.contains("nextScene") && data.at("nextScene").is_string()) {
      nextScene_ = data.at("nextScene").get<std::string>();
   }

   if (data.contains("finishDelay") && data.at("finishDelay").is_number()) {
      finishDelay_ = std::max(data.at("finishDelay").get<float>(), 0.0f);
   }
}

void RaceManagerComponent::EnterCountdown() {
   auto* brain = GameEngine::EngineContext::GetActiveBrain();
   presentation_.CancelIntro(brain);
   GameEngine::EngineContext::SetCameraTransitionOpacity(0.0f);
   presentation_.RestoreEffects();
   countdownRemaining_ = countdownSeconds_;

   if (useCountdownCameraPose_) {
      presentation_.ApplyCountdownCameraPose(
         brain, countdownCameraPosition_, countdownCameraRotation_, countdownCameraFov_);
   }

   presentation_.SetCameraMotionPaused(brain, true, startCameraBlendSeconds_, startCameraBlendEasing_);
   presentation_.SetPlayerLocked(lockPlayerDuringCountdown_);
}

void RaceManagerComponent::EnterRunning() {
   // カウントダウン時間は走行記録へ含めず、開始時点を0秒として計測し直す。
   presentation_.SetCameraMotionPaused(
      GameEngine::EngineContext::GetActiveBrain(), false, startCameraBlendSeconds_, startCameraBlendEasing_);
   elapsedTime_ = 0.0f;
   startBannerRemaining_ = showStartBannerOnEntry_ ? startTextDuration_ : 0.0f;
   nextCheckpointIndex_ = 0;
   startGateExited_ = false;
   presentation_.SetPlayerLocked(false);
}

bool RaceManagerComponent::CanFinish() const {
   return nextCheckpointIndex_ >= checkpointCount_;
}

#ifdef USE_IMGUI
void RaceManagerComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   const char* startModes[] = {"Immediate", "Countdown", "Gate"};
   std::int32_t mode = static_cast<std::int32_t>(startMode_);

   if (ImGui::Combo("Start Mode", &mode, startModes, 3)) {
      startMode_ = static_cast<StartMode>(mode);
   }

   ImGui::DragFloat("Countdown Seconds", &countdownSeconds_, 0.1f, 0.0f, 60.0f);
   DrawIntroInspector();
   ImGui::DragFloat("Start Camera Blend Seconds", &startCameraBlendSeconds_, 0.05f, 0.0f, 10.0f);
   auto easingIndex = static_cast<std::int32_t>(startCameraBlendEasing_);

   if (ImGui::Combo("Start Camera Blend Easing", &easingIndex, kCameraBlendEasingNames.data(),
          static_cast<std::int32_t>(kCameraBlendEasingNames.size()))) {
      startCameraBlendEasing_ = static_cast<GameEngine::CinemachineBrain::BlendEasing>(easingIndex);
   }

   ImGui::Checkbox("Use Countdown Camera Pose", &useCountdownCameraPose_);

   if (ImGui::Button("Capture Current Camera for Countdown")) {
      if (const auto* brain = GameEngine::EngineContext::GetActiveBrain()) {
         // 追従カメラのビュー行列指定もTRSへ正規化し、見えている姿勢を保存する。
         const auto state = GameEngine::CameraState::Lerp(brain->GetCurrentState(), brain->GetCurrentState(), 0.0f);
         countdownCameraPosition_ = state.transform.translation;
         countdownCameraRotation_ = state.transform.GetActiveEuler();
         countdownCameraFov_ = state.fov;
         useCountdownCameraPose_ = true;
      }
   }

   if (useCountdownCameraPose_) {
      ImGui::DragFloat3("Countdown Camera Position", &countdownCameraPosition_.x, 0.1f);
      auto rotationDegrees = countdownCameraRotation_ * GameEngine::MathConstants::kRadiansToDegrees;

      if (ImGui::DragFloat3("Countdown Camera Rotation (deg)", &rotationDegrees.x, 0.1f)) {
         countdownCameraRotation_ = rotationDegrees * GameEngine::MathConstants::kDegreesToRadians;
      }

      ImGui::SliderAngle("Countdown Camera FOV (deg)", &countdownCameraFov_, 1.0f, 179.0f);
      ImGui::TextWrapped("Save the scene to keep the countdown camera pose and start blend time.");
   }

   ImGui::DragFloat("Start Text Duration", &startTextDuration_, 0.05f, 0.0f, 60.0f);
   std::int32_t count = static_cast<std::int32_t>(std::min(checkpointCount_, size_t{100000}));

   if (ImGui::InputInt("Checkpoint Count", &count)) {
      checkpointCount_ = static_cast<size_t>(std::max(count, 0));
   }

   GameEngine::EditorUI::ObjectReference("Player", playerObjectId_, "TransformComponent");
   GameEngine::EditorUI::CameraReference("Finish Camera", finishCameraId_);
   GameEngine::EditorUI::SceneReference("Restart Scene", restartScene_);
   GameEngine::EditorUI::SceneReference("Next Scene", nextScene_);
   GameEngine::ImGuiHelper::DrawInputString("Record Key", recordKey_);
   GameEngine::ImGuiHelper::DrawInputString("Record File", recordFile_);
   ImGui::Checkbox("Lock Player During Countdown", &lockPlayerDuringCountdown_);
   ImGui::Checkbox("Lock Player On Finish", &lockPlayerOnFinish_);
   ImGui::DragFloat("Finish Delay", &finishDelay_, 0.1f, 0.0f, 60.0f);
   static const char* stateNames[] = {"Waiting", "Intro", "Countdown", "Running", "Finished"};
   ImGui::Text("State: %s", stateNames[static_cast<std::int32_t>(GetState())]);
   ImGui::Text("Time: %.3f", elapsedTime_);
   ImGui::Text("Checkpoint: %zu / %zu", nextCheckpointIndex_, checkpointCount_);

   for (size_t index = 0; index < records_.GetBestTimes().size(); ++index) {
      ImGui::Text("Best %zu: %.3f", index + 1, records_.GetBestTimes()[index]);
   }

   if (ImGui::Button("Restart State")) {
      Restart();
   }
}

void RaceManagerComponent::DrawIntroInspector() {
   if (!ImGui::TreeNode(GameEngine::LocalizeEditorText("ステージ紹介", "Stage Intro"))) return;

   ImGui::Checkbox(GameEngine::LocalizeEditorText("ステージ紹介を再生", "Enable Stage Intro"), &introSettings_.enabled);
   GameEngine::EditorUI::CameraReference(
      GameEngine::LocalizeEditorText("導入専用カメラ", "Intro Camera"), introSettings_.cameraId);
   ImGui::DragFloat(GameEngine::LocalizeEditorText("ゴール紹介の移動時間（秒）", "Goal Focus Move Seconds"),
      &introSettings_.goalFocusSeconds, 0.1f, 0.0f, 120.0f);
   ImGui::DragFloat(GameEngine::LocalizeEditorText("移動時間（秒）", "Move Seconds"), &introSettings_.moveSeconds, 0.1f,
      0.0f, 120.0f);
   ImGui::DragFloat(GameEngine::LocalizeEditorText("暗転時間（秒）", "Fade Out Seconds"),
      &introSettings_.fadeOutSeconds, 0.05f, 0.0f, 10.0f);
   ImGui::DragFloat(GameEngine::LocalizeEditorText("明転時間（秒）", "Fade In Seconds"), &introSettings_.fadeInSeconds,
      0.05f, 0.0f, 10.0f);

   auto drawPose = [](const char* id, const char* captureLabel, std::optional<GameEngine::CameraState>& pose) {
      ImGui::PushID(id);

      if (ImGui::Button(captureLabel)) {
         if (const auto* brain = GameEngine::EngineContext::GetActiveBrain()) {
            // ビュー行列指定のカメラもTRSへ揃え、画面で確認した構図をそのまま保存する。
            pose = GameEngine::CameraState::Lerp(brain->GetCurrentState(), brain->GetCurrentState(), 0.0f);
         }
      }

      if (pose) {
         ImGui::DragFloat3(GameEngine::LocalizeEditorText("位置", "Position"), &pose->transform.translation.x, 1.0f);
         auto degrees = pose->transform.GetActiveEuler() * GameEngine::MathConstants::kRadiansToDegrees;

         if (ImGui::DragFloat3(GameEngine::LocalizeEditorText("回転（度）", "Rotation (deg)"), &degrees.x, 0.1f)) {
            pose->transform.SetRotationEuler(degrees * GameEngine::MathConstants::kDegreesToRadians);
         }

         ImGui::SliderAngle(GameEngine::LocalizeEditorText("画角（度）", "FOV (deg)"), &pose->fov, 1.0f, 179.0f);
      } else {
         ImGui::TextUnformatted(GameEngine::LocalizeEditorText("構図が未保存です。", "Pose has not been captured."));
      }

      ImGui::PopID();
   };

   ImGui::Separator();
   ImGui::TextUnformatted(GameEngine::LocalizeEditorText("1. ゴール紹介（横移動）", "1. Goal Focus (Sideways Pass)"));
   drawPose("GoalFocusStartPose",
      GameEngine::LocalizeEditorText(
         "現在のカメラをゴール紹介の始点として保存", "Capture Current Camera for Goal Focus Start"),
      introSettings_.goalFocusStartPose);
   drawPose("GoalFocusEndPose",
      GameEngine::LocalizeEditorText(
         "現在のカメラをゴール紹介の終点として保存", "Capture Current Camera for Goal Focus End"),
      introSettings_.goalFocusEndPose);
   ImGui::Separator();
   ImGui::TextUnformatted(GameEngine::LocalizeEditorText("2. ゴールからスタートへ移動", "2. Goal to Start"));
   drawPose("GoalPose",
      GameEngine::LocalizeEditorText("現在のカメラを移動始点として保存", "Capture Current Camera for Move Start"),
      introSettings_.goalPose);
   drawPose("StartPose",
      GameEngine::LocalizeEditorText("現在のカメラを移動終点として保存", "Capture Current Camera for Move End"),
      introSettings_.startPose);

   if (introSettings_.enabled && !introSettings_.IsValid()) {
      ImGui::TextWrapped(GameEngine::LocalizeEditorText(
         "ゴール紹介と俯瞰移動、それぞれの始点・終点を保存してください。未設定時は導入を省略します。",
         "Capture both endpoints for the goal focus and overview shots. Invalid settings skip the intro."));
   }

   ImGui::TreePop();
}
#endif

} // namespace App
