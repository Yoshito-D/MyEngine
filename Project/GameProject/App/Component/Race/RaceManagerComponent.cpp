#include "RaceManagerComponent.h"

#include "../Camera/CameraModeSwitcher.h"
#include "../Gravity/GravityBody.h"
#include "../Vehicle/VehicleController.h"
#include "../Vehicle/VehicleSpeedPostEffectController.h"
#include "Logger.h"
#include "Object/Object.h"
#include "Scene/BaseScene.h"
#include "Scene/SceneWorld.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace {
constexpr size_t kBestTimeCount = 3;
constexpr int kRecordFormatVersion = 1;
}

namespace App {

void RaceManagerComponent::Update(float deltaTime) {
   if (!runtimeInitialized_) {
      // Edit中のシーン読込ではPlayerを変更せず、最初のランタイム更新でのみレース用ロックを開始する。
      runtimeInitialized_ = true;
      Restart();
   }

   const float safeDeltaTime = (std::max)(deltaTime, 0.0f);
   switch (state_) {
      case State::Countdown:
         countdownRemaining_ -= safeDeltaTime;
         if (countdownRemaining_ <= 0.0f) {
            BeginRace();
         }
         break;
      case State::Running:
         elapsedTime_ += static_cast<double>(safeDeltaTime);
         startBannerRemaining_ = (std::max)(startBannerRemaining_ - safeDeltaTime, 0.0f);
         break;
      case State::Finished:
         if (!restartRequested_ && !nextScene_.empty() && !sceneChangeRequested_) {
            finishElapsed_ += safeDeltaTime;
            if (finishElapsed_ >= finishDelay_) {
               GameEngine::BaseScene::SetNextSceneName(nextScene_);
               sceneChangeRequested_ = true;
            }
         }
         break;
      default:
         break;
   }
}

void RaceManagerComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   vehicleController_ = nullptr;
   gravityBody_ = nullptr;
   cameraSwitcher_ = nullptr;
   speedPostEffectController_ = nullptr;
   runtimeInitialized_ = false;
   playerLocked_ = false;

   if (auto* playerObject = sceneWorld.FindObjectById(playerObjectId_)) {
      vehicleController_ = playerObject->GetComponent<VehicleController>();
      gravityBody_ = playerObject->GetComponent<GravityBody>();
      cameraSwitcher_ = playerObject->GetComponent<CameraModeSwitcher>();
      speedPostEffectController_ = playerObject->GetComponent<VehicleSpeedPostEffectController>();
   } else if (!playerObjectId_.empty()) {
      Logger::Warning("Race player object was not found: " + playerObjectId_, Logger::LogChannel::Game);
   }

   // RaceManagerがロックを所有するPlayerの必須コンポーネントは、解除時に必ず有効へ戻す。
   // 以前のランタイムロックがシーンへ保存されていても、操作不能を次のセッションへ持ち越さない。
   vehicleControllerEnabledWhenUnlocked_ = vehicleController_ != nullptr;
   gravityBodyEnabledWhenUnlocked_ = gravityBody_ != nullptr;
   cameraSwitcherEnabledWhenUnlocked_ = cameraSwitcher_ && cameraSwitcher_->IsEnabled();
   speedPostEffectEnabledWhenRacing_ = speedPostEffectController_ && speedPostEffectController_->IsEnabled();

   if (vehicleController_ && !vehicleController_->IsEnabled()) {
      Logger::Warning(
         "Race player VehicleController was disabled in scene data; it will be restored when unlocked.",
         Logger::LogChannel::Game);
   }
   if (gravityBody_ && !gravityBody_->IsEnabled()) {
      Logger::Warning(
         "Race player GravityBody was disabled in scene data; it will be restored when unlocked.",
         Logger::LogChannel::Game);
   }

   LoadBestTimes();
   Restart();
}

void RaceManagerComponent::NotifyStart() {
   if (state_ != State::Waiting) {
      return;
   }
   BeginRace();
}

void RaceManagerComponent::NotifyCheckpoint(size_t checkpointIndex) {
   if (state_ == State::Running && checkpointIndex == nextCheckpointIndex_ && checkpointIndex < checkpointCount_) {
      ++nextCheckpointIndex_;
   }
}

void RaceManagerComponent::NotifyFinish() {
   if (state_ != State::Running || !CanFinish()) {
      return;
   }

   state_ = State::Finished;
   startBannerRemaining_ = 0.0f;
   finishElapsed_ = 0.0f;
   AddBestTime(elapsedTime_);
   SaveBestTimes();

   if (lockPlayerOnFinish_) {
      SetPlayerLocked(true);
   }

   // 結果画面で停止後の速度値から加速演出が再有効化されないよう、制御元ごと停止する。
   if (speedPostEffectController_) {
      speedPostEffectController_->SetEnabled(false);
   }

   if (cameraSwitcher_ && !finishCameraId_.empty()) {
      if (cameraSwitcher_->SwitchToCamera(finishCameraId_)) {
         // 結果画面中に手動入力でゴールカメラから離れないよう固定する。
         cameraSwitcher_->SetEnabled(false);
      } else {
         Logger::Warning("Race finish camera was not found: " + finishCameraId_, Logger::LogChannel::Game);
      }
   }
}

void RaceManagerComponent::NotifyStartFinish() {
   if (state_ == State::Waiting) {
      NotifyStart();
      return;
   }
   if (state_ == State::Running && startGateExited_) {
      NotifyFinish();
   }
}

void RaceManagerComponent::NotifyStartGateExit() {
   if (state_ == State::Running) {
      startGateExited_ = true;
   }
}

void RaceManagerComponent::Restart() {
   elapsedTime_ = 0.0;
   countdownRemaining_ = countdownSeconds_;
   startBannerRemaining_ = 0.0f;
   nextCheckpointIndex_ = 0;
   startGateExited_ = false;
   finishElapsed_ = 0.0f;
   sceneChangeRequested_ = false;
   restartRequested_ = false;

   if (cameraSwitcher_) {
      cameraSwitcher_->SetEnabled(cameraSwitcherEnabledWhenUnlocked_);
   }
   if (speedPostEffectController_) {
      speedPostEffectController_->SetEnabled(speedPostEffectEnabledWhenRacing_);
   }

   switch (startMode_) {
      case StartMode::Immediate:
         state_ = State::Running;
         SetPlayerLocked(false);
         break;
      case StartMode::Countdown:
         state_ = State::Countdown;
         SetPlayerLocked(runtimeInitialized_ && lockPlayerDuringCountdown_);
         break;
      case StartMode::Gate:
      default:
         state_ = State::Waiting;
         SetPlayerLocked(false);
         break;
   }
}

void RaceManagerComponent::RequestRestart() {
   if (state_ != State::Finished || restartRequested_ || restartScene_.empty()) {
      return;
   }
   restartRequested_ = true;
   GameEngine::BaseScene::SetNextSceneName(restartScene_);
}

nlohmann::json RaceManagerComponent::Serialize() const {
   const char* startModeName = "Countdown";
   if (startMode_ == StartMode::Immediate) {
      startModeName = "Immediate";
   } else if (startMode_ == StartMode::Gate) {
      startModeName = "Gate";
   }
   return nlohmann::json{
      { "startMode", startModeName },
      { "countdownSeconds", countdownSeconds_ },
      { "startTextDuration", startTextDuration_ },
      { "checkpointCount", checkpointCount_ },
      { "playerObjectId", playerObjectId_ },
      { "finishCameraId", finishCameraId_ },
      { "restartScene", restartScene_ },
      { "recordKey", recordKey_ },
      { "recordFile", recordFile_ },
      { "lockPlayerDuringCountdown", lockPlayerDuringCountdown_ },
      { "lockPlayerOnFinish", lockPlayerOnFinish_ },
      { "nextScene", nextScene_ },
      { "finishDelay", finishDelay_ }
   };
}

void RaceManagerComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
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
      countdownSeconds_ = (std::max)(data.at("countdownSeconds").get<float>(), 0.0f);
   }
   if (data.contains("startTextDuration") && data.at("startTextDuration").is_number()) {
      startTextDuration_ = (std::max)(data.at("startTextDuration").get<float>(), 0.0f);
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
      finishDelay_ = (std::max)(data.at("finishDelay").get<float>(), 0.0f);
   }
}

void RaceManagerComponent::BeginRace() {
   state_ = State::Running;
   elapsedTime_ = 0.0;
   startBannerRemaining_ = startTextDuration_;
   nextCheckpointIndex_ = 0;
   startGateExited_ = false;
   SetPlayerLocked(false);
}

bool RaceManagerComponent::CanFinish() const {
   return nextCheckpointIndex_ >= checkpointCount_;
}

void RaceManagerComponent::SetPlayerLocked(bool locked) {
   playerLocked_ = locked;
   if (locked) {
      if (gravityBody_) {
         gravityBody_->SetVelocity({ 0.0f, 0.0f, 0.0f });
         gravityBody_->SetEnabled(false);
      }
      if (vehicleController_) {
         vehicleController_->SetEnabled(false);
      }
      return;
   }

   if (gravityBody_) {
      gravityBody_->SetVelocity({ 0.0f, 0.0f, 0.0f });
      gravityBody_->SetEnabled(gravityBodyEnabledWhenUnlocked_);
   }
   if (vehicleController_) {
      vehicleController_->SetEnabled(vehicleControllerEnabledWhenUnlocked_);
   }
}

void RaceManagerComponent::AddBestTime(double timeSeconds) {
   if (!std::isfinite(timeSeconds) || timeSeconds <= 0.0) {
      return;
   }
   bestTimes_.push_back(timeSeconds);
   std::sort(bestTimes_.begin(), bestTimes_.end());
   if (bestTimes_.size() > kBestTimeCount) {
      bestTimes_.resize(kBestTimeCount);
   }
}

bool RaceManagerComponent::LoadBestTimes() {
   bestTimes_.clear();
   if (recordFile_.empty() || recordKey_.empty()) {
      return true;
   }

   const std::filesystem::path recordPath(recordFile_);
   if (!std::filesystem::exists(recordPath)) {
      return true;
   }

   try {
      std::ifstream input(recordPath);
      if (!input.is_open()) {
         LogRecordWarning("could not open record file");
         return false;
      }

      nlohmann::json root;
      input >> root;
      if (!root.is_object() || !root.contains("races") || !root.at("races").is_object()) {
         LogRecordWarning("record file schema is invalid");
         return false;
      }

      const auto& races = root.at("races");
      if (!races.contains(recordKey_) || !races.at(recordKey_).is_object()) {
         return true;
      }
      const auto& raceData = races.at(recordKey_);
      if (!raceData.contains("bestTimes") || !raceData.at("bestTimes").is_array()) {
         return true;
      }

      for (const auto& value : raceData.at("bestTimes")) {
         if (value.is_number()) {
            AddBestTime(value.get<double>());
         }
      }
      return true;
   } catch (const std::exception& exception) {
      LogRecordWarning(std::string("failed to load records: ") + exception.what());
      bestTimes_.clear();
      return false;
   }
}

bool RaceManagerComponent::SaveBestTimes() const {
   if (recordFile_.empty() || recordKey_.empty()) {
      LogRecordWarning("recordFile or recordKey is empty");
      return false;
   }

   const std::filesystem::path recordPath(recordFile_);
   nlohmann::json root = nlohmann::json::object();
   if (std::filesystem::exists(recordPath)) {
      try {
         std::ifstream input(recordPath);
         if (input.is_open()) {
            input >> root;
         }
      } catch (const std::exception& exception) {
         LogRecordWarning(std::string("existing record file will be replaced: ") + exception.what());
         root = nlohmann::json::object();
      }
   }
   if (!root.is_object()) {
      root = nlohmann::json::object();
   }
   root["version"] = kRecordFormatVersion;
   if (!root.contains("races") || !root.at("races").is_object()) {
      root["races"] = nlohmann::json::object();
   }
   root["races"][recordKey_]["bestTimes"] = bestTimes_;

   std::error_code error;
   if (!recordPath.parent_path().empty()) {
      std::filesystem::create_directories(recordPath.parent_path(), error);
      if (error) {
         LogRecordWarning("could not create record directory: " + error.message());
         return false;
      }
   }

   std::filesystem::path temporaryPath = recordPath;
   temporaryPath += ".tmp";
   try {
      std::ofstream output(temporaryPath, std::ios::out | std::ios::trunc);
      if (!output.is_open()) {
         LogRecordWarning("could not open temporary record file");
         return false;
      }
      output << root.dump(3) << '\n';
      output.close();
      if (!output) {
         LogRecordWarning("could not finish writing temporary record file");
         return false;
      }
   } catch (const std::exception& exception) {
      LogRecordWarning(std::string("failed to write records: ") + exception.what());
      return false;
   }

#ifdef _WIN32
   if (!MoveFileExW(
      temporaryPath.c_str(),
      recordPath.c_str(),
      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      LogRecordWarning("could not replace record file (Win32 error " + std::to_string(GetLastError()) + ")");
      std::filesystem::remove(temporaryPath, error);
      return false;
   }
#else
   std::filesystem::rename(temporaryPath, recordPath, error);
   if (error) {
      LogRecordWarning("could not replace record file: " + error.message());
      std::filesystem::remove(temporaryPath, error);
      return false;
   }
#endif
   return true;
}

void RaceManagerComponent::LogRecordWarning(const std::string& message) const {
   Logger::Warning("Race record: " + message + " [" + recordFile_ + "]", Logger::LogChannel::Game);
}

#ifdef USE_IMGUI
void RaceManagerComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   static const char* stateNames[] = { "Waiting", "Countdown", "Running", "Finished" };
   ImGui::Text("State: %s", stateNames[static_cast<int>(state_)]);
   ImGui::Text("Time: %.3f", elapsedTime_);
   ImGui::Text("Checkpoint: %zu / %zu", nextCheckpointIndex_, checkpointCount_);
   for (size_t index = 0; index < bestTimes_.size(); ++index) {
      ImGui::Text("Best %zu: %.3f", index + 1, bestTimes_[index]);
   }
   if (ImGui::Button("Restart State")) {
      Restart();
   }
}
#endif

} // namespace App
