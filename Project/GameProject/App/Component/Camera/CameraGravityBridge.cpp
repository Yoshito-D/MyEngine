#include "GameProject/App/Component/Camera/CameraGravityBridge.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include <cstdint>
#include "GameProject/App/Component/Camera/CameraModeSwitcher.h"
#include "GameProject/App/Component/Character/CharacterLanding.h"
#include "GameProject/App/Component/Character/CharacterJump.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"
#include "GameProject/App/Component/Vehicle/VehicleGroundMover.h"
#include "GameProject/App/Component/Vehicle/VehicleDrift.h"
#include "GameProject/App/Component/Vehicle/VehicleInputComponent.h"
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
void CameraGravityBridge::OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) {
   gravityFollowCamera_ = nullptr;
   playerRearFollowCamera_ = nullptr;
   planetLeashCamera_ = nullptr;

   if (auto* camera = sceneWorld.FindVirtualCamera(gravityFollowCameraId_)) {
      gravityFollowCamera_ = camera->GetComponent<GravityFollowCamera>();
   }

   if (auto* camera = sceneWorld.FindVirtualCamera(playerRearFollowCameraId_)) {
      playerRearFollowCamera_ = camera->GetComponent<PlayerRearFollowCamera>();
   }

   if (auto* camera = sceneWorld.FindVirtualCamera(planetLeashCameraId_)) {
      planetLeashCamera_ = camera->GetComponent<PlanetLeashCamera>();
   }
}

void CameraGravityBridge::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   OnReferencesChanged(sceneWorld);
   const auto* drift = HasOwner() ? GetOwner().GetComponent<VehicleDrift>() : nullptr;
   effects_.Reset(drift ? drift->GetMiniTurboFireCount() : 0);
   presentation_.Reset();
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
   const bool rearCameraSelected = playerRearFollowCamera_ && playerRearFollowCamera_->IsEnabled() &&
                                   (!selectedCamera || playerRearFollowCamera_->GetOwnerCamera() == selectedCamera);
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

   // GravityFollowCamera 側へ重力Upと注視対象を同期
   if (gravityFollowCamera_) {
      gravityFollowCamera_->SetGravityUp(gravityUp);
      gravityFollowCamera_->SetPivotTarget(pos);
   }

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

      playerRearFollowCamera_->SetGravityUp(gravityUp);
      playerRearFollowCamera_->SetPivotTarget(pos);
      playerRearFollowCamera_->SetPlanetCenter(cameraPlanetCenter);
      playerRearFollowCamera_->SetFollowForward(forward);
      playerRearFollowCamera_->SetAirborneMoveForward(airborneMoveForward);
      playerRearFollowCamera_->SetAirborne(isAirborne);
      playerRearFollowCamera_->SetPlayerVelocity(playerVelocity);

      // 条件を満たさないフレームでは前回値を残さず、カメラ側の予測補間を確実に解除する。
      playerRearFollowCamera_->ClearLandingPrediction();
      LandingPrediction prediction{};
      bool hasLandingPrediction = false;

      if (isAirborne && hasCameraPlanet && gravityBody && landing) {
         float predictionHorizon = std::max(0.0f, playerRearFollowCamera_->GetPreLandingPredictionHorizon());
         hasLandingPrediction = PredictLanding(pos, playerVelocity, cameraPlanetCenter, cameraPlanetSurfaceRadius,
            landing->DescribeSettings().landingOffset, landing->DescribeSettings().obbHalfExtents, rotation,
            *gravityBody, predictionHorizon, forward, playerRearFollowCamera_->GetCameraRight(), prediction);

         if (hasLandingPrediction) {
            playerRearFollowCamera_->SetLandingPrediction(
               prediction.up, prediction.backward, prediction.contactPoint, prediction.secondsToImpact);
         }
      }

      if (rearCameraSelected) {
         presentation_.Update(settings_, deltaTime, *playerRearFollowCamera_, jump, landing && landing->IsGrounded(),
            hasLandingPrediction ? &prediction : nullptr);
      }
   }

   // VehicleGroundMover から速度と autoSpeed を取得し、両カメラへ供給する
   float speed = 0.0f;
   float autoSpeed = 13.0f;

   if (auto* mover = GetOwner().GetComponent<VehicleGroundMover>()) {
      speed = mover->GetCurrentSpeed();
      autoSpeed = mover->DescribeSettings().autoSpeed;
   }

   if (gravityFollowCamera_) {
      gravityFollowCamera_->SetPlayerSpeed(speed);
      gravityFollowCamera_->SetAutoSpeed(autoSpeed);
   }

   if (playerRearFollowCamera_) {
      playerRearFollowCamera_->SetPlayerSpeed(speed);
      playerRearFollowCamera_->SetAutoSpeed(autoSpeed);
   }

   // PlanetLeashCamera 側へ重力Up・注視対象・惑星中心を同期
   if (planetLeashCamera_) {
      planetLeashCamera_->SetGravityUp(gravityUp);
      planetLeashCamera_->SetPivotTarget(pos);
      planetLeashCamera_->SetSphereCenter(planetCenter_);
   }

   const bool grounded = (!landing || landing->IsGrounded()) && (!jump || !jump->IsJumping());
   const bool steering = grounded && drift && drift->IsEnabled() && input && input->GetSteerInput() != 0.0f;
   effects_.Update(settings_, deltaTime, steering, grounded, drift ? drift->GetMiniTurboFireCount() : 0, gravityUp,
      gravityFollowCamera_, playerRearFollowCamera_, planetLeashCamera_, selectedCamera);
}

#ifdef USE_IMGUI
void CameraGravityBridge::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   GameEngine::EditorUI::CameraReference("Gravity Follow Camera", gravityFollowCameraId_, "GravityFollowCamera");
   GameEngine::EditorUI::CameraReference(
      "Player Rear Follow Camera", playerRearFollowCameraId_, "PlayerRearFollowCamera");
   GameEngine::EditorUI::CameraReference("Planet Leash Camera", planetLeashCameraId_, "PlanetLeashCamera");
   ImGui::Separator();
   ImGui::Text(
      "%s: (%.2f, %.2f, %.2f)", Tr("惑星中心", "Planet Center"), planetCenter_.x, planetCenter_.y, planetCenter_.z);
   ImGui::Text("%s: %s", Tr("重力追従カメラ", "Gravity Follow Camera"),
      gravityFollowCamera_ ? Tr("設定済み", "Set") : Tr("なし", "None"));
   ImGui::Text("%s: %s", Tr("プレイヤー後方追従", "Player Rear Follow"),
      playerRearFollowCamera_ ? Tr("設定済み", "Set") : Tr("なし", "None"));
   ImGui::Text("%s: %s", Tr("惑星レアッシュカメラ", "Planet Leash Camera"),
      planetLeashCamera_ ? Tr("設定済み", "Set") : Tr("なし", "None"));
   ImGui::Separator();
   ImGui::Checkbox(Tr("着地シェイク", "Landing Shake"), &settings_.enableLandingShake);
   ImGui::DragFloat(Tr("シェイク振幅", "Shake Amplitude"), &settings_.landingShakeAmplitude, 0.01f, 0.0f, 5.0f);
   ImGui::DragFloat(Tr("シェイク周波数", "Shake Frequency"), &settings_.landingShakeFrequency, 0.1f, 0.1f, 60.0f);
   ImGui::DragFloat(Tr("シェイク時間", "Shake Duration"), &settings_.landingShakeDuration, 0.01f, 0.0f, 3.0f);
   ImGui::Separator();
   ImGui::Checkbox(Tr("ミニターボシェイク", "Mini Turbo Shake"), &settings_.enableMiniTurboShake);
   ImGui::DragFloat(Tr("ミニターボ横揺れ振幅", "Mini Turbo Horizontal Amplitude"),
      &settings_.miniTurboShakeHorizontalAmplitude, 0.01f, 0.0f, 5.0f);
   ImGui::DragFloat(Tr("ミニターボ縦揺れ振幅", "Mini Turbo Vertical Amplitude"),
      &settings_.miniTurboShakeVerticalAmplitude, 0.01f, 0.0f, 5.0f);
   ImGui::DragFloat(Tr("ミニターボシェイク周波数", "Mini Turbo Shake Frequency"), &settings_.miniTurboShakeFrequency,
      0.1f, 0.1f, 60.0f);
   ImGui::DragFloat(
      Tr("ミニターボシェイク時間", "Mini Turbo Shake Duration"), &settings_.miniTurboShakeDuration, 0.01f, 0.0f, 3.0f);
   ImGui::Separator();
   ImGui::Checkbox(
      Tr("横入力時にカメラを近づける", "Camera Approach on Steering"), &settings_.enableDriftCameraApproach);
   ImGui::DragFloat(Tr("ドリフト時の距離短縮率", "Drift Distance Reduction"), &settings_.driftCameraDistanceReduction,
      0.01f, 0.0f, 0.5f);
   ImGui::DragFloat(Tr("ドリフト時のカメラ高さ追加量", "Drift Camera Height Offset"),
      &settings_.driftCameraHeightOffset, 0.05f, -20.0f, 20.0f);
   ImGui::DragFloat(Tr("ドリフト時の注視点高さ追加量", "Drift Look Target Height Offset"),
      &settings_.driftLookTargetHeightOffset, 0.05f, -10.0f, 10.0f);
   ImGui::TextDisabled("%s", Tr("高さは通常時への加算量です（正: 上、負: 下）。",
                                "Height offsets are added to normal height (positive: up, negative: down)."));
   ImGui::DragFloat(
      Tr("ドリフトカメラの補間速度", "Drift Camera Lerp Speed"), &settings_.driftCameraLerpSpeed, 0.1f, 0.1f, 30.0f);
   ImGui::Separator();
   ImGui::Checkbox(Tr("発表用カメラガイド", "Presentation Camera Guides"), &settings_.debugDrawPresentationGuides);
   ImGui::DragFloat(
      Tr("ガイド矢印の長さ", "Guide Arrow Length"), &settings_.presentationGuideLength, 0.1f, 1.0f, 30.0f);
   ImGui::Checkbox(
      Tr("発表用連続撮影", "Auto-capture Presentation Sequence"), &settings_.autoCapturePresentationSequence);
   ImGui::DragFloat(
      Tr("自動ジャンプ待ち時間", "Auto-jump Delay"), &settings_.presentationCaptureJumpDelay, 0.05f, 0.0f, 10.0f);
   ImGui::Checkbox(
      Tr("発表動画用の連番PNG", "Presentation Video Frames"), &settings_.autoCapturePresentationVideoFrames);
   ImGui::DragFloat(
      Tr("動画フレームレート", "Video Frame Rate"), &settings_.presentationVideoFrameRate, 1.0f, 1.0f, 60.0f);
   ImGui::DragFloat(Tr("動画撮影開始待ち", "Video Capture Start Delay"), &settings_.presentationVideoCaptureStartDelay,
      0.05f, 0.0f, 30.0f);
   ImGui::DragFloat(
      Tr("動画撮影時間", "Video Capture Duration"), &settings_.presentationVideoCaptureDuration, 0.1f, 0.0f, 60.0f);
   Configure(DescribeSettings());
}
#endif

nlohmann::json CameraGravityBridge::Serialize() const {
   nlohmann::json json;
   json["gravityFollowCameraId"] = gravityFollowCameraId_;
   json["playerRearFollowCameraId"] = playerRearFollowCameraId_;
   json["planetLeashCameraId"] = planetLeashCameraId_;
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
   json["debugDrawPresentationGuides"] = settings_.debugDrawPresentationGuides;
   json["presentationGuideLength"] = settings_.presentationGuideLength;
   json["autoCapturePresentationSequence"] = settings_.autoCapturePresentationSequence;
   json["presentationCaptureJumpDelay"] = settings_.presentationCaptureJumpDelay;
   json["autoCapturePresentationVideoFrames"] = settings_.autoCapturePresentationVideoFrames;
   json["presentationVideoFrameRate"] = settings_.presentationVideoFrameRate;
   json["presentationVideoCaptureStartDelay"] = settings_.presentationVideoCaptureStartDelay;
   json["presentationVideoCaptureDuration"] = settings_.presentationVideoCaptureDuration;
   return json;
}

nlohmann::json CameraGravityBridge::GetSharedParameters() const {
   return ExtractSharedParameters({"gravityFollowCameraId", "planetLeashCameraId", "playerRearFollowCameraId"});
}

void CameraGravityBridge::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void CameraGravityBridge::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void CameraGravityBridge::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   if (data.contains("gravityFollowCameraId") && data.at("gravityFollowCameraId").is_string()) {
      gravityFollowCameraId_ = data.at("gravityFollowCameraId").get<std::string>();
   }

   if (data.contains("playerRearFollowCameraId") && data.at("playerRearFollowCameraId").is_string()) {
      playerRearFollowCameraId_ = data.at("playerRearFollowCameraId").get<std::string>();
   }

   if (data.contains("planetLeashCameraId") && data.at("planetLeashCameraId").is_string()) {
      planetLeashCameraId_ = data.at("planetLeashCameraId").get<std::string>();
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

   if (data.contains("debugDrawPresentationGuides") && data.at("debugDrawPresentationGuides").is_boolean()) {
      settings_.debugDrawPresentationGuides = data.at("debugDrawPresentationGuides").get<bool>();
   }

   if (data.contains("presentationGuideLength") && data.at("presentationGuideLength").is_number()) {
      settings_.presentationGuideLength = std::max(1.0f, data.at("presentationGuideLength").get<float>());
   }

   if (data.contains("autoCapturePresentationSequence") && data.at("autoCapturePresentationSequence").is_boolean()) {
      settings_.autoCapturePresentationSequence = data.at("autoCapturePresentationSequence").get<bool>();
   }

   if (data.contains("presentationCaptureJumpDelay") && data.at("presentationCaptureJumpDelay").is_number()) {
      settings_.presentationCaptureJumpDelay = std::max(0.0f, data.at("presentationCaptureJumpDelay").get<float>());
   }

   if (data.contains("autoCapturePresentationVideoFrames") &&
       data.at("autoCapturePresentationVideoFrames").is_boolean()) {
      settings_.autoCapturePresentationVideoFrames = data.at("autoCapturePresentationVideoFrames").get<bool>();
   }

   if (data.contains("presentationVideoFrameRate") && data.at("presentationVideoFrameRate").is_number()) {
      settings_.presentationVideoFrameRate =
         std::clamp(data.at("presentationVideoFrameRate").get<float>(), 1.0f, 60.0f);
   }

   if (data.contains("presentationVideoCaptureStartDelay") &&
       data.at("presentationVideoCaptureStartDelay").is_number()) {
      settings_.presentationVideoCaptureStartDelay =
         std::max(0.0f, data.at("presentationVideoCaptureStartDelay").get<float>());
   }

   if (data.contains("presentationVideoCaptureDuration") && data.at("presentationVideoCaptureDuration").is_number()) {
      settings_.presentationVideoCaptureDuration =
         std::max(0.0f, data.at("presentationVideoCaptureDuration").get<float>());
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

   if (!std::isfinite(settings.presentationGuideLength)) {
      settings.presentationGuideLength = defaults.presentationGuideLength;
   }

   if (!std::isfinite(settings.presentationCaptureJumpDelay)) {
      settings.presentationCaptureJumpDelay = defaults.presentationCaptureJumpDelay;
   }

   if (!std::isfinite(settings.presentationVideoFrameRate)) {
      settings.presentationVideoFrameRate = defaults.presentationVideoFrameRate;
   }

   if (!std::isfinite(settings.presentationVideoCaptureStartDelay)) {
      settings.presentationVideoCaptureStartDelay = defaults.presentationVideoCaptureStartDelay;
   }

   if (!std::isfinite(settings.presentationVideoCaptureDuration)) {
      settings.presentationVideoCaptureDuration = defaults.presentationVideoCaptureDuration;
   }

   settings.landingShakeAmplitude = std::max(0.0f, settings.landingShakeAmplitude);
   settings.landingShakeFrequency = std::max(0.1f, settings.landingShakeFrequency);
   settings.landingShakeDuration = std::max(0.0f, settings.landingShakeDuration);
   settings.presentationGuideLength = std::max(1.0f, settings.presentationGuideLength);
   settings.presentationCaptureJumpDelay = std::max(0.0f, settings.presentationCaptureJumpDelay);
   settings.presentationVideoFrameRate = std::clamp(settings.presentationVideoFrameRate, 1.0f, 60.0f);
   settings.presentationVideoCaptureStartDelay = std::max(0.0f, settings.presentationVideoCaptureStartDelay);
   settings.presentationVideoCaptureDuration = std::max(0.0f, settings.presentationVideoCaptureDuration);
   settings_ = settings;
}

} // namespace App
