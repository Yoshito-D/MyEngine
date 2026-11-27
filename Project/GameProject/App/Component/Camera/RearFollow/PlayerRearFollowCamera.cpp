#include "GameEngine/Utility/JsonValue.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameProject/App/Component/Camera/RearFollow/PlayerRearFollowCamera.h"
#include "GameProject/App/Component/Camera/RearFollow/RearCameraMath.h"
#include "GameEngine/Scene/Camera/Camera.h"
#include "GameEngine/Scene/Camera/Components/PerlinNoise.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"
#include "GameEngine/Window/Window.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "GameProject/App/Component/Camera/RearFollow/RearCameraDebugView.h"
#include "imgui.h"
#endif

namespace App {
using namespace RearCameraMath;

namespace {
constexpr float kMinimumSpeedBoostRange = 0.001f;
constexpr float kMinimumGravityDirectionBoostRange = 0.001f;
constexpr float kMinimumLookAtRecoveryRange = 1e-4f;
constexpr float kMinimumDriftDistanceScale = 0.5f;
constexpr float kVehicleFramingScreenFraction = 0.95f; ///< 画面端に余白を残すために使用する幅と高さの割合。
}

const bool kRegistered = GameEngine::VirtualCamera::RegisterComponentFactory(
   "PlayerRearFollowCamera", [](GameEngine::VirtualCamera& camera) -> GameEngine::ICinemachineComponent* {
      if (auto* existing = camera.GetComponent<PlayerRearFollowCamera>()) {
         return existing;
      }

      return camera.AddComponent<PlayerRearFollowCamera>();
   });

void PlayerRearFollowCamera::Configure(const RearCameraSettings& requested) {
   auto settings = requested;
   const RearCameraSettings defaults;

   // 補間計算へNaNや無限大が伝わらないよう、範囲制限より先に非有限値を既定値へ戻す。
   if (!std::isfinite(settings.distance)) {
      settings.distance = defaults.distance;
   }

   if (!std::isfinite(settings.height)) {
      settings.height = defaults.height;
   }

   if (!std::isfinite(settings.groundedTargetHeight)) {
      settings.groundedTargetHeight = defaults.groundedTargetHeight;
   }

   if (!std::isfinite(settings.takeoffFramingBlendSeconds)) {
      settings.takeoffFramingBlendSeconds = defaults.takeoffFramingBlendSeconds;
   }

   if (!std::isfinite(settings.landingFramingBlendSeconds)) {
      settings.landingFramingBlendSeconds = defaults.landingFramingBlendSeconds;
   }

   if (!std::isfinite(settings.airborneDistanceOffset)) {
      settings.airborneDistanceOffset = defaults.airborneDistanceOffset;
   }

   if (!std::isfinite(settings.airborneFovOffset)) {
      settings.airborneFovOffset = defaults.airborneFovOffset;
   }

   if (!std::isfinite(settings.airbornePlanetDirectionBlend)) {
      settings.airbornePlanetDirectionBlend = defaults.airbornePlanetDirectionBlend;
   }

   if (!std::isfinite(settings.airbornePlanetDirectionLerpSpeed)) {
      settings.airbornePlanetDirectionLerpSpeed = defaults.airbornePlanetDirectionLerpSpeed;
   }

   if (!std::isfinite(settings.jumpPlanetDirectionDelaySeconds)) {
      settings.jumpPlanetDirectionDelaySeconds = defaults.jumpPlanetDirectionDelaySeconds;
   }

   if (!std::isfinite(settings.jumpPlanetDirectionRestoreSeconds)) {
      settings.jumpPlanetDirectionRestoreSeconds = defaults.jumpPlanetDirectionRestoreSeconds;
   }

   if (!std::isfinite(settings.airborneGravityDirectionBoostThreshold)) {
      settings.airborneGravityDirectionBoostThreshold = defaults.airborneGravityDirectionBoostThreshold;
   }

   if (!std::isfinite(settings.airborneGravityDirectionBoostFullThreshold)) {
      settings.airborneGravityDirectionBoostFullThreshold = defaults.airborneGravityDirectionBoostFullThreshold;
   }

   if (!std::isfinite(settings.airborneGravityDirectionBoostBias)) {
      settings.airborneGravityDirectionBoostBias = defaults.airborneGravityDirectionBoostBias;
   }

   if (!std::isfinite(settings.airborneBlendLerpSpeed)) {
      settings.airborneBlendLerpSpeed = defaults.airborneBlendLerpSpeed;
   }

   if (!std::isfinite(settings.preLandingPredictionSeconds)) {
      settings.preLandingPredictionSeconds = defaults.preLandingPredictionSeconds;
   }

   if (!std::isfinite(settings.preLandingFullBlendSeconds)) {
      settings.preLandingFullBlendSeconds = defaults.preLandingFullBlendSeconds;
   }

   if (!std::isfinite(settings.preLandingBlendLerpSpeed)) {
      settings.preLandingBlendLerpSpeed = defaults.preLandingBlendLerpSpeed;
   }

   if (!std::isfinite(settings.preLandingReleaseLerpSpeed)) {
      settings.preLandingReleaseLerpSpeed = defaults.preLandingReleaseLerpSpeed;
   }

   if (!std::isfinite(settings.preLandingTerrainLookAhead)) {
      settings.preLandingTerrainLookAhead = defaults.preLandingTerrainLookAhead;
   }

   if (!std::isfinite(settings.preLandingTerrainLookBlend)) {
      settings.preLandingTerrainLookBlend = defaults.preLandingTerrainLookBlend;
   }

   if (!std::isfinite(settings.preLandingMinOutwardHeight)) {
      settings.preLandingMinOutwardHeight = defaults.preLandingMinOutwardHeight;
   }

   if (!std::isfinite(settings.airborneForwardLerpSpeed)) {
      settings.airborneForwardLerpSpeed = defaults.airborneForwardLerpSpeed;
   }

   if (!std::isfinite(settings.rearLerpSpeed)) {
      settings.rearLerpSpeed = defaults.rearLerpSpeed;
   }

   if (!std::isfinite(settings.landingRearLerpRampSeconds)) {
      settings.landingRearLerpRampSeconds = defaults.landingRearLerpRampSeconds;
   }

   if (!std::isfinite(settings.gravityUpLerpSpeed)) {
      settings.gravityUpLerpSpeed = defaults.gravityUpLerpSpeed;
   }

   if (!std::isfinite(settings.fovDefault)) {
      settings.fovDefault = defaults.fovDefault;
   }

   if (!std::isfinite(settings.fovBoostMax)) {
      settings.fovBoostMax = defaults.fovBoostMax;
   }

   if (!std::isfinite(settings.fovLerpSpeed)) {
      settings.fovLerpSpeed = defaults.fovLerpSpeed;
   }

   if (!std::isfinite(settings.distanceBoostMax)) {
      settings.distanceBoostMax = defaults.distanceBoostMax;
   }

   if (!std::isfinite(settings.springStiffness)) {
      settings.springStiffness = defaults.springStiffness;
   }

   if (!std::isfinite(settings.springDamping)) {
      settings.springDamping = defaults.springDamping;
   }

   if (!std::isfinite(settings.speedChangeFovKickMax)) {
      settings.speedChangeFovKickMax = defaults.speedChangeFovKickMax;
   }

   if (!std::isfinite(settings.speedChangeDistanceKickMax)) {
      settings.speedChangeDistanceKickMax = defaults.speedChangeDistanceKickMax;
   }

   if (!std::isfinite(settings.speedBoostThreshold)) {
      settings.speedBoostThreshold = defaults.speedBoostThreshold;
   }

   if (!std::isfinite(settings.speedBoostMax)) {
      settings.speedBoostMax = defaults.speedBoostMax;
   }

   if (!std::isfinite(settings.positionLerpSpeed)) {
      settings.positionLerpSpeed = defaults.positionLerpSpeed;
   }

   if (!std::isfinite(settings.eyeDirectionMaxAngularSpeed)) {
      settings.eyeDirectionMaxAngularSpeed = defaults.eyeDirectionMaxAngularSpeed;
   }

   if (!std::isfinite(settings.rotationLerpSpeed)) {
      settings.rotationLerpSpeed = defaults.rotationLerpSpeed;
   }

   if (!std::isfinite(settings.lookAtRecoveryRange)) {
      settings.lookAtRecoveryRange = defaults.lookAtRecoveryRange;
   }

   if (!std::isfinite(settings.lookAtRecoveryCurveControl1)) {
      settings.lookAtRecoveryCurveControl1 = defaults.lookAtRecoveryCurveControl1;
   }

   if (!std::isfinite(settings.lookAtRecoveryCurveControl2)) {
      settings.lookAtRecoveryCurveControl2 = defaults.lookAtRecoveryCurveControl2;
   }

   settings.distance = std::max(settings.distance, 0.0f);

   // FOVはラジアンで保持し、投影が退化する0度・180度を避けて1度〜179度に制限する。
   settings.fovDefault = std::clamp(settings.fovDefault, kMinSafeFov, kMaxSafeFov);

   // 強度を正規化する区間には幅を持たせ、着地の最大ブレンド時刻は予測区間内に収める。
   settings.speedBoostMax = std::max(settings.speedBoostThreshold + kMinimumSpeedBoostRange, settings.speedBoostMax);
   settings.preLandingFullBlendSeconds =
      std::clamp(settings.preLandingFullBlendSeconds, 0.0f, std::max(0.0f, settings.preLandingPredictionSeconds));
   settings.airborneGravityDirectionBoostFullThreshold =
      std::max(settings.airborneGravityDirectionBoostThreshold + kMinimumGravityDirectionBoostRange,
         settings.airborneGravityDirectionBoostFullThreshold);
   static_cast<RearCameraSettings&>(*this) = settings;
}

void PlayerRearFollowCamera::ResetRuntimeState() {
   input_.gravityUp = {0.0f, 1.0f, 0.0f};
   input_.pivotTarget = {0.0f, 0.0f, 0.0f};
   input_.planetCenter = {0.0f, 0.0f, 0.0f};
   input_.followForward = {0.0f, 0.0f, 1.0f};
   input_.airborneMoveForward = {0.0f, 0.0f, 1.0f};
   input_.isAirborne = false;
   input_.landingPredictionValid = false;
   input_.predictedLandingUp = {0.0f, 1.0f, 0.0f};
   input_.predictedLandingBackward = {0.0f, 0.0f, -1.0f};
   input_.predictedLandingContact = {0.0f, 0.0f, 0.0f};
   input_.predictedLandingSeconds = 0.0f;
   input_.playerSpeed = 0.0f;
   input_.driftDistanceScale = 1.0f;
   input_.driftCameraHeightOffset = 0.0f;
   input_.driftLookTargetHeightOffset = 0.0f;
   input_.playerVelocity = {0.0f, 0.0f, 0.0f};
   landingFailureFramingRadius_ = 0.0f;
   landingShakeAmplitude_ = 0.0f;
   framingRetreat_ = 0.0f;

   transition_.Reset(*this);
   gravity_.Reset(*this);
   planet_.Reset(*this);
   direction_.Reset(*this);
   speed_.Reset(*this);
   position_.Reset(*this);
   aim_.Reset(*this);

   if (auto* owner = GetOwnerCamera()) {
      // 演出側のFOVを初期化した直後にカメラ本体へも反映し、次回更新まで以前の画角が残るのを防ぐ。
      auto state = owner->GetState();
      state.fov = speed_.GetState().currentFov;
      owner->SetState(state);
   }
}

void PlayerRearFollowCamera::SetLandingPrediction(const GameEngine::Vector3& up, const GameEngine::Vector3& backward,
   const GameEngine::Vector3& contactPoint, float secondsToImpact) {
   // 予測方向がほぼゼロの場合は直前の方向を使い、着地への姿勢補間へゼロベクトルを渡さない。
   input_.predictedLandingUp = NormalizeOrFallback(up, input_.predictedLandingUp);
   input_.predictedLandingBackward = NormalizeOrFallback(backward, input_.predictedLandingBackward);
   input_.predictedLandingContact = contactPoint;
   input_.predictedLandingSeconds = std::max(0.0f, secondsToImpact);
   input_.landingPredictionValid = true;
}

void PlayerRearFollowCamera::OnAttach() {
   ResetRuntimeState();
}

void PlayerRearFollowCamera::MutateCameraState(GameEngine::CameraState& state, float deltaTime) {
   // 遷移を先に確定し、同一フレームのUpを方向・位置・注視点へ順に伝える。
   const auto events = transition_.BeginFrame(input_, aim_.GetState().lastLookTargetOffset);
   transition_.UpdatePreLandingBlend(input_, *this, deltaTime);
   transition_.UpdateAirborneBlend(input_, *this, deltaTime);
   transition_.UpdatePlayerFramingBlend(input_, *this, deltaTime);
   transition_.EndFrame(input_.isAirborne);
   const auto oldUp = gravity_.GetState().currentGravityUp;
   const auto up = gravity_.SmoothGravityUp(input_, *this, transition_.GetState(), aim_.GetState(), deltaTime);
   direction_.TransportWithGravity(oldUp, up);
   planet_.AdvanceTakeoffTimer(input_, *this, events, direction_.GetState().currentBackward, deltaTime);
   planet_.UpdatePlanetDirectionGuide(input_, *this, direction_.GetState().currentBackward, deltaTime);
   direction_.BeginLanding(events.justLanded);
   const float planetBlend =
      planet_.ComputePlanetDirectionBlend(input_, *this, transition_.GetState(), direction_.GetState().currentBackward);
   direction_.UpdateBackwardVector(
      input_, *this, transition_.GetState(), aim_.GetState(), planet_.GetState(), planetBlend, up, deltaTime);
   const float boostAlpha = speed_.UpdateAccelerationEffect(input_, *this, transition_.GetState(), state, deltaTime);
   const auto eye = position_.Update(input_, *this, transition_.GetState(), direction_.GetState(), speed_.GetState(),
      aim_.GetState(), state, up, boostAlpha, deltaTime);
   aim_.ApplyLookAt(input_, *this, transition_.GetState(), transition_.ComputePreLandingGuideBlend(input_),
      direction_.GetState(), state, eye, up, deltaTime);
   KeepFailedLandingInFrame(state, deltaTime);
}

void PlayerRearFollowCamera::SetLandingFailureFraming(
   bool groundedAfterFailure, float vehicleRadius, float shakeAmplitude) {
   landingFailureFramingRadius_ = groundedAfterFailure ? vehicleRadius : 0.0f;
   landingShakeAmplitude_ = shakeAmplitude;

   if (!groundedAfterFailure) {
      // このカメラが選択されていない間の離陸でも、補正を次の着地へ持ち越さない。
      framingRetreat_ = 0.0f;
   }
}

void PlayerRearFollowCamera::KeepFailedLandingInFrame(GameEngine::CameraState& state, float deltaTime) {
   using namespace GameEngine;

   if (input_.isAirborne) {
      framingRetreat_ = 0.0f;
      return;
   }

   if (landingFailureFramingRadius_ <= 0.0f && framingRetreat_ <= 0.0f) return;

   float requiredRetreat = 0.0f;
   auto view = state.GetViewMatrix();

   if (landingFailureFramingRadius_ > 0.0f) {
      const auto* output = EngineContext::GetActiveCamera();
      constexpr float defaultAspect = static_cast<float>(Window::kResolutionWidth) / Window::kResolutionHeight;
      const float aspect = output ? output->GetAspectRatio() : defaultAspect;
      const auto* owner = GetOwnerCamera();
      const auto* noise = owner ? owner->GetComponent<PerlinNoise>() : nullptr;

      // 後段でシェイクが加わっても収まるよう、揺れている間は移動量の分だけ境界を広げる。
      const float shakePadding = noise && noise->IsEnabled() && noise->IsShaking() ? landingShakeAmplitude_ : 0.0f;
      const float radius = landingFailureFramingRadius_ + shakePadding;
      const Vector3 center = TransformCoordinate(input_.pivotTarget, view);
      const float slopeY = std::tan(state.fov * 0.5f) * kVehicleFramingScreenFraction;
      const float slopeX = slopeY * aspect;

      // 視錐台の各面から車体を囲む球の半径分の距離を確保するために必要な奥行き。
      const float depthX = (std::abs(center.x) + radius * std::hypot(1.0f, slopeX)) / slopeX;
      const float depthY = (std::abs(center.y) + radius * std::hypot(1.0f, slopeY)) / slopeY;
      const float minimumDepth = std::max({depthX, depthY, state.nearClip + radius});
      requiredRetreat = std::max(0.0f, minimumDepth - center.z);
   }

   // 同じ失敗の接地中は、必要な後退距離が減ったときも通常の追従速度で距離を戻す。
   const float blend = ExpSmoothingFactor(positionLerpSpeed, deltaTime);
   framingRetreat_ = std::max(requiredRetreat, std::lerp(framingRetreat_, requiredRetreat, blend));

   if (framingRetreat_ <= 0.0f) return;

   state.transform.translation -= aim_.GetState().cachedForward * framingRetreat_;
   view.m[3][2] += framingRetreat_;
   state.SetViewMatrix(view);
}

GameEngine::Vector3 PlayerRearFollowCamera::GetCameraUp() const {
   return aim_.GetState().cachedUp;
}

GameEngine::Vector3 PlayerRearFollowCamera::GetCameraRight() const {
   return aim_.GetState().cachedRight;
}

#ifdef USE_IMGUI
void PlayerRearFollowCamera::DrawInspector() {
   bool enabled = IsEnabled();
   const auto sharedBefore = GetSharedParameters();
   auto settings = DescribeSettings();
   RearCameraDebugView::DrawInspector(settings, enabled);
   Configure(settings);
   CommitParameterEdit(sharedBefore, ImGui::IsAnyItemActive());
   SetEnabled(enabled);
}
#endif

nlohmann::json PlayerRearFollowCamera::Serialize() const {
   return nlohmann::json{
      {"distance", distance},
      {"height", height},
      {"groundedTargetHeight", groundedTargetHeight},
      {"takeoffFramingBlendSeconds", takeoffFramingBlendSeconds},
      {"landingFramingBlendSeconds", landingFramingBlendSeconds},
      {"airborneDistanceOffset", airborneDistanceOffset},
      {"airborneFovOffset", airborneFovOffset},
      {"airbornePlanetDirectionBlend", airbornePlanetDirectionBlend},
      {"airbornePlanetDirectionLerpSpeed", airbornePlanetDirectionLerpSpeed},
      {"jumpPlanetDirectionDelaySeconds", jumpPlanetDirectionDelaySeconds},
      {"jumpPlanetDirectionRestoreSeconds", jumpPlanetDirectionRestoreSeconds},
      {"enableAirbornePlanetDirectionGuide", enableAirbornePlanetDirectionGuide},
      {"enableAirborneGravityDirectionBoost", enableAirborneGravityDirectionBoost},
      {"airborneGravityDirectionBoostThreshold", airborneGravityDirectionBoostThreshold},
      {"airborneGravityDirectionBoostFullThreshold", airborneGravityDirectionBoostFullThreshold},
      {"airborneGravityDirectionBoostBias", airborneGravityDirectionBoostBias},
      {"airborneBlendLerpSpeed", airborneBlendLerpSpeed},
      {"enablePreLandingCamera", enablePreLandingCamera},
      {"preLandingPredictionSeconds", preLandingPredictionSeconds},
      {"preLandingFullBlendSeconds", preLandingFullBlendSeconds},
      {"preLandingBlendLerpSpeed", preLandingBlendLerpSpeed},
      {"preLandingReleaseLerpSpeed", preLandingReleaseLerpSpeed},
      {"preLandingTerrainLookAhead", preLandingTerrainLookAhead},
      {"preLandingTerrainLookBlend", preLandingTerrainLookBlend},
      {"preLandingMinOutwardHeight", preLandingMinOutwardHeight},
      {"airborneForwardLerpSpeed", airborneForwardLerpSpeed},
      {"rearLerpSpeed", rearLerpSpeed},
      {"landingRearLerpRampSeconds", landingRearLerpRampSeconds},
      {"gravityUpLerpSpeed", gravityUpLerpSpeed},
      {"fovDefault", fovDefault},
      {"fovBoostMax", fovBoostMax},
      {"fovLerpSpeed", fovLerpSpeed},
      {"distanceBoostMax", distanceBoostMax},
      {"springStiffness", springStiffness},
      {"springDamping", springDamping},
      {"speedChangeFovKickMax", speedChangeFovKickMax},
      {"speedChangeDistanceKickMax", speedChangeDistanceKickMax},
      {"speedBoostThreshold", speedBoostThreshold},
      {"speedBoostMax", speedBoostMax},
      {"positionLerpSpeed", positionLerpSpeed},
      {"eyeDirectionMaxAngularSpeed", eyeDirectionMaxAngularSpeed},
      {"rotationLerpSpeed", rotationLerpSpeed},
      {"lookAtRecoveryRange", lookAtRecoveryRange},
      {"lookAtRecoveryCurveControl1", lookAtRecoveryCurveControl1},
      {"lookAtRecoveryCurveControl2", lookAtRecoveryCurveControl2},
   };
}

nlohmann::json PlayerRearFollowCamera::GetSharedParameters() const {
   return ExtractSharedParameters({"autoSpeed"});
}

void PlayerRearFollowCamera::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void PlayerRearFollowCamera::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void PlayerRearFollowCamera::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   if (!data.is_object()) return;

   distance = GameEngine::JsonValue::ReadFloat(data, "distance", distance);
   height = GameEngine::JsonValue::ReadFloat(data, "height", height);
   groundedTargetHeight = GameEngine::JsonValue::ReadFloat(data, "groundedTargetHeight", groundedTargetHeight);
   takeoffFramingBlendSeconds =
      std::max(0.0f, GameEngine::JsonValue::ReadFloat(data, "takeoffFramingBlendSeconds", takeoffFramingBlendSeconds));
   landingFramingBlendSeconds =
      std::max(0.0f, GameEngine::JsonValue::ReadFloat(data, "landingFramingBlendSeconds", landingFramingBlendSeconds));
   airborneDistanceOffset = GameEngine::JsonValue::ReadFloat(data, "airborneDistanceOffset", airborneDistanceOffset);
   airborneFovOffset = GameEngine::JsonValue::ReadFloat(data, "airborneFovOffset", airborneFovOffset);
   airbornePlanetDirectionBlend =
      GameEngine::JsonValue::ReadFloat(data, "airbornePlanetDirectionBlend", airbornePlanetDirectionBlend);
   airbornePlanetDirectionLerpSpeed =
      GameEngine::JsonValue::ReadFloat(data, "airbornePlanetDirectionLerpSpeed", airbornePlanetDirectionLerpSpeed);
   jumpPlanetDirectionDelaySeconds = std::max(
      0.0f, GameEngine::JsonValue::ReadFloat(data, "jumpPlanetDirectionDelaySeconds", jumpPlanetDirectionDelaySeconds));
   jumpPlanetDirectionRestoreSeconds = std::max(0.0f,
      GameEngine::JsonValue::ReadFloat(data, "jumpPlanetDirectionRestoreSeconds", jumpPlanetDirectionRestoreSeconds));
   enableAirbornePlanetDirectionGuide =
      GameEngine::JsonValue::ReadBool(data, "enableAirbornePlanetDirectionGuide", enableAirbornePlanetDirectionGuide);
   enableAirborneGravityDirectionBoost =
      GameEngine::JsonValue::ReadBool(data, "enableAirborneGravityDirectionBoost", enableAirborneGravityDirectionBoost);
   airborneGravityDirectionBoostThreshold = GameEngine::JsonValue::ReadFloat(
      data, "airborneGravityDirectionBoostThreshold", airborneGravityDirectionBoostThreshold);
   airborneGravityDirectionBoostFullThreshold = GameEngine::JsonValue::ReadFloat(
      data, "airborneGravityDirectionBoostFullThreshold", airborneGravityDirectionBoostFullThreshold);
   airborneGravityDirectionBoostBias =
      GameEngine::JsonValue::ReadFloat(data, "airborneGravityDirectionBoostBias", airborneGravityDirectionBoostBias);
   airborneBlendLerpSpeed = GameEngine::JsonValue::ReadFloat(data, "airborneBlendLerpSpeed", airborneBlendLerpSpeed);
   enablePreLandingCamera = GameEngine::JsonValue::ReadBool(data, "enablePreLandingCamera", enablePreLandingCamera);
   preLandingPredictionSeconds = std::max(
      0.0f, GameEngine::JsonValue::ReadFloat(data, "preLandingPredictionSeconds", preLandingPredictionSeconds));
   preLandingFullBlendSeconds =
      std::max(0.0f, GameEngine::JsonValue::ReadFloat(data, "preLandingFullBlendSeconds", preLandingFullBlendSeconds));
   preLandingBlendLerpSpeed =
      std::max(0.0f, GameEngine::JsonValue::ReadFloat(data, "preLandingBlendLerpSpeed", preLandingBlendLerpSpeed));
   preLandingReleaseLerpSpeed =
      std::max(0.0f, GameEngine::JsonValue::ReadFloat(data, "preLandingReleaseLerpSpeed", preLandingReleaseLerpSpeed));
   preLandingTerrainLookAhead =
      std::max(0.0f, GameEngine::JsonValue::ReadFloat(data, "preLandingTerrainLookAhead", preLandingTerrainLookAhead));
   preLandingTerrainLookBlend = std::clamp(
      GameEngine::JsonValue::ReadFloat(data, "preLandingTerrainLookBlend", preLandingTerrainLookBlend), 0.0f, 1.0f);
   preLandingMinOutwardHeight =
      std::max(0.0f, GameEngine::JsonValue::ReadFloat(data, "preLandingMinOutwardHeight", preLandingMinOutwardHeight));
   airborneForwardLerpSpeed =
      GameEngine::JsonValue::ReadFloat(data, "airborneForwardLerpSpeed", airborneForwardLerpSpeed);
   rearLerpSpeed = GameEngine::JsonValue::ReadFloat(data, "rearLerpSpeed", rearLerpSpeed);
   landingRearLerpRampSeconds =
      GameEngine::JsonValue::ReadFloat(data, "landingRearLerpRampSeconds", landingRearLerpRampSeconds);
   gravityUpLerpSpeed = GameEngine::JsonValue::ReadFloat(data, "gravityUpLerpSpeed", gravityUpLerpSpeed);
   fovDefault = ClampCameraFov(GameEngine::JsonValue::ReadFloat(data, "fovDefault", fovDefault));
   fovBoostMax = std::max(0.0f, GameEngine::JsonValue::ReadFloat(data, "fovBoostMax", fovBoostMax));
   fovLerpSpeed = GameEngine::JsonValue::ReadFloat(data, "fovLerpSpeed", fovLerpSpeed);
   distanceBoostMax = GameEngine::JsonValue::ReadFloat(data, "distanceBoostMax", distanceBoostMax);
   springStiffness = GameEngine::JsonValue::ReadFloat(data, "springStiffness", springStiffness);
   springDamping = GameEngine::JsonValue::ReadFloat(data, "springDamping", springDamping);

   speedChangeFovKickMax =
      std::max(0.0f, GameEngine::JsonValue::ReadFloat(data, "speedChangeFovKickMax", speedChangeFovKickMax));
   speedChangeDistanceKickMax =
      std::max(0.0f, GameEngine::JsonValue::ReadFloat(data, "speedChangeDistanceKickMax", speedChangeDistanceKickMax));
   speedBoostThreshold = GameEngine::JsonValue::ReadFloat(data, "speedBoostThreshold", speedBoostThreshold);
   speedBoostMax = GameEngine::JsonValue::ReadFloat(data, "speedBoostMax", speedBoostMax);
   positionLerpSpeed = GameEngine::JsonValue::ReadFloat(data, "positionLerpSpeed", positionLerpSpeed);
   eyeDirectionMaxAngularSpeed = std::max(
      0.0f, GameEngine::JsonValue::ReadFloat(data, "eyeDirectionMaxAngularSpeed", eyeDirectionMaxAngularSpeed));
   rotationLerpSpeed = GameEngine::JsonValue::ReadFloat(data, "rotationLerpSpeed", rotationLerpSpeed);
   lookAtRecoveryRange = std::max(
      kMinimumLookAtRecoveryRange, GameEngine::JsonValue::ReadFloat(data, "lookAtRecoveryRange", lookAtRecoveryRange));
   lookAtRecoveryCurveControl1 = std::clamp(
      GameEngine::JsonValue::ReadFloat(data, "lookAtRecoveryCurveControl1", lookAtRecoveryCurveControl1), 0.0f, 1.0f);
   lookAtRecoveryCurveControl2 = std::clamp(
      GameEngine::JsonValue::ReadFloat(data, "lookAtRecoveryCurveControl2", lookAtRecoveryCurveControl2), 0.0f, 1.0f);
   input_.autoSpeed = GameEngine::JsonValue::ReadFloat(data, "autoSpeed", input_.autoSpeed);

   // シーン復元時だけ履歴を初期化し、共有設定を編集中に反映するときは追従・補間状態を維持する。
   if (initializeRuntime) {
      ResetRuntimeState();
   }
   Configure(DescribeSettings());
}

} // namespace App

namespace App {

void PlayerRearFollowCamera::ApplyVehicleFollowInput(const VehicleCameraFollowInput& input) {
   input_.gravityUp = input.gravityUp;
   input_.pivotTarget = input.target;
   input_.planetCenter = input.landingPlanetCenter;
   input_.hasLandingPlanet = input.hasLandingPlanet;
   input_.playerSpeed = input.speed;
   input_.autoSpeed = input.autoSpeed;
}

void PlayerRearFollowCamera::ApplyVehicleEffectInput(const VehicleCameraEffectInput& input) {
   input_.driftDistanceScale =
      std::clamp(std::isfinite(input.distanceScale) ? input.distanceScale : 1.0f, kMinimumDriftDistanceScale, 1.0f);
   input_.driftCameraHeightOffset = std::isfinite(input.cameraHeightOffset) ? input.cameraHeightOffset : 0.0f;
   input_.driftLookTargetHeightOffset =
      std::isfinite(input.lookTargetHeightOffset) ? input.lookTargetHeightOffset : 0.0f;
}

} // namespace App
