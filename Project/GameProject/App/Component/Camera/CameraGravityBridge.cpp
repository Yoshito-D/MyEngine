#include "GameProject/App/Component/Camera/CameraGravityBridge.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include <cstdint>
#include "GameProject/App/Component/Camera/CameraModeSwitcher.h"
#include "GameProject/App/Component/Camera/LandingPrediction.h"
#include "GameProject/App/Component/Character/CharacterLanding.h"
#include "GameProject/App/Component/Character/CharacterJump.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"
#include "GameProject/App/Component/Vehicle/VehicleGroundMover.h"
#include "GameProject/App/Component/Vehicle/VehicleAirController.h"
#include "GameProject/App/Component/Vehicle/VehicleController.h"
#include "GameProject/App/Component/Vehicle/VehicleDrift.h"
#include "GameProject/App/Component/Vehicle/VehicleInputComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleMover.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {
namespace {
constexpr float kVehicleFramingPadding = 0.1f; ///< 接地判定の境界からはみ出す車体やタイヤに確保する余白（m）。
}

void CameraGravityBridge::OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) {
   cameras_.clear();
   playerRearFollowCamera_ = nullptr;

   for (const auto& id : cameraIds_) {
      auto* camera = sceneWorld.FindVirtualCamera(id);
      auto* follow = camera ? camera->GetComponent<IVehicleCamera>() : nullptr;

      if (follow && std::find(cameras_.begin(), cameras_.end(), follow) == cameras_.end()) {
         cameras_.push_back(follow);
      }

      if (camera && !playerRearFollowCamera_) {
         playerRearFollowCamera_ = camera->GetComponent<PlayerRearFollowCamera>();
      }
   }
}

void CameraGravityBridge::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   OnReferencesChanged(sceneWorld);
   const auto* events = HasOwner() ? GetOwner().GetComponent<VehicleEvents>() : nullptr;
   effects_.Reset(events ? events->GetSnapshot() : VehicleEventSnapshot{});
}

void CameraGravityBridge::Update(float deltaTime) {
   // オーナー不在時は更新しない
   if (!HasOwner()) return;

   // ワールド座標の取得元（Transform）を参照
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();

   if (!transform) return;

   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   auto* landing = GetOwner().GetComponent<CharacterLanding>();
   auto* jump = GetOwner().GetComponent<CharacterJump>();
   auto* switcher = GetOwner().GetComponent<PlanetSwitcher>();
   const auto* drift = GetOwner().GetComponent<VehicleDrift>();
   const auto* input = GetOwner().GetComponent<VehicleInputComponent>();

   const auto* cameraMode = GetOwner().GetComponent<CameraModeSwitcher>();
   const auto* selectedCamera = cameraMode ? cameraMode->GetSelectedCamera() : nullptr;

   // 着地済みの基準を参照側から取得し、惑星選択側からの設定書き換えを不要にする。
   if (switcher) {
      float radius = 0.0f;
      switcher->TryGetCurrentPlanet(planetCenter_, radius);
   }

   // 惑星中心→自身方向を正規化して重力Upを作る
   GameEngine::Vector3 toSelf = transform->GetLocalPose().translation - planetCenter_;
   float len = toSelf.Length();

   if (len < 1e-4f) return;

   GameEngine::Vector3 gravityUp = toSelf * (1.0f / len);
   GameEngine::Vector3 pos = transform->GetLocalPose().translation;
   GameEngine::Vector3 cameraPlanetCenter = planetCenter_;
   float cameraPlanetSurfaceRadius = 0.0f;
   bool hasCameraPlanet = false;

   if (switcher) {
      // 空中切替中は確定惑星ではなく保留中の着地先を使い、予測と重力先を一致させる。
      hasCameraPlanet = switcher->TryGetLandingPlanet(cameraPlanetCenter, cameraPlanetSurfaceRadius);
   }

   float speed = 0.0f;
   float autoSpeed = 13.0f;

   const auto* groundMover = GetOwner().GetComponent<VehicleGroundMover>();

   if (groundMover) {
      speed = groundMover->GetCurrentSpeed();
      autoSpeed = groundMover->DescribeSettings().autoSpeed;
   }

   const VehicleCameraFollowInput followInput{.gravityUp = gravityUp,
      .target = pos,
      .planetCenter = planetCenter_,
      .landingPlanetCenter = cameraPlanetCenter,
      .speed = speed,
      .autoSpeed = autoSpeed};

   for (auto* camera : cameras_) {
      camera->ApplyVehicleFollowInput(followInput);
   }

   const auto* events = GetOwner().GetComponent<VehicleEvents>();

   // PlayerRearFollowCamera 側へ重力Up・注視対象・近傍惑星・前方・空中状態を同期
   if (playerRearFollowCamera_) {
      GameEngine::Vector3 forward = {0.0f, 0.0f, 1.0f};
      GameEngine::Quaternion rotation = transform->GetLocalPose().GetActiveQuaternion();
      forward = GameEngine::RotateVector(forward, rotation);

      bool isAirborne = false;

      if (jump) {
         isAirborne = jump->IsJumping();
      }

      GameEngine::Vector3 airborneMoveForward = forward;
      GameEngine::Vector3 playerVelocity = {0.0f, 0.0f, 0.0f};

      if (gravityBody) {
         GameEngine::Vector3 velocity = gravityBody->GetVelocity();
         playerVelocity = velocity;

         // 重力方向成分を除いた進行方向を使い、上下速度だけでカメラが真上/真下を向くのを避ける。
         GameEngine::Vector3 horizontalVelocity = velocity - gravityUp * velocity.Dot(gravityUp);
         float horizontalSpeed = horizontalVelocity.Length();

         if (horizontalSpeed > 1e-4f) {
            airborneMoveForward = horizontalVelocity * (1.0f / horizontalSpeed);
         }
      }

      playerRearFollowCamera_->SetFollowForward(forward);
      playerRearFollowCamera_->SetAirborneMoveForward(airborneMoveForward);
      playerRearFollowCamera_->SetAirborne(isAirborne);
      playerRearFollowCamera_->SetPlayerVelocity(playerVelocity);

      // 加速禁止はジャンプ中も続くため、失敗した着地の接地中だけ見切れを補正する。
      const bool groundedAfterFailure = landing && events && events->IsEnabled() && events->GetSnapshot().isGrounded &&
                                        events->GetSnapshot().landingResult == LandingResult::Failure;
      const bool failedLandingPenalty =
         groundedAfterFailure && groundMover && groundMover->IsEnabled() && groundMover->IsAccelerationBlocked();
      const float framingRadius =
         failedLandingPenalty ? landing->DescribeSettings().obbHalfExtents.Length() + kVehicleFramingPadding : 0.0f;
      playerRearFollowCamera_->SetLandingFailureFraming(
         groundedAfterFailure, framingRadius, settings_.landingShakeAmplitude);

      // 条件を満たさないフレームでは前回値を残さず、カメラ側の予測補間を確実に解除する。
      playerRearFollowCamera_->ClearLandingPrediction();

      if (isAirborne && hasCameraPlanet && gravityBody && landing) {
         const auto* air = GetOwner().GetComponent<VehicleAirController>();
         const auto* controller = GetOwner().GetComponent<VehicleController>();
         const auto* mover = GetOwner().GetComponent<VehicleMover>();
         const bool appliesAirDrag =
            air && air->IsEnabled() && controller && controller->IsEnabled() && mover && mover->IsEnabled();
         const float airDrag = appliesAirDrag ? air->GetAirDrag() : 0.0f;
         float predictionHorizon = std::max(0.0f, playerRearFollowCamera_->GetPreLandingPredictionHorizon());
         LandingPrediction prediction{};
         const bool hasLandingPrediction =
            PredictLanding(pos, playerVelocity, cameraPlanetCenter, cameraPlanetSurfaceRadius,
               landing->DescribeSettings().landingOffset, landing->DescribeSettings().obbHalfExtents, rotation,
               *gravityBody, airDrag, predictionHorizon, forward, playerRearFollowCamera_->GetCameraRight(), prediction);

         if (hasLandingPrediction) {
            playerRearFollowCamera_->SetLandingPrediction(
               prediction.up, prediction.backward, prediction.contactPoint, prediction.secondsToImpact);
         }
      }
   }

   const bool grounded = (!landing || landing->IsGrounded()) && (!jump || !jump->IsJumping());
   const bool steering = grounded && drift && drift->IsEnabled() && input && input->GetSteerInput() != 0.0f;
   effects_.Update(settings_, deltaTime, steering, events ? events->GetSnapshot() : VehicleEventSnapshot{}, gravityUp,
      cameras_, selectedCamera);
}

#ifdef USE_IMGUI
void CameraGravityBridge::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   for (size_t index = 0; index < cameraIds_.size(); ++index) {
      ImGui::PushID(static_cast<std::int32_t>(index));
      GameEngine::EditorUI::CameraReference("Follow Camera", cameraIds_[index]);
      ImGui::SameLine();
      const bool remove = ImGui::SmallButton("Remove");
      ImGui::PopID();

      if (remove) {
         cameraIds_.erase(cameraIds_.begin() + index);
         break;
      }
   }

   if (ImGui::Button("Add Follow Camera")) {
      cameraIds_.emplace_back();
   }

   ImGui::Separator();
   ImGui::Text(
      "%s: (%.2f, %.2f, %.2f)", Tr("惑星中心", "Planet Center"), planetCenter_.x, planetCenter_.y, planetCenter_.z);
   ImGui::Text("Resolved vehicle cameras: %zu", cameras_.size());
   ImGui::Separator();
   ImGui::Checkbox(Tr("着地シェイク", "Landing Shake"), &settings_.enableLandingShake);
   ImGui::DragFloat(Tr("シェイク振幅 (m)", "Shake Amplitude (m)"), &settings_.landingShakeAmplitude, 0.01f, 0.0f, 5.0f);
   ImGui::DragFloat(
      Tr("シェイク周波数 (Hz)", "Shake Frequency (Hz)"), &settings_.landingShakeFrequency, 0.1f, 0.1f, 60.0f);
   ImGui::DragFloat(Tr("シェイク時間 (s)", "Shake Duration (s)"), &settings_.landingShakeDuration, 0.01f, 0.0f, 3.0f);
   ImGui::Separator();
   ImGui::Checkbox(Tr("ミニターボシェイク", "Mini Turbo Shake"), &settings_.enableMiniTurboShake);
   ImGui::DragFloat(Tr("ミニターボ横揺れ振幅 (m)", "Mini Turbo Horizontal Amplitude (m)"),
      &settings_.miniTurboShakeHorizontalAmplitude, 0.01f, 0.0f, 5.0f);
   ImGui::DragFloat(Tr("ミニターボ縦揺れ振幅 (m)", "Mini Turbo Vertical Amplitude (m)"),
      &settings_.miniTurboShakeVerticalAmplitude, 0.01f, 0.0f, 5.0f);
   ImGui::DragFloat(Tr("ミニターボシェイク周波数 (Hz)", "Mini Turbo Shake Frequency (Hz)"),
      &settings_.miniTurboShakeFrequency, 0.1f, 0.1f, 60.0f);
   ImGui::DragFloat(Tr("ミニターボシェイク時間 (s)", "Mini Turbo Shake Duration (s)"),
      &settings_.miniTurboShakeDuration, 0.01f, 0.0f, 3.0f);
   ImGui::Separator();
   ImGui::Checkbox(
      Tr("横入力時にカメラを近づける", "Camera Approach on Steering"), &settings_.enableDriftCameraApproach);
   ImGui::DragFloat(Tr("ドリフト時の距離短縮率", "Drift Distance Reduction"), &settings_.driftCameraDistanceReduction,
      0.01f, 0.0f, 0.5f);
   ImGui::DragFloat(Tr("ドリフト時のカメラ高さ追加量 (m)", "Drift Camera Height Offset (m)"),
      &settings_.driftCameraHeightOffset, 0.05f, -20.0f, 20.0f);
   ImGui::DragFloat(Tr("ドリフト時の注視点高さ追加量 (m)", "Drift Look Target Height Offset (m)"),
      &settings_.driftLookTargetHeightOffset, 0.05f, -10.0f, 10.0f);
   ImGui::TextDisabled("%s", Tr("高さは通常時への加算量です（正: 上、負: 下）。",
                                "Height offsets are added to normal height (positive: up, negative: down)."));
   ImGui::DragFloat(Tr("ドリフトカメラの補間速度 (1/s)", "Drift Camera Lerp Speed (1/s)"),
      &settings_.driftCameraLerpSpeed, 0.1f, 0.1f, 30.0f);
   Configure(DescribeSettings());
}
#endif

nlohmann::json CameraGravityBridge::Serialize() const {
   nlohmann::json json;
   json["cameraIds"] = cameraIds_;
   json["enableLandingShake"] = settings_.enableLandingShake;
   json["landingShakeAmplitude"] = settings_.landingShakeAmplitude;
   json["landingShakeFrequency"] = settings_.landingShakeFrequency;
   json["landingShakeDuration"] = settings_.landingShakeDuration;
   json["enableMiniTurboShake"] = settings_.enableMiniTurboShake;
   json["miniTurboShakeHorizontalAmplitude"] = settings_.miniTurboShakeHorizontalAmplitude;
   json["miniTurboShakeVerticalAmplitude"] = settings_.miniTurboShakeVerticalAmplitude;
   json["miniTurboShakeFrequency"] = settings_.miniTurboShakeFrequency;
   json["miniTurboShakeDuration"] = settings_.miniTurboShakeDuration;
   json["enableDriftCameraApproach"] = settings_.enableDriftCameraApproach;
   json["driftCameraDistanceReduction"] = settings_.driftCameraDistanceReduction;
   json["driftCameraHeightOffset"] = settings_.driftCameraHeightOffset;
   json["driftLookTargetHeightOffset"] = settings_.driftLookTargetHeightOffset;
   json["driftCameraLerpSpeed"] = settings_.driftCameraLerpSpeed;
   return json;
}

nlohmann::json CameraGravityBridge::GetSharedParameters() const {
   return ExtractSharedParameters({"cameraIds"});
}

void CameraGravityBridge::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void CameraGravityBridge::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void CameraGravityBridge::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   if (data.contains("cameraIds") && data.at("cameraIds").is_array()) {
      std::vector<std::string> ids;
      bool valid = true;

      for (const auto& id : data.at("cameraIds")) {
         if (!id.is_string()) {
            valid = false;
            break;
         }
         ids.push_back(id.get<std::string>());
      }
      if (valid) {
         cameraIds_ = std::move(ids);
      }
   }

   if (data.contains("enableLandingShake")) {
      settings_.enableLandingShake = data["enableLandingShake"];
   }

   if (data.contains("landingShakeAmplitude")) {
      settings_.landingShakeAmplitude = data["landingShakeAmplitude"];
   }

   if (data.contains("landingShakeFrequency")) {
      settings_.landingShakeFrequency = data["landingShakeFrequency"];
   }

   if (data.contains("landingShakeDuration")) {
      settings_.landingShakeDuration = data["landingShakeDuration"];
   }

   if (data.contains("enableMiniTurboShake") && data.at("enableMiniTurboShake").is_boolean()) {
      settings_.enableMiniTurboShake = data.at("enableMiniTurboShake").get<bool>();
   }

   if (data.contains("miniTurboShakeHorizontalAmplitude") && data.at("miniTurboShakeHorizontalAmplitude").is_number()) {
      settings_.miniTurboShakeHorizontalAmplitude = data.at("miniTurboShakeHorizontalAmplitude").get<float>();
   }

   if (data.contains("miniTurboShakeVerticalAmplitude") && data.at("miniTurboShakeVerticalAmplitude").is_number()) {
      settings_.miniTurboShakeVerticalAmplitude = data.at("miniTurboShakeVerticalAmplitude").get<float>();
   }

   if (data.contains("miniTurboShakeFrequency") && data.at("miniTurboShakeFrequency").is_number()) {
      settings_.miniTurboShakeFrequency = data.at("miniTurboShakeFrequency").get<float>();
   }

   if (data.contains("miniTurboShakeDuration") && data.at("miniTurboShakeDuration").is_number()) {
      settings_.miniTurboShakeDuration = data.at("miniTurboShakeDuration").get<float>();
   }

   if (data.contains("enableDriftCameraApproach") && data.at("enableDriftCameraApproach").is_boolean()) {
      settings_.enableDriftCameraApproach = data.at("enableDriftCameraApproach").get<bool>();
   }

   if (data.contains("driftCameraDistanceReduction") && data.at("driftCameraDistanceReduction").is_number()) {
      settings_.driftCameraDistanceReduction = data.at("driftCameraDistanceReduction").get<float>();
   }

   if (data.contains("driftCameraHeightOffset") && data.at("driftCameraHeightOffset").is_number()) {
      settings_.driftCameraHeightOffset = data.at("driftCameraHeightOffset").get<float>();
   }

   if (data.contains("driftLookTargetHeightOffset") && data.at("driftLookTargetHeightOffset").is_number()) {
      settings_.driftLookTargetHeightOffset = data.at("driftLookTargetHeightOffset").get<float>();
   }

   if (data.contains("driftCameraLerpSpeed") && data.at("driftCameraLerpSpeed").is_number()) {
      settings_.driftCameraLerpSpeed = data.at("driftCameraLerpSpeed").get<float>();
   }

   Configure(DescribeSettings());
}

} // namespace App

namespace App {

CameraGravityBridge::Settings CameraGravityBridge::DescribeSettings() const {
   return settings_;
}

void CameraGravityBridge::Configure(const Settings& requested) {
   auto settings = requested;
   [[maybe_unused]] const Settings defaults;

   if (!std::isfinite(settings.landingShakeAmplitude)) {
      settings.landingShakeAmplitude = defaults.landingShakeAmplitude;
   }

   if (!std::isfinite(settings.landingShakeFrequency)) {
      settings.landingShakeFrequency = defaults.landingShakeFrequency;
   }

   if (!std::isfinite(settings.landingShakeDuration)) {
      settings.landingShakeDuration = defaults.landingShakeDuration;
   }

   if (!std::isfinite(settings.miniTurboShakeHorizontalAmplitude)) {
      settings.miniTurboShakeHorizontalAmplitude = defaults.miniTurboShakeHorizontalAmplitude;
   }

   if (!std::isfinite(settings.miniTurboShakeVerticalAmplitude)) {
      settings.miniTurboShakeVerticalAmplitude = defaults.miniTurboShakeVerticalAmplitude;
   }

   if (!std::isfinite(settings.miniTurboShakeFrequency)) {
      settings.miniTurboShakeFrequency = defaults.miniTurboShakeFrequency;
   }

   if (!std::isfinite(settings.miniTurboShakeDuration)) {
      settings.miniTurboShakeDuration = defaults.miniTurboShakeDuration;
   }

   settings.miniTurboShakeHorizontalAmplitude = std::max(0.0f, settings.miniTurboShakeHorizontalAmplitude);
   settings.miniTurboShakeVerticalAmplitude = std::max(0.0f, settings.miniTurboShakeVerticalAmplitude);
   settings.miniTurboShakeFrequency = std::max(0.1f, settings.miniTurboShakeFrequency);
   settings.miniTurboShakeDuration = std::max(0.0f, settings.miniTurboShakeDuration);

   if (!std::isfinite(settings.driftCameraDistanceReduction)) {
      settings.driftCameraDistanceReduction = defaults.driftCameraDistanceReduction;
   }

   if (!std::isfinite(settings.driftCameraHeightOffset)) {
      settings.driftCameraHeightOffset = defaults.driftCameraHeightOffset;
   }

   if (!std::isfinite(settings.driftLookTargetHeightOffset)) {
      settings.driftLookTargetHeightOffset = defaults.driftLookTargetHeightOffset;
   }

   if (!std::isfinite(settings.driftCameraLerpSpeed)) {
      settings.driftCameraLerpSpeed = defaults.driftCameraLerpSpeed;
   }

   settings.driftCameraDistanceReduction = std::clamp(settings.driftCameraDistanceReduction, 0.0f, 0.5f);
   settings.driftCameraLerpSpeed = std::max(0.1f, settings.driftCameraLerpSpeed);

   settings.landingShakeAmplitude = std::max(0.0f, settings.landingShakeAmplitude);
   settings.landingShakeFrequency = std::max(0.1f, settings.landingShakeFrequency);
   settings.landingShakeDuration = std::max(0.0f, settings.landingShakeDuration);
   settings_ = settings;
}

} // namespace App
