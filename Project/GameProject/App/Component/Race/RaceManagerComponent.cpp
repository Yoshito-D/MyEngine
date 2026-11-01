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
   }

   const float safeDeltaTime = std::max(deltaTime, 0.0f);

   // 状態ごとに進める時計を分離し、カウントダウンや結果待機を走行時間へ混ぜない。
   switch (state_) {
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

   presentation_.RestoreEffects();

   switch (startMode_) {
      case StartMode::Immediate:
         state_ = State::Running;
         presentation_.SetCameraMotionPaused(
            GameEngine::EngineContext::GetActiveBrain(), false, startCameraBlendSeconds_, startCameraBlendEasing_);
         presentation_.SetPlayerLocked(false);
         break;
      case StartMode::Countdown:
         state_ = State::Countdown;

         // Edit中の読込では追従カメラを書き換えず、実行開始時に保存した姿勢を適用する。
         if (runtimeInitialized_ && useCountdownCameraPose_) {
            presentation_.ApplyCountdownCameraPose(GameEngine::EngineContext::GetActiveBrain(),
               countdownCameraPosition_, countdownCameraRotation_, countdownCameraFov_);
         }

         presentation_.SetCameraMotionPaused(
            GameEngine::EngineContext::GetActiveBrain(), true, startCameraBlendSeconds_, startCameraBlendEasing_);
         presentation_.SetPlayerLocked(runtimeInitialized_ && lockPlayerDuringCountdown_);
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
      {"useCountdownCameraPose", useCountdownCameraPose_},
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
      {"playerObjectId", "finishCameraId", "restartScene", "nextScene", "recordKey", "recordFile"});
}

void RaceManagerComponent::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void RaceManagerComponent::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void RaceManagerComponent::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   if (!data.is_object()) return;

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
   static const char* stateNames[] = {"Waiting", "Countdown", "Running", "Finished"};
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
#endif

} // namespace App
