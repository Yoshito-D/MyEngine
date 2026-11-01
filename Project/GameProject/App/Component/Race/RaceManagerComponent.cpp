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

void RaceManagerComponent::Update(float deltaTime) {
   if (!runtimeInitialized_) {
      // Edit中のシーン読込ではPlayerを変更せず、最初のランタイム更新でのみレース用ロックを開始する。
      runtimeInitialized_ = true;
      Restart();
      return;
   }

   const float safeDeltaTime = std::isfinite(deltaTime) ? std::max(deltaTime, 0.0f) : 0.0f;

   // 状態ごとに進める時計を分離し、カウントダウンや結果待機を走行時間へ混ぜない。
   switch (state_) {
      case State::Intro: {
         if (!introSettings_.enabled || startMode_ != StartMode::Countdown) {
            Restart();
            break;
         }

         // 車両入力はロック中なので、UIアクションを直接参照する。入場前の決定は持ち越さない。
         const auto& confirm = GameEngine::EngineContext::GetInputActionState("UI", "UI.Confirm", 0);

         if (!confirm.held) {
            introSkipArmed_ = true;
         }

         auto* brain = GameEngine::EngineContext::GetActiveBrain();
         const auto frame = presentation_.UpdateIntro(brain, safeDeltaTime, introSkipArmed_ && confirm.triggered,
            GameEngine::EngineContext::IsSceneTransitioning());
         GameEngine::EngineContext::SetCameraTransitionOpacity(frame.opacity);

         if (frame.switchToCountdown && useCountdownCameraPose_) {
            presentation_.ApplyCountdownCameraPose(
               brain, countdownCameraPosition_, countdownCameraRotation_, countdownCameraFov_);
         }

         if (frame.completed) {
            BeginCountdown();
         }

         break;
      }
      case State::Countdown:
         countdownRemaining_ -= safeDeltaTime;

         if (countdownRemaining_ <= 0.0f) {
            BeginRace();
         }

         break;
      case State::Running:
         elapsedTime_ += safeDeltaTime;
         startBannerRemaining_ = std::max(startBannerRemaining_ - safeDeltaTime, 0.0f);
         break;
      case State::Finished:
         // リスタートを選んだ場合は自動遷移より優先し、二重のシーン要求を防ぐ。
         if (!restartRequested_ && !nextScene_.empty() && !sceneChangeRequested_) {
            finishElapsed_ += safeDeltaTime;

            if (finishElapsed_ >= finishDelay_) {
               GameEngine::EngineContext::ChangeScene(nextScene_);
               sceneChangeRequested_ = true;
            }
         }

         break;
      default:
         break;
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
   runtimeInitialized_ = false;
   state_ = State::Waiting;
}

void RaceManagerComponent::NotifyStart() {
   if (state_ != State::Waiting) return;

   BeginRace();
}

void RaceManagerComponent::NotifyCheckpoint(size_t checkpointIndex) {
   // 次に期待する番号だけを受理し、逆走やゲートの往復で進捗を飛ばさない。
   if (state_ == State::Running && checkpointIndex == nextCheckpointIndex_ && checkpointIndex < checkpointCount_) {
      ++nextCheckpointIndex_;
   }
}

void RaceManagerComponent::NotifyFinish() {
   if (state_ != State::Running || !CanFinish()) return;

   state_ = State::Finished;
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
   if (state_ == State::Waiting) {
      // 共用ゲートへの最初の進入はスタートとして扱う。
      NotifyStart();
      return;
   }

   if (state_ == State::Running && startGateExited_) {
      // スタート後に一度ゲート外へ出るまで、同じ重なりをゴールとして受理しない。
      NotifyFinish();
   }
}

void RaceManagerComponent::NotifyStartGateExit() {
   if (state_ == State::Running) {
      startGateExited_ = true;
   }
}

void RaceManagerComponent::Restart() {
   // レース進行と遷移要求を一括で初期化し、再試行を新規セッションと同じ状態に戻す。
   elapsedTime_ = 0.0f;
   countdownRemaining_ = countdownSeconds_;
   startBannerRemaining_ = 0.0f;
   nextCheckpointIndex_ = 0;
   startGateExited_ = false;
   finishElapsed_ = 0.0f;
   sceneChangeRequested_ = false;
   restartRequested_ = false;
   introSkipArmed_ = false;

   // 編集時の読み込みは状態の準備だけに留め、カメラやプレイヤーへ実行時の変更を加えない。
   if (!runtimeInitialized_) {
      state_ = startMode_ == StartMode::Countdown ? State::Countdown : State::Waiting;
      return;
   }

   presentation_.CancelIntro(GameEngine::EngineContext::GetActiveBrain());
   GameEngine::EngineContext::SetCameraTransitionOpacity(0.0f);
   presentation_.RestoreEffects();

   switch (startMode_) {
      case StartMode::Immediate:
         state_ = State::Running;
         presentation_.SetCameraMotionPaused(
            GameEngine::EngineContext::GetActiveBrain(), false, startCameraBlendSeconds_, startCameraBlendEasing_);
         presentation_.SetPlayerLocked(false);
         break;
      case StartMode::Countdown:
         if (introSettings_.enabled &&
             presentation_.BeginIntro(GameEngine::EngineContext::GetActiveBrain(), introSettings_)) {
            state_ = State::Intro;
         } else {
            BeginCountdown();
         }

         break;
      case StartMode::Gate:
      default:
         state_ = State::Waiting;
         presentation_.SetCameraMotionPaused(
            GameEngine::EngineContext::GetActiveBrain(), false, startCameraBlendSeconds_, startCameraBlendEasing_);
         presentation_.SetPlayerLocked(false);
         break;
   }
}

bool RaceManagerComponent::RequestRestart() {
   if (state_ != State::Finished || restartRequested_ || restartScene_.empty()) {
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

void RaceManagerComponent::BeginCountdown() {
   auto* brain = GameEngine::EngineContext::GetActiveBrain();
   presentation_.CancelIntro(brain);
   GameEngine::EngineContext::SetCameraTransitionOpacity(0.0f);
   presentation_.RestoreEffects();
   state_ = State::Countdown;
   countdownRemaining_ = countdownSeconds_;

   if (useCountdownCameraPose_) {
      presentation_.ApplyCountdownCameraPose(
         brain, countdownCameraPosition_, countdownCameraRotation_, countdownCameraFov_);
   }

   presentation_.SetCameraMotionPaused(brain, true, startCameraBlendSeconds_, startCameraBlendEasing_);
   presentation_.SetPlayerLocked(lockPlayerDuringCountdown_);
}

void RaceManagerComponent::BeginRace() {
   // カウントダウン時間は走行記録へ含めず、開始時点を0秒として計測し直す。
   state_ = State::Running;
   presentation_.SetCameraMotionPaused(
      GameEngine::EngineContext::GetActiveBrain(), false, startCameraBlendSeconds_, startCameraBlendEasing_);
   elapsedTime_ = 0.0f;
   startBannerRemaining_ = startTextDuration_;
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
   ImGui::Text("State: %s", stateNames[static_cast<std::int32_t>(state_)]);
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
