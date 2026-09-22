#include "RearCameraMeasurementRecorder.h"
#include "PlayerRearFollowCamera.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "Framework/EngineContext.h"
#include "RearCameraMath.h"
#include "Utility/Logger.h"
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace App {
using namespace RearCameraMath;

void RearCameraMeasurementRecorder::MutateCameraState(GameEngine::CameraState&, float) {
   auto* camera = GetRearCamera();
   if (!camera) return;
   if (const auto frame = camera->GetFrameView()) {
      // 旧実装と同様、カメラ補間用dtではなくゲーム更新時刻で計測する。
      RecordCameraMeasurementSample(GameEngine::EngineContext::GetDeltaTime(),
         GameEngine::EngineContext::GetGameFrameNumber(), *camera, *frame);
   }
}

/// @brief 検証条件名をファイル名として安全なASCIIへ変換する
inline std::string SanitizeMeasurementName(const std::string& name) {
   std::string sanitized;
   sanitized.reserve(name.size());
   for (unsigned char character : name) {
      if (std::isalnum(character) || character == '-' || character == '_') {
         sanitized.push_back(static_cast<char>(character));
      } else if (sanitized.empty() || sanitized.back() != '_') {
         sanitized.push_back('_');
      }
   }
   return sanitized.empty() ? "camera_measurement" : sanitized;
}

/// @brief 計測ファイル名へ付けるローカル時刻を返す
inline std::string BuildMeasurementTimestamp() {
   const auto now = std::chrono::system_clock::now();
   const std::time_t time = std::chrono::system_clock::to_time_t(now);
   std::tm localTime{};
   localtime_s(&localTime, &time);
   std::ostringstream stream;
   stream << std::put_time(&localTime, "%Y%m%d_%H%M%S");
   return stream.str();
}


void RearCameraMeasurementRecorder::StartCameraMeasurement(const std::string& testName, const RearCameraFrameView& frame) {
   ClearCameraMeasurement();
   state_.cameraMeasurementTestName = testName.empty() ? "camera_live" : testName;
   state_.cameraMeasurementSamples.reserve(3600);
   state_.cameraMeasurementPreviousGravityUp = frame.gravity.GetState().currentGravityUp;
   state_.cameraMeasurementPreviousRight = frame.aim.GetState().cachedRight;
   state_.cameraMeasurementPreviousUp = frame.aim.GetState().cachedUp;
   state_.cameraMeasurementPreviousForward = frame.aim.GetState().cachedForward;
   state_.cameraMeasurementHasPrevious = true;
   state_.cameraMeasurementActive = true;
   Logger::Info(
      "Camera measurement started: " + state_.cameraMeasurementTestName,
      Logger::LogChannel::Game);
}

bool RearCameraMeasurementRecorder::StopCameraMeasurement(bool saveToFile, const RearCameraSettings& settings) {
   state_.cameraMeasurementActive = false;
   if (!saveToFile) {
      return true;
   }
   return SaveCameraMeasurementFiles(settings);
}

void RearCameraMeasurementRecorder::ClearCameraMeasurement() {
   state_.cameraMeasurementActive = false;
   state_.cameraMeasurementHasPrevious = false;
   state_.cameraMeasurementElapsedSeconds = 0.0f;
   state_.cameraMeasurementNextFrame = 0;
   state_.cameraMeasurementLastGameFrame = UINT64_MAX;
   state_.cameraMeasurementSamples.clear();
   state_.cameraMeasurementMaxGravityUpStepDegrees = 0.0f;
   state_.cameraMeasurementMaxRightStepDegrees = 0.0f;
   state_.cameraMeasurementMaxUpStepDegrees = 0.0f;
   state_.cameraMeasurementMaxForwardStepDegrees = 0.0f;
   state_.cameraMeasurementMaxOrthogonalityError = 0.0f;
   state_.cameraMeasurementMaxLengthError = 0.0f;
   state_.cameraMeasurementInvalidCount = 0;
   state_.lastCameraMeasurementPath.clear();
}

void RearCameraMeasurementRecorder::RecordCameraMeasurementSample(
   float deltaTime,
   uint64_t gameFrame,
   const RearCameraSettings& settings,
   const RearCameraFrameView& frame) {
   if (!state_.cameraMeasurementActive) {
      return;
   }

   if (state_.cameraMeasurementLastGameFrame == gameFrame) {
      return;
   }
   state_.cameraMeasurementLastGameFrame = gameFrame;

   CameraMeasurementSample sample{};
   const float safeDeltaTime = std::max(0.0f, deltaTime);
   state_.cameraMeasurementElapsedSeconds += safeDeltaTime;
   sample.frame = state_.cameraMeasurementNextFrame++;
   sample.timeSeconds = state_.cameraMeasurementElapsedSeconds;
   sample.deltaTimeSeconds = safeDeltaTime;
   sample.targetGravityUp = frame.input.gravityUp;
   sample.currentGravityUp = frame.gravity.GetState().currentGravityUp;
   sample.cameraRight = frame.aim.GetState().cachedRight;
   sample.cameraUp = frame.aim.GetState().cachedUp;
   sample.cameraForward = frame.aim.GetState().cachedForward;

   if (state_.cameraMeasurementHasPrevious) {
      sample.gravityUpStepDegrees = AngleDegrees(
         state_.cameraMeasurementPreviousGravityUp,
         frame.gravity.GetState().currentGravityUp);
      sample.cameraRightStepDegrees = AngleDegrees(
         state_.cameraMeasurementPreviousRight,
         frame.aim.GetState().cachedRight);
      sample.cameraUpStepDegrees = AngleDegrees(
         state_.cameraMeasurementPreviousUp,
         frame.aim.GetState().cachedUp);
      sample.cameraForwardStepDegrees = AngleDegrees(
         state_.cameraMeasurementPreviousForward,
         frame.aim.GetState().cachedForward);
   }

   sample.targetGravityUpErrorDegrees = AngleDegrees(frame.gravity.GetState().currentGravityUp, frame.input.gravityUp);
   sample.cameraForwardGravityUpAbsDot = std::abs(frame.aim.GetState().cachedForward.Dot(frame.gravity.GetState().currentGravityUp));
   sample.rightLength = frame.aim.GetState().cachedRight.Length();
   sample.upLength = frame.aim.GetState().cachedUp.Length();
   sample.forwardLength = frame.aim.GetState().cachedForward.Length();
   sample.rightUpAbsDot = std::abs(frame.aim.GetState().cachedRight.Dot(frame.aim.GetState().cachedUp));
   sample.upForwardAbsDot = std::abs(frame.aim.GetState().cachedUp.Dot(frame.aim.GetState().cachedForward));
   sample.forwardRightAbsDot = std::abs(frame.aim.GetState().cachedForward.Dot(frame.aim.GetState().cachedRight));
   sample.lookAtCandidateRightLength = frame.aim.GetState().lastLookAtCandidateRightLength;
   sample.lookAtRecoveryInput = frame.aim.GetState().lastLookAtRecoveryInput;
   sample.lookAtRecoveryBlend = frame.aim.GetState().lastLookAtRecoveryBlend;
   sample.preLandingBlend = frame.transition.GetState().currentPreLandingBlend;
   sample.predictedImpactSeconds = frame.input.predictedLandingSeconds;
   sample.airborne = frame.input.isAirborne;
   sample.landingPredictionValid = frame.input.landingPredictionValid;
   sample.invalid =
      !std::isfinite(safeDeltaTime)
      || !IsFiniteVector(frame.input.gravityUp)
      || !IsFiniteVector(frame.gravity.GetState().currentGravityUp)
      || !IsFiniteVector(frame.aim.GetState().cachedRight)
      || !IsFiniteVector(frame.aim.GetState().cachedUp)
      || !IsFiniteVector(frame.aim.GetState().cachedForward)
      || !std::isfinite(sample.gravityUpStepDegrees)
      || !std::isfinite(sample.cameraRightStepDegrees)
      || !std::isfinite(sample.cameraUpStepDegrees)
      || !std::isfinite(sample.cameraForwardStepDegrees);

   state_.cameraMeasurementMaxGravityUpStepDegrees = std::max(
      state_.cameraMeasurementMaxGravityUpStepDegrees,
      sample.gravityUpStepDegrees);
   state_.cameraMeasurementMaxRightStepDegrees = std::max(
      state_.cameraMeasurementMaxRightStepDegrees,
      sample.cameraRightStepDegrees);
   state_.cameraMeasurementMaxUpStepDegrees = std::max(
      state_.cameraMeasurementMaxUpStepDegrees,
      sample.cameraUpStepDegrees);
   state_.cameraMeasurementMaxForwardStepDegrees = std::max(
      state_.cameraMeasurementMaxForwardStepDegrees,
      sample.cameraForwardStepDegrees);
   state_.cameraMeasurementMaxOrthogonalityError = std::max({
      state_.cameraMeasurementMaxOrthogonalityError,
      sample.rightUpAbsDot,
      sample.upForwardAbsDot,
      sample.forwardRightAbsDot });
   state_.cameraMeasurementMaxLengthError = std::max({
      state_.cameraMeasurementMaxLengthError,
      std::abs(sample.rightLength - 1.0f),
      std::abs(sample.upLength - 1.0f),
      std::abs(sample.forwardLength - 1.0f) });
   if (sample.invalid) {
      ++state_.cameraMeasurementInvalidCount;
   }

   state_.cameraMeasurementSamples.push_back(sample);
   state_.cameraMeasurementPreviousGravityUp = frame.gravity.GetState().currentGravityUp;
   state_.cameraMeasurementPreviousRight = frame.aim.GetState().cachedRight;
   state_.cameraMeasurementPreviousUp = frame.aim.GetState().cachedUp;
   state_.cameraMeasurementPreviousForward = frame.aim.GetState().cachedForward;
   state_.cameraMeasurementHasPrevious = true;

   if (state_.cameraMeasurementSamples.size() >= kMaxCameraMeasurementSamples) {
      state_.cameraMeasurementActive = false;
      SaveCameraMeasurementFiles(settings);
   }
}

bool RearCameraMeasurementRecorder::SaveCameraMeasurementFiles(const RearCameraSettings& settings) {
   if (state_.cameraMeasurementSamples.empty()) {
      Logger::Warning("Camera measurement contains no samples.", Logger::LogChannel::Game);
      return false;
   }

   const std::filesystem::path directory(kCameraMeasurementDirectory);
   std::error_code error;
   std::filesystem::create_directories(directory, error);
   if (error) {
      Logger::Warning(
         "Camera measurement directory could not be created: " + error.message(),
         Logger::LogChannel::Game);
      return false;
   }

   const std::string fileStem =
      BuildMeasurementTimestamp() + "_" + SanitizeMeasurementName(state_.cameraMeasurementTestName);
   const std::filesystem::path csvPath = directory / (fileStem + ".csv");
   const std::filesystem::path summaryPath = directory / (fileStem + ".summary.json");
   std::ofstream csv(csvPath, std::ios::out | std::ios::trunc);
   if (!csv.is_open()) {
      Logger::Warning(
         "Camera measurement CSV could not be opened: " + csvPath.generic_string(),
         Logger::LogChannel::Game);
      return false;
   }

   csv << "frame,time_s,delta_time_s,gravity_up_step_deg,camera_right_step_deg,"
          "camera_up_step_deg,camera_forward_step_deg,target_gravity_up_error_deg,"
          "camera_forward_gravity_up_abs_dot,right_length,up_length,forward_length,"
          "right_up_abs_dot,up_forward_abs_dot,forward_right_abs_dot,"
          "lookat_candidate_right_length,lookat_recovery_input,lookat_recovery_blend,"
          "prelanding_blend,predicted_impact_s,airborne,landing_prediction_valid,invalid,"
          "target_up_x,target_up_y,target_up_z,current_up_x,current_up_y,current_up_z,"
          "camera_right_x,camera_right_y,camera_right_z,camera_up_x,camera_up_y,camera_up_z,"
          "camera_forward_x,camera_forward_y,camera_forward_z\n";
   csv << std::fixed << std::setprecision(7);
   for (const CameraMeasurementSample& sample : state_.cameraMeasurementSamples) {
      csv
         << sample.frame << ','
         << sample.timeSeconds << ','
         << sample.deltaTimeSeconds << ','
         << sample.gravityUpStepDegrees << ','
         << sample.cameraRightStepDegrees << ','
         << sample.cameraUpStepDegrees << ','
         << sample.cameraForwardStepDegrees << ','
         << sample.targetGravityUpErrorDegrees << ','
         << sample.cameraForwardGravityUpAbsDot << ','
         << sample.rightLength << ','
         << sample.upLength << ','
         << sample.forwardLength << ','
         << sample.rightUpAbsDot << ','
         << sample.upForwardAbsDot << ','
         << sample.forwardRightAbsDot << ','
         << sample.lookAtCandidateRightLength << ','
         << sample.lookAtRecoveryInput << ','
         << sample.lookAtRecoveryBlend << ','
         << sample.preLandingBlend << ','
         << sample.predictedImpactSeconds << ','
         << (sample.airborne ? 1 : 0) << ','
         << (sample.landingPredictionValid ? 1 : 0) << ','
         << (sample.invalid ? 1 : 0) << ','
         << sample.targetGravityUp.x << ','
         << sample.targetGravityUp.y << ','
         << sample.targetGravityUp.z << ','
         << sample.currentGravityUp.x << ','
         << sample.currentGravityUp.y << ','
         << sample.currentGravityUp.z << ','
         << sample.cameraRight.x << ','
         << sample.cameraRight.y << ','
         << sample.cameraRight.z << ','
         << sample.cameraUp.x << ','
         << sample.cameraUp.y << ','
         << sample.cameraUp.z << ','
         << sample.cameraForward.x << ','
         << sample.cameraForward.y << ','
         << sample.cameraForward.z << '\n';
   }
   csv.close();
   if (!csv) {
      Logger::Warning(
         "Camera measurement CSV write failed: " + csvPath.generic_string(),
         Logger::LogChannel::Game);
      return false;
   }

   const float theoreticalGravityUpDegreesPerFrameAt60Fps =
      std::max(0.0f, settings.gravityUpLerpSpeed) * kRadiansToDegrees / 60.0f;
   const float averageFps = state_.cameraMeasurementElapsedSeconds > 1e-6f
      ? static_cast<float>(state_.cameraMeasurementSamples.size()) / state_.cameraMeasurementElapsedSeconds
      : 0.0f;
   nlohmann::json summary{
      { "formatVersion", 1 },
      { "testName", state_.cameraMeasurementTestName },
      { "csvFile", csvPath.filename().generic_string() },
      { "definitions", {
         { "settingValue", "settings.gravityUpLerpSpeed is a maximum angular speed in rad/s" },
         { "calculatedValue", "settings.gravityUpLerpSpeed * 180 / pi / 60 is the 60 FPS upper bound in deg/frame" },
         { "measuredValue", "angle between consecutive finalized camera basis vectors" },
         { "invalidFrame", "any non-finite finalized axis or angular sample" },
      } },
      { "settings", {
         { "gravityUpLerpSpeedRadPerSecond", settings.gravityUpLerpSpeed },
         { "theoreticalGravityUpDegreesPerFrameAt60Fps", theoreticalGravityUpDegreesPerFrameAt60Fps },
         { "settings.rotationLerpSpeed", settings.rotationLerpSpeed },
         { "settings.lookAtRecoveryRange", settings.lookAtRecoveryRange },
         { "settings.lookAtRecoveryCurveControl1", settings.lookAtRecoveryCurveControl1 },
         { "settings.lookAtRecoveryCurveControl2", settings.lookAtRecoveryCurveControl2 },
         { "settings.takeoffFramingBlendSeconds", settings.takeoffFramingBlendSeconds },
         { "settings.landingFramingBlendSeconds", settings.landingFramingBlendSeconds },
         { "settings.preLandingPredictionSeconds", settings.preLandingPredictionSeconds },
         { "settings.preLandingFullBlendSeconds", settings.preLandingFullBlendSeconds },
         { "settings.landingRearLerpRampSeconds", settings.landingRearLerpRampSeconds },
      } },
      { "summary", {
         { "frames", state_.cameraMeasurementSamples.size() },
         { "durationSeconds", state_.cameraMeasurementElapsedSeconds },
         { "averageFps", averageFps },
         { "maxGravityUpStepDegrees", state_.cameraMeasurementMaxGravityUpStepDegrees },
         { "maxCameraRightStepDegrees", state_.cameraMeasurementMaxRightStepDegrees },
         { "maxCameraUpStepDegrees", state_.cameraMeasurementMaxUpStepDegrees },
         { "maxCameraForwardStepDegrees", state_.cameraMeasurementMaxForwardStepDegrees },
         { "maxBasisAbsDot", state_.cameraMeasurementMaxOrthogonalityError },
         { "maxAxisLengthError", state_.cameraMeasurementMaxLengthError },
         { "invalidFrameCount", state_.cameraMeasurementInvalidCount },
      } },
   };

   std::ofstream summaryFile(summaryPath, std::ios::out | std::ios::trunc);
   if (!summaryFile.is_open()) {
      Logger::Warning(
         "Camera measurement summary could not be opened: " + summaryPath.generic_string(),
         Logger::LogChannel::Game);
      return false;
   }
   summaryFile << summary.dump(3);
   summaryFile.close();
   if (!summaryFile) {
      Logger::Warning(
         "Camera measurement summary write failed: " + summaryPath.generic_string(),
         Logger::LogChannel::Game);
      return false;
   }

   state_.lastCameraMeasurementPath = csvPath.generic_string();
   Logger::Info(
      "Camera measurement saved: " + state_.lastCameraMeasurementPath,
      Logger::LogChannel::Game);
   return true;
}

void RearCameraMeasurementRecorder::RunFixedGravityUpVerification(const RearCameraSettings& settings) {
   ClearCameraMeasurement();
   state_.cameraMeasurementTestName = "fixed_180_gravity_up_60fps";

   constexpr float fixedDeltaTime = 1.0f / 60.0f;
   const GameEngine::Vector3 targetUp = { 0.0f, -1.0f, 0.0f };
   const GameEngine::Vector3 cameraRight = { 1.0f, 0.0f, 0.0f };
   GameEngine::Vector3 currentUp = { 0.0f, 1.0f, 0.0f };
   GameEngine::Vector3 previousCameraUp = currentUp;
   GameEngine::Vector3 previousCameraForward = cameraRight.Cross(currentUp);
   const float maxRadiansDelta = std::max(0.0f, settings.gravityUpLerpSpeed) * fixedDeltaTime;

   for (uint64_t frame = 0; frame < 240; ++frame) {
      const GameEngine::Vector3 previousGravityUp = currentUp;
      currentUp = RotateTowardsUnit(
         currentUp,
         targetUp,
         maxRadiansDelta,
         cameraRight);
      const GameEngine::Vector3 cameraForward = NormalizeOrFallback(
         cameraRight.Cross(currentUp),
         previousCameraForward);

      CameraMeasurementSample sample{};
      sample.frame = frame;
      sample.timeSeconds = static_cast<float>(frame + 1) * fixedDeltaTime;
      sample.deltaTimeSeconds = fixedDeltaTime;
      sample.gravityUpStepDegrees = AngleDegrees(previousGravityUp, currentUp);
      sample.cameraRightStepDegrees = 0.0f;
      sample.cameraUpStepDegrees = AngleDegrees(previousCameraUp, currentUp);
      sample.cameraForwardStepDegrees = AngleDegrees(previousCameraForward, cameraForward);
      sample.targetGravityUpErrorDegrees = AngleDegrees(currentUp, targetUp);
      sample.cameraForwardGravityUpAbsDot = std::abs(cameraForward.Dot(currentUp));
      sample.rightLength = cameraRight.Length();
      sample.upLength = currentUp.Length();
      sample.forwardLength = cameraForward.Length();
      sample.rightUpAbsDot = std::abs(cameraRight.Dot(currentUp));
      sample.upForwardAbsDot = std::abs(currentUp.Dot(cameraForward));
      sample.forwardRightAbsDot = std::abs(cameraForward.Dot(cameraRight));
      sample.lookAtCandidateRightLength = 1.0f;
      sample.lookAtRecoveryInput = 1.0f;
      sample.lookAtRecoveryBlend = 1.0f;
      sample.targetGravityUp = targetUp;
      sample.currentGravityUp = currentUp;
      sample.cameraRight = cameraRight;
      sample.cameraUp = currentUp;
      sample.cameraForward = cameraForward;
      sample.invalid =
         !IsFiniteVector(currentUp)
         || !IsFiniteVector(cameraForward)
         || !std::isfinite(sample.gravityUpStepDegrees);

      state_.cameraMeasurementSamples.push_back(sample);
      state_.cameraMeasurementMaxGravityUpStepDegrees = std::max(
         state_.cameraMeasurementMaxGravityUpStepDegrees,
         sample.gravityUpStepDegrees);
      state_.cameraMeasurementMaxUpStepDegrees = std::max(
         state_.cameraMeasurementMaxUpStepDegrees,
         sample.cameraUpStepDegrees);
      state_.cameraMeasurementMaxForwardStepDegrees = std::max(
         state_.cameraMeasurementMaxForwardStepDegrees,
         sample.cameraForwardStepDegrees);
      state_.cameraMeasurementMaxOrthogonalityError = std::max({
         state_.cameraMeasurementMaxOrthogonalityError,
         sample.rightUpAbsDot,
         sample.upForwardAbsDot,
         sample.forwardRightAbsDot });
      state_.cameraMeasurementMaxLengthError = std::max({
         state_.cameraMeasurementMaxLengthError,
         std::abs(sample.rightLength - 1.0f),
         std::abs(sample.upLength - 1.0f),
         std::abs(sample.forwardLength - 1.0f) });
      if (sample.invalid) {
         ++state_.cameraMeasurementInvalidCount;
      }

      previousCameraUp = currentUp;
      previousCameraForward = cameraForward;
      if (sample.targetGravityUpErrorDegrees <= 1e-4f) {
         break;
      }
   }

   state_.cameraMeasurementElapsedSeconds =
      static_cast<float>(state_.cameraMeasurementSamples.size()) * fixedDeltaTime;
   state_.cameraMeasurementNextFrame = state_.cameraMeasurementSamples.size();
   SaveCameraMeasurementFiles(settings);
}

} // namespace App
