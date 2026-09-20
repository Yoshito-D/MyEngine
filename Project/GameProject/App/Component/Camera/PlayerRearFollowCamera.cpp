#include "PlayerRearFollowCamera.h"
#include "RearCameraDebugView.h"
#include "RearCameraMath.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace App {
using namespace RearCameraMath;

const bool kRegistered = GameEngine::VirtualCamera::RegisterComponentFactory(
   "PlayerRearFollowCamera",
   [](GameEngine::VirtualCamera& camera) -> GameEngine::ICinemachineComponent* {
	  if (auto* existing = camera.GetComponent<PlayerRearFollowCamera>()) {
		 return existing;
	  }
	  return camera.AddComponent<PlayerRearFollowCamera>();
   });


void PlayerRearFollowCamera::ResetRuntimeState() {
   input_.gravityUp = { 0.0f, 1.0f, 0.0f };
   input_.pivotTarget = { 0.0f, 0.0f, 0.0f };
   input_.planetCenter = { 0.0f, 0.0f, 0.0f };
   input_.followForward = { 0.0f, 0.0f, 1.0f };
   input_.airborneMoveForward = { 0.0f, 0.0f, 1.0f };
   input_.isAirborne = false;
   input_.landingPredictionValid = false;
   input_.predictedLandingUp = { 0.0f, 1.0f, 0.0f };
   input_.predictedLandingBackward = { 0.0f, 0.0f, -1.0f };
   input_.predictedLandingContact = { 0.0f, 0.0f, 0.0f };
   input_.predictedLandingSeconds = 0.0f;
   input_.playerSpeed = 0.0f;
   input_.playerVelocity = { 0.0f, 0.0f, 0.0f };
   if (auto* owner = GetOwnerCamera()) {
      for (const auto& component : owner->GetComponents()) {
         if (auto* rearComponent = dynamic_cast<RearCameraComponent*>(component.get())) {
            rearComponent->Reset(*this);
         }
      }
      if (const auto* speed = owner->GetComponent<RearCameraSpeedEffects>()) {
         GameEngine::CameraState state = owner->GetState();
         state.fov = speed->GetState().currentFov;
         owner->SetState(state);
      }
   }
}

void PlayerRearFollowCamera::SetLandingPrediction(
   const GameEngine::Vector3& up,
   const GameEngine::Vector3& backward,
   const GameEngine::Vector3& contactPoint,
   float secondsToImpact) {
   input_.predictedLandingUp = NormalizeOrFallback(up, input_.predictedLandingUp);
   input_.predictedLandingBackward = NormalizeOrFallback(backward, input_.predictedLandingBackward);
   input_.predictedLandingContact = contactPoint;
   input_.predictedLandingSeconds = std::max(0.0f, secondsToImpact);
   input_.landingPredictionValid = true;
}

void PlayerRearFollowCamera::Initialize(GameEngine::VirtualCamera* owner) {
   ICinemachineComponent::Initialize(owner);
   if (!owner) return;
   // シーン内の従来の単一エントリを、その場で実際の独立コンポーネントへ展開する。
   if (!owner->GetComponent<RearCameraTransition>()) owner->AddComponent<RearCameraTransition>();
   if (!owner->GetComponent<RearCameraGravityUp>()) owner->AddComponent<RearCameraGravityUp>();
   if (!owner->GetComponent<RearCameraPlanetGuide>()) owner->AddComponent<RearCameraPlanetGuide>();
   if (!owner->GetComponent<RearCameraDirectionTracker>()) owner->AddComponent<RearCameraDirectionTracker>();
   if (!owner->GetComponent<RearCameraSpeedEffects>()) owner->AddComponent<RearCameraSpeedEffects>();
   if (!owner->GetComponent<RearCameraPositionSolver>()) owner->AddComponent<RearCameraPositionSolver>();
   if (!owner->GetComponent<RearCameraAimSolver>()) owner->AddComponent<RearCameraAimSolver>();
   if (!owner->GetComponent<RearCameraMeasurementRecorder>()) owner->AddComponent<RearCameraMeasurementRecorder>();
   if (!owner->GetComponent<RearCameraDebugView>()) owner->AddComponent<RearCameraDebugView>();
   ResetRuntimeState();
}

bool PlayerRearFollowCamera::HasRequiredComponents() const {
   const auto* owner = GetOwnerCamera();
   return owner
      && owner->GetComponent<RearCameraTransition>()
      && owner->GetComponent<RearCameraGravityUp>()
      && owner->GetComponent<RearCameraPlanetGuide>()
      && owner->GetComponent<RearCameraDirectionTracker>()
      && owner->GetComponent<RearCameraSpeedEffects>()
      && owner->GetComponent<RearCameraPositionSolver>()
      && owner->GetComponent<RearCameraAimSolver>();
}

std::optional<RearCameraFrameView> PlayerRearFollowCamera::GetFrameView() const {
   if (!HasRequiredComponents()) return std::nullopt;
   const auto* owner = GetOwnerCamera();
   return RearCameraFrameView{ input_, *owner->GetComponent<RearCameraTransition>(),
      *owner->GetComponent<RearCameraDirectionTracker>(), *owner->GetComponent<RearCameraPlanetGuide>(),
      *owner->GetComponent<RearCameraSpeedEffects>(), *owner->GetComponent<RearCameraGravityUp>(),
      *owner->GetComponent<RearCameraAimSolver>() };
}

void PlayerRearFollowCamera::MutateCameraState(GameEngine::CameraState&, float) {
   // 計算部品はVirtualCamera::CalculateStateが順番に呼ぶ。ここから重複実行しない。
}

GameEngine::Vector3 PlayerRearFollowCamera::GetCameraUp() const {
   const auto* aim = owner_ ? owner_->GetComponent<RearCameraAimSolver>() : nullptr;
   return aim ? aim->GetState().cachedUp : GameEngine::Vector3{ 0.0f, 1.0f, 0.0f };
}

GameEngine::Vector3 PlayerRearFollowCamera::GetCameraRight() const {
   const auto* aim = owner_ ? owner_->GetComponent<RearCameraAimSolver>() : nullptr;
   return aim ? aim->GetState().cachedRight : GameEngine::Vector3{ 1.0f, 0.0f, 0.0f };
}

GameEngine::Vector3 PlayerRearFollowCamera::GetCameraForward() const {
   const auto* aim = owner_ ? owner_->GetComponent<RearCameraAimSolver>() : nullptr;
   return aim ? aim->GetState().cachedForward : GameEngine::Vector3{ 0.0f, 0.0f, 1.0f };
}

void PlayerRearFollowCamera::StartCameraMeasurement(const std::string& testName) {
   auto* recorder = owner_ ? owner_->GetComponent<RearCameraMeasurementRecorder>() : nullptr;
   if (recorder) {
      if (const auto frame = GetFrameView()) recorder->StartCameraMeasurement(testName, *frame);
   }
}

bool PlayerRearFollowCamera::StopCameraMeasurement(bool saveToFile) {
   auto* recorder = owner_ ? owner_->GetComponent<RearCameraMeasurementRecorder>() : nullptr;
   return recorder ? recorder->StopCameraMeasurement(saveToFile, *this) : !saveToFile;
}

void PlayerRearFollowCamera::ClearCameraMeasurement() {
   if (auto* recorder = owner_ ? owner_->GetComponent<RearCameraMeasurementRecorder>() : nullptr) {
      recorder->ClearCameraMeasurement();
   }
}

bool PlayerRearFollowCamera::IsCameraMeasurementActive() const {
   const auto* recorder = owner_ ? owner_->GetComponent<RearCameraMeasurementRecorder>() : nullptr;
   return recorder && recorder->GetState().cameraMeasurementActive;
}

size_t PlayerRearFollowCamera::GetCameraMeasurementSampleCount() const {
   const auto* recorder = owner_ ? owner_->GetComponent<RearCameraMeasurementRecorder>() : nullptr;
   return recorder ? recorder->GetState().cameraMeasurementSamples.size() : 0;
}

const std::string& PlayerRearFollowCamera::GetLastCameraMeasurementPath() const {
   const auto* recorder = owner_ ? owner_->GetComponent<RearCameraMeasurementRecorder>() : nullptr;
   static const std::string empty;
   return recorder ? recorder->GetState().lastCameraMeasurementPath : empty;
}

#ifdef USE_IMGUI
void PlayerRearFollowCamera::DrawInspector() {
   ImGui::Checkbox("Enabled", &isEnabled_);
   ImGui::TextDisabled("Settings: RearCameraDebugView");
   if (!HasRequiredComponents()) {
      ImGui::TextDisabled("A required rear camera component is missing.");
   }
}
#endif

nlohmann::json PlayerRearFollowCamera::Serialize() const {
   return nlohmann::json{
	   { "distance", distance },
	   { "height", height },
	   { "groundedTargetHeight", groundedTargetHeight },
	   { "takeoffFramingBlendSeconds", takeoffFramingBlendSeconds },
	   { "landingFramingBlendSeconds", landingFramingBlendSeconds },
	   { "airborneDistanceOffset", airborneDistanceOffset },
	   { "airborneFovOffset", airborneFovOffset },
	   { "airbornePlanetDirectionBlend", airbornePlanetDirectionBlend },
	   { "airbornePlanetDirectionLerpSpeed", airbornePlanetDirectionLerpSpeed },
	   { "jumpPlanetDirectionDelaySeconds", jumpPlanetDirectionDelaySeconds },
	   { "jumpPlanetDirectionRestoreSeconds", jumpPlanetDirectionRestoreSeconds },
	   { "enableAirbornePlanetDirectionGuide", enableAirbornePlanetDirectionGuide },
	   { "enableAirborneGravityDirectionBoost", enableAirborneGravityDirectionBoost },
	   { "airborneGravityDirectionBoostThreshold", airborneGravityDirectionBoostThreshold },
	   { "airborneGravityDirectionBoostFullThreshold", airborneGravityDirectionBoostFullThreshold },
	   { "airborneGravityDirectionBoostBias", airborneGravityDirectionBoostBias },
	   { "airborneBlendLerpSpeed", airborneBlendLerpSpeed },
	   { "enablePreLandingCamera", enablePreLandingCamera },
	   { "preLandingPredictionSeconds", preLandingPredictionSeconds },
	   { "preLandingFullBlendSeconds", preLandingFullBlendSeconds },
	   { "preLandingBlendLerpSpeed", preLandingBlendLerpSpeed },
	   { "preLandingReleaseLerpSpeed", preLandingReleaseLerpSpeed },
	   { "preLandingTerrainLookAhead", preLandingTerrainLookAhead },
	   { "preLandingTerrainLookBlend", preLandingTerrainLookBlend },
	   { "preLandingMinOutwardHeight", preLandingMinOutwardHeight },
	   { "airborneForwardLerpSpeed", airborneForwardLerpSpeed },
	   { "rearLerpSpeed", rearLerpSpeed },
	   { "landingRearLerpRampSeconds", landingRearLerpRampSeconds },
	   { "gravityUpLerpSpeed", gravityUpLerpSpeed },
	   { "fovDefault", fovDefault },
	   { "fovBoostMax", fovBoostMax },
	   { "fovLerpSpeed", fovLerpSpeed },
	   { "distanceBoostMax", distanceBoostMax },
	   { "springStiffness", springStiffness },
	   { "springDamping", springDamping },
	   { "speedChangeFovKickMax", speedChangeFovKickMax },
	   { "speedChangeDistanceKickMax", speedChangeDistanceKickMax },
      { "speedBoostThreshold", speedBoostThreshold },
      { "speedBoostMax", speedBoostMax },
      { "positionLerpSpeed", positionLerpSpeed },
      { "eyeDirectionMaxAngularSpeed", eyeDirectionMaxAngularSpeed },
      { "rotationLerpSpeed", rotationLerpSpeed },
      { "lookAtRecoveryRange", lookAtRecoveryRange },
      { "lookAtRecoveryCurveControl1", lookAtRecoveryCurveControl1 },
      { "lookAtRecoveryCurveControl2", lookAtRecoveryCurveControl2 },
      { "showCameraMeasurementOverlay", showCameraMeasurementOverlay },
      { "showCameraEvidenceWindow", showCameraEvidenceWindow },
   };
}

void PlayerRearFollowCamera::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
	  return;
   }

   distance = ReadFloat(data, "distance", distance);
   height = ReadFloat(data, "height", height);
   groundedTargetHeight = ReadFloat(data, "groundedTargetHeight", groundedTargetHeight);
   takeoffFramingBlendSeconds = std::max(
      0.0f,
      ReadFloat(data, "takeoffFramingBlendSeconds", takeoffFramingBlendSeconds));
   landingFramingBlendSeconds = std::max(
      0.0f,
      ReadFloat(data, "landingFramingBlendSeconds", landingFramingBlendSeconds));
   airborneDistanceOffset = ReadFloat(data, "airborneDistanceOffset", airborneDistanceOffset);
   airborneFovOffset = ReadFloat(data, "airborneFovOffset", airborneFovOffset);
   airbornePlanetDirectionBlend = ReadFloat(data, "airbornePlanetDirectionBlend", airbornePlanetDirectionBlend);
   airbornePlanetDirectionLerpSpeed = ReadFloat(data, "airbornePlanetDirectionLerpSpeed", airbornePlanetDirectionLerpSpeed);
	jumpPlanetDirectionDelaySeconds = std::max(
	  0.0f,
	  ReadFloat(
		 data,
		 "jumpPlanetDirectionDelaySeconds",
		 ReadFloat(data, "jumpPlanetDirectionRampSeconds", jumpPlanetDirectionDelaySeconds)));
	jumpPlanetDirectionRestoreSeconds = std::max(
	  0.0f,
	  ReadFloat(data, "jumpPlanetDirectionRestoreSeconds", jumpPlanetDirectionRestoreSeconds));
   enableAirbornePlanetDirectionGuide = ReadBool(data, "enableAirbornePlanetDirectionGuide", enableAirbornePlanetDirectionGuide);
   enableAirborneGravityDirectionBoost = ReadBool(data, "enableAirborneGravityDirectionBoost", enableAirborneGravityDirectionBoost);
   airborneGravityDirectionBoostThreshold = ReadFloat(data, "airborneGravityDirectionBoostThreshold", airborneGravityDirectionBoostThreshold);
   airborneGravityDirectionBoostFullThreshold = ReadFloat(data, "airborneGravityDirectionBoostFullThreshold", airborneGravityDirectionBoostFullThreshold);
   airborneGravityDirectionBoostBias = ReadFloat(data, "airborneGravityDirectionBoostBias", airborneGravityDirectionBoostBias);
   airborneBlendLerpSpeed = ReadFloat(data, "airborneBlendLerpSpeed", airborneBlendLerpSpeed);
   enablePreLandingCamera = ReadBool(data, "enablePreLandingCamera", enablePreLandingCamera);
   preLandingPredictionSeconds = std::max(
	  0.0f,
	  ReadFloat(data, "preLandingPredictionSeconds", preLandingPredictionSeconds));
   preLandingFullBlendSeconds = std::max(
	  0.0f,
	  ReadFloat(data, "preLandingFullBlendSeconds", preLandingFullBlendSeconds));
   preLandingBlendLerpSpeed = std::max(
	  0.0f,
	  ReadFloat(data, "preLandingBlendLerpSpeed", preLandingBlendLerpSpeed));
   preLandingReleaseLerpSpeed = std::max(
	  0.0f,
	  ReadFloat(data, "preLandingReleaseLerpSpeed", preLandingReleaseLerpSpeed));
   preLandingTerrainLookAhead = std::max(
	  0.0f,
	  ReadFloat(data, "preLandingTerrainLookAhead", preLandingTerrainLookAhead));
   preLandingTerrainLookBlend = std::clamp(
	  ReadFloat(data, "preLandingTerrainLookBlend", preLandingTerrainLookBlend),
	  0.0f,
	  1.0f);
   preLandingMinOutwardHeight = std::max(
	  0.0f,
	  ReadFloat(data, "preLandingMinOutwardHeight", preLandingMinOutwardHeight));
   airborneForwardLerpSpeed = ReadFloat(data, "airborneForwardLerpSpeed", airborneForwardLerpSpeed);
   rearLerpSpeed = ReadFloat(data, "rearLerpSpeed", rearLerpSpeed);
   landingRearLerpRampSeconds = ReadFloat(data, "landingRearLerpRampSeconds", landingRearLerpRampSeconds);
   gravityUpLerpSpeed = ReadFloat(data, "gravityUpLerpSpeed", gravityUpLerpSpeed);
   fovDefault = ClampCameraFov(ReadFloat(data, "fovDefault", fovDefault));
   fovBoostMax = std::max(0.0f, ReadFloat(data, "fovBoostMax", fovBoostMax));
   fovLerpSpeed = ReadFloat(data, "fovLerpSpeed", fovLerpSpeed);
   distanceBoostMax = ReadFloat(data, "distanceBoostMax", distanceBoostMax);
   springStiffness = ReadFloat(data, "springStiffness", springStiffness);
   springDamping = ReadFloat(data, "springDamping", springDamping);
	// 旧データでは同じ速度差に「加速度係数」と「ターボ最大量」を重ねていた。
	// 最大キックに到達する速度差で両者が作る量を合算し、新しい1項目へ移行する。
	float legacyTurboFovKickMax = std::max(0.0f, ReadFloat(data, "turboFovKickMax", speedChangeFovKickMax));
	float legacyTurboDistanceKickMax = std::max(0.0f, ReadFloat(data, "turboDistanceKickMax", speedChangeDistanceKickMax));
	float legacyFovKickMax = std::min(
	   legacyTurboFovKickMax + std::max(0.0f, ReadFloat(data, "accelToFovKick", 0.0f)) * kSpeedDeltaForMaxKick,
	   std::max(0.0f, fovBoostMax) + legacyTurboFovKickMax);
	float legacyDistanceKickMax = std::min(
	   legacyTurboDistanceKickMax + std::max(0.0f, ReadFloat(data, "accelToDistanceKick", 0.0f)) * kSpeedDeltaForMaxKick,
	   std::max(0.0f, distanceBoostMax) + legacyTurboDistanceKickMax);
	speedChangeFovKickMax = std::max(0.0f, ReadFloat(data, "speedChangeFovKickMax", legacyFovKickMax));
	speedChangeDistanceKickMax = std::max(0.0f, ReadFloat(data, "speedChangeDistanceKickMax", legacyDistanceKickMax));
   speedBoostThreshold = ReadFloat(data, "speedBoostThreshold", speedBoostThreshold);
   speedBoostMax = ReadFloat(data, "speedBoostMax", speedBoostMax);
   positionLerpSpeed = ReadFloat(data, "positionLerpSpeed", positionLerpSpeed);
   eyeDirectionMaxAngularSpeed = std::max(
	  0.0f,
	  ReadFloat(data, "eyeDirectionMaxAngularSpeed", eyeDirectionMaxAngularSpeed));
   rotationLerpSpeed = ReadFloat(data, "rotationLerpSpeed", rotationLerpSpeed);
   lookAtRecoveryRange = std::max(
      1e-4f,
      ReadFloat(data, "lookAtRecoveryRange", lookAtRecoveryRange));
   lookAtRecoveryCurveControl1 = std::clamp(
      ReadFloat(data, "lookAtRecoveryCurveControl1", lookAtRecoveryCurveControl1),
      0.0f,
      1.0f);
   lookAtRecoveryCurveControl2 = std::clamp(
      ReadFloat(data, "lookAtRecoveryCurveControl2", lookAtRecoveryCurveControl2),
      0.0f,
      1.0f);
   showCameraMeasurementOverlay = ReadBool(
      data,
      "showCameraMeasurementOverlay",
      showCameraMeasurementOverlay);
   showCameraEvidenceWindow = ReadBool(
      data,
      "showCameraEvidenceWindow",
      showCameraEvidenceWindow);
   input_.autoSpeed = ReadFloat(data, "autoSpeed", input_.autoSpeed);

   ResetRuntimeState();
}

} // namespace App
