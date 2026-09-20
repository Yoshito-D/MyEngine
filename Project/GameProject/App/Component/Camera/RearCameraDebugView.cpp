#include "RearCameraDebugView.h"
#include "PlayerRearFollowCamera.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "RearCameraMath.h"
#include "Framework/EngineContext.h"
#include <cstdio>
#ifdef USE_IMGUI
#include "imgui.h"
#include "Object/Component/IObjectComponent.h"
#endif

namespace App {
using namespace RearCameraMath;

void RearCameraDebugView::MutateCameraState(GameEngine::CameraState&, float) {
   auto* camera = GetRearCamera();
   if (!camera) return;
   const auto* recorder = owner_->GetComponent<RearCameraMeasurementRecorder>();
   if (!recorder) return;
   DrawCameraMeasurementOverlay(*camera, *recorder);
#ifdef USE_IMGUI
   if (const auto frame = camera->GetFrameView()) {
      DrawCameraEvidenceWindow(*camera, *frame, *owner_->GetComponent<RearCameraMeasurementRecorder>());
   }
#endif
}

void RearCameraDebugView::DrawCameraMeasurementOverlay(
   const RearCameraSettings& settings,
   const RearCameraMeasurementRecorder& recorder) {
   if (!settings.showCameraMeasurementOverlay || recorder.GetState().cameraMeasurementSamples.empty()) {
      return;
   }

   const CameraMeasurementSample& latest = recorder.GetState().cameraMeasurementSamples.back();
   GameEngine::TextStyle shadowStyle{};
   shadowStyle.fontId = "arial";
   shadowStyle.fontSize = 20;
   shadowStyle.color = { 0.0f, 0.0f, 0.0f, 0.92f };
   shadowStyle.sortingOrder = 910;
   GameEngine::TextStyle style = shadowStyle;
   style.color = recorder.GetState().cameraMeasurementActive
      ? GameEngine::Vector4{ 1.0f, 0.75f, 0.12f, 1.0f }
      : GameEngine::Vector4{ 0.55f, 1.0f, 0.65f, 1.0f };
   style.sortingOrder = 911;

   auto DrawLine = [&](const std::string& text, float y) {
      GameEngine::EngineContext::DrawUIText(text, { 26.0f, y + 2.0f }, shadowStyle);
      GameEngine::EngineContext::DrawUIText(text, { 24.0f, y }, style);
   };

   char text[192]{};
   std::snprintf(
      text,
      sizeof(text),
      "CAMERA MEASUREMENT %s  frames=%llu  time=%.2fs",
      recorder.GetState().cameraMeasurementActive ? "REC" : "STOP",
      static_cast<unsigned long long>(recorder.GetState().cameraMeasurementSamples.size()),
      recorder.GetState().cameraMeasurementElapsedSeconds);
   DrawLine(text, 176.0f);
   std::snprintf(
      text,
      sizeof(text),
      "Up direction deg/frame current=%.3f  max=%.3f  60FPS limit=%.3f",
      latest.gravityUpStepDegrees,
      recorder.GetState().cameraMeasurementMaxGravityUpStepDegrees,
      std::max(0.0f, settings.gravityUpLerpSpeed) * kRadiansToDegrees / 60.0f);
   DrawLine(text, 202.0f);
   std::snprintf(
      text,
      sizeof(text),
      "Horizontal deg/frame current=%.3f  max=%.3f  stability=%.3f",
      latest.cameraRightStepDegrees,
      recorder.GetState().cameraMeasurementMaxRightStepDegrees,
      latest.lookAtRecoveryBlend);
   DrawLine(text, 228.0f);
   std::snprintf(
      text,
      sizeof(text),
      "axis alignment error=%.7f  axis length error=%.7f  invalid=%llu",
      recorder.GetState().cameraMeasurementMaxOrthogonalityError,
      recorder.GetState().cameraMeasurementMaxLengthError,
      static_cast<unsigned long long>(recorder.GetState().cameraMeasurementInvalidCount));
   DrawLine(text, 254.0f);
}

#ifdef USE_IMGUI
void RearCameraDebugView::DrawCameraEvidenceWindow(
   RearCameraSettings& settings,
   const RearCameraFrameView& frame,
   RearCameraMeasurementRecorder& recorder) {
   if (!settings.showCameraEvidenceWindow) {
      return;
   }

   ImGui::SetNextWindowPos(ImVec2(350.0f, 12.0f), ImGuiCond_Always);
   ImGui::SetNextWindowSize(ImVec2(660.0f, 535.0f), ImGuiCond_Always);
   ImGui::SetNextWindowFocus();
   if (!ImGui::Begin("カメラの調整と計測", &settings.showCameraEvidenceWindow)) {
      ImGui::End();
      return;
   }

   ImGui::TextUnformatted("横向きを通常へ戻す形");
   ImGui::SliderFloat("戻り始め", &settings.lookAtRecoveryCurveControl1, 0.0f, 1.0f, "%.2f");
   ImGui::SliderFloat("戻り終わり", &settings.lookAtRecoveryCurveControl2, 0.0f, 1.0f, "%.2f");

   float recoveryCurvePreview[65]{};
   for (int index = 0; index < 65; ++index) {
      recoveryCurvePreview[index] = frame.aim.EvaluateLookAtRecoveryCurve(settings, 
         static_cast<float>(index) / 64.0f);
   }
   ImGui::PlotLines(
      "##PresentationRecoveryCurve",
      recoveryCurvePreview,
      65,
      0,
      "横向きが安定するほど、通常の向きへ戻す割合",
      0.0f,
      1.0f,
      ImVec2(-1.0f, 105.0f));

   ImGui::Separator();
   ImGui::TextUnformatted("ゲーム中の計測");
   if (ImGui::Button("現在の動きを計測し直す")) {
      recorder.StartCameraMeasurement("manual_current_settings", frame);
   }
   ImGui::SameLine();
   if (ImGui::Button("上方向を180度変える固定条件で確認")) {
      recorder.RunFixedGravityUpVerification(settings);
   }
   ImGui::Text(
      "状態: %s　記録: %lluフレーム　経過: %.2f秒",
      recorder.GetState().cameraMeasurementActive ? "計測中" : "停止",
      static_cast<unsigned long long>(recorder.GetState().cameraMeasurementSamples.size()),
      recorder.GetState().cameraMeasurementElapsedSeconds);
   ImGui::Text(
      "上方向の最大変化: %.3f度/フレーム　横向きの最大変化: %.3f度/フレーム",
      recorder.GetState().cameraMeasurementMaxGravityUpStepDegrees,
      recorder.GetState().cameraMeasurementMaxRightStepDegrees);
   ImGui::Text(
      "計算失敗: %llu件",
      static_cast<unsigned long long>(recorder.GetState().cameraMeasurementInvalidCount));

   if (!recorder.GetState().cameraMeasurementSamples.empty()) {
      const size_t historyCount = std::min<size_t>(recorder.GetState().cameraMeasurementSamples.size(), 360);
      const size_t historyStart = recorder.GetState().cameraMeasurementSamples.size() - historyCount;
      std::vector<float> upwardHistory(historyCount);
      std::vector<float> horizontalHistory(historyCount);
      for (size_t index = 0; index < historyCount; ++index) {
         const CameraMeasurementSample& sample = recorder.GetState().cameraMeasurementSamples[historyStart + index];
         upwardHistory[index] = sample.gravityUpStepDegrees;
         horizontalHistory[index] = sample.cameraRightStepDegrees;
      }

      char upwardOverlay[96]{};
      char horizontalOverlay[96]{};
      std::snprintf(
         upwardOverlay,
         sizeof(upwardOverlay),
         "上方向の変化　最大 %.3f度/フレーム",
         recorder.GetState().cameraMeasurementMaxGravityUpStepDegrees);
      std::snprintf(
         horizontalOverlay,
         sizeof(horizontalOverlay),
         "横向きの変化　最大 %.3f度/フレーム",
         recorder.GetState().cameraMeasurementMaxRightStepDegrees);

      ImGui::PlotLines(
         "##PresentationUpwardHistory",
         upwardHistory.data(),
         static_cast<int>(upwardHistory.size()),
         0,
         upwardOverlay,
         0.0f,
         std::max(1.0f, recorder.GetState().cameraMeasurementMaxGravityUpStepDegrees * 1.1f),
         ImVec2(-1.0f, 95.0f));
      ImGui::PlotLines(
         "##PresentationHorizontalHistory",
         horizontalHistory.data(),
         static_cast<int>(horizontalHistory.size()),
         0,
         horizontalOverlay,
         0.0f,
         std::max(1.0f, recorder.GetState().cameraMeasurementMaxRightStepDegrees * 1.1f),
         ImVec2(-1.0f, 95.0f));
   }

   ImGui::End();
}
#endif

#ifdef USE_IMGUI
void RearCameraDebugView::DrawInspector() {
   ImGui::PushID("DiagnosticsComponent");
   RearCameraComponent::DrawInspector();
   ImGui::PopID();
   auto* camera = owner_ ? owner_->GetComponent<PlayerRearFollowCamera>() : nullptr;
   auto* recorder = owner_ ? owner_->GetComponent<RearCameraMeasurementRecorder>() : nullptr;
   if (!camera || !recorder) return;
   if (const auto frame = camera->GetFrameView()) {
      bool enabled = camera->IsEnabled();
      DrawInspector(*camera, enabled, *frame, *recorder);
      camera->SetEnabled(enabled);
   }
}

void RearCameraDebugView::DrawInspector(
   RearCameraSettings& settings,
   bool& enabled,
   const RearCameraFrameView& frame,
   RearCameraMeasurementRecorder& recorder) {
   auto Tr = GameEngine::LocalizeEditorText;
   auto DrawHelp = [Tr](const char* japanese, const char* english) {
      ImGui::SameLine();
      ImGui::TextDisabled("(?)");
      if (ImGui::IsItemHovered()) {
         ImGui::SetTooltip("%s", Tr(japanese, english));
      }
   };

   if (ImGui::Checkbox(Tr("有効", "Enabled"), &enabled)) {}

   ImGui::TextDisabled("%s", Tr("(?) にカーソルを合わせると役割を表示します。補間速度は大きいほど速く、秒数は大きいほど遅く変化します。",
      "Hover (?) for details. Larger blend speeds react faster; larger durations change more slowly."));

   if (ImGui::CollapsingHeader(Tr("基本構図", "Base Framing"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat(Tr("基準後方距離", "Base Rear Distance"), &settings.distance, 0.1f, 1.0f, 100.0f);
      DrawHelp("通常時にプレイヤーからカメラまで離す後方距離です。空中距離や速度演出の加算前の基準値です。",
         "Base settings.distance behind the player before airborne and speed-effect offsets are added.");
      ImGui::DragFloat(Tr("カメラ高さ", "Camera Height"), &settings.height, 0.1f, -20.0f, 50.0f);
      DrawHelp("カメラ位置を現在のUp方向へずらす量です。注視点の高さではありません。",
         "Moves the camera position along the current Up direction; it does not move the look target.");
      ImGui::DragFloat(Tr("地上注視点高さ", "Grounded Look Target Height"), &settings.groundedTargetHeight, 0.05f, -5.0f, 10.0f);
      DrawHelp("地上でカメラが見る点をプレイヤー中心から上へずらします。値を上げるとプレイヤーが画面下側に寄ります。",
         "Raises the grounded look target above the player center, placing the player lower in the frame.");
      ImGui::DragFloat(Tr("離陸構図切替秒", "Takeoff Framing Duration"), &settings.takeoffFramingBlendSeconds, 0.01f, 0.0f, 5.0f, "%.2f");
      DrawHelp("離陸時に、地上の画面位置から空中の中央構図へ切り替える時間です。距離やFOVには影響しません。",
         "Time to move from grounded framing to centered airborne framing; it does not affect settings.distance or FOV.");
      ImGui::DragFloat(Tr("着地構図切替秒", "Landing Framing Duration"), &settings.landingFramingBlendSeconds, 0.01f, 0.0f, 5.0f, "%.2f");
      DrawHelp("着地時に、空中の中央構図から地上の画面位置へ戻す時間です。",
         "Time to return from centered airborne framing to grounded framing.");
   }

   if (ImGui::CollapsingHeader(Tr("空中の見え方", "Airborne Presentation"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat(Tr("空中時の追加距離", "Airborne Extra Distance"), &settings.airborneDistanceOffset, 0.1f, 0.0f, 30.0f);
      DrawHelp("空中で基準後方距離へ加える量です。周囲を広く見せます。",
         "Extra rear settings.distance added while airborne to show more surroundings.");
      ImGui::DragFloat(Tr("空中時の追加FOV", "Airborne Extra FOV"), &settings.airborneFovOffset, 0.001f, 0.0f, 0.5f, "%.3f");
      DrawHelp("空中で通常FOVへ加える量です。速度によるFOV演出とは別です。",
         "FOV added while airborne, independently of speed-based FOV effects.");
      ImGui::DragFloat(Tr("空中表示切替速度", "Airborne State Blend Speed"), &settings.airborneBlendLerpSpeed, 0.1f, 0.1f, 30.0f);
      DrawHelp("空中距離・空中FOV・惑星ガイドの有効量を地上と空中の間で切り替える速度です。画面内のプレイヤー位置は構図切替秒で調整します。",
         "Blends airborne settings.distance, FOV, and planet-guide strength. Player screen placement uses the framing durations instead.");
   }

   if (ImGui::CollapsingHeader(Tr("着地前カメラ", "Pre-landing Camera"), ImGuiTreeNodeFlags_DefaultOpen)) {
	  ImGui::Checkbox(Tr("着地前補間を使う", "Enable Pre-landing Blend"), &settings.enablePreLandingCamera);
	  DrawHelp("現在軌道が地表へ到達すると予測できたとき、接触前から着地Up・着地後方・地形注視へ移行します。",
		 "When the current trajectory is predicted to reach the surface, prepares landing Up, rear direction, and terrain framing before contact.");
	  ImGui::DragFloat(Tr("着地予測開始秒", "Prediction Lead Time"), &settings.preLandingPredictionSeconds, 0.05f, 0.0f, 5.0f, "%.2f");
	  DrawHelp("予測接触までの残り時間がこの値を下回ると、着地方向への補間を開始します。",
		 "Starts blending toward the landing frame when predicted time to contact falls below this value.");
	  ImGui::DragFloat(Tr("着地補間完了秒", "Full Blend Before Impact"), &settings.preLandingFullBlendSeconds, 0.01f, 0.0f, 1.0f, "%.2f");
	  DrawHelp("接触の何秒前までに着地方向への補間を完了させるかを指定します。予測開始秒より小さくしてください。",
		 "How long before contact the landing blend should be complete; keep it below the prediction lead time.");
	  ImGui::DragFloat(Tr("着地補間の追従速度", "Pre-landing Blend Follow"), &settings.preLandingBlendLerpSpeed, 0.1f, 0.0f, 30.0f);
	  DrawHelp("残り時間から求めた補間量へ実際の着地前ブレンドが追従する速度です。小さくすると切り替わりがより穏やかになります。",
		 "How quickly the actual pre-landing blend follows its time-based target; lower values make the transition more gradual.");
	  ImGui::DragFloat(Tr("着地補間解除速度", "Pre-landing Release Speed"), &settings.preLandingReleaseLerpSpeed, 0.1f, 0.0f, 30.0f);
	  DrawHelp("予測が外れた場合と着地後に、保持していた着地前補間を解除する速度です。",
		 "How quickly the retained pre-landing blend is released after a missed prediction or actual landing.");
	  ImGui::DragFloat(Tr("地形注視の先読み距離", "Terrain Look-ahead Distance"), &settings.preLandingTerrainLookAhead, 0.1f, 0.0f, 30.0f);
	  DrawHelp("予測接触地点から着地後の進行方向へ、注視候補を先読みする距離です。",
		 "Distance ahead of the predicted contact point used as the terrain look target.");
	  ImGui::DragFloat(Tr("地形注視の最大補正率", "Terrain Look Blend"), &settings.preLandingTerrainLookBlend, 0.01f, 0.0f, 1.0f, "%.2f");
	  DrawHelp("プレイヤー注視点から予測地形注視点へ寄せる最大割合です。",
		 "Maximum blend from the player framing target toward the predicted terrain target.");
	  ImGui::DragFloat(Tr("地表外側の最低高さ", "Minimum Outward Height"), &settings.preLandingMinOutwardHeight, 0.1f, 0.0f, 20.0f);
	  DrawHelp("着地前にカメラをプレイヤーと惑星の間へ入れないため、予測地表法線方向へ確保する最低高さです。",
		 "Minimum offset along the predicted surface normal that keeps the camera from moving between the player and planet.");
   }

   if (ImGui::CollapsingHeader(Tr("空中の惑星方向ガイド", "Airborne Planet Guide"))) {
      ImGui::Checkbox(Tr("惑星方向ガイドを使う", "Enable Planet Direction Guide"), &settings.enableAirbornePlanetDirectionGuide);
      DrawHelp("空中でカメラ位置を近傍惑星と反対側へ少し回り込ませ、プレイヤーの奥に惑星を映しやすくします。注視点はプレイヤーのままです。",
         "Orbits the camera slightly away from the nearby planet so the planet is easier to see behind the player; the player remains the look target.");
      ImGui::DragFloat(Tr("惑星方向への最大補正率", "Maximum Planet Correction"), &settings.airbornePlanetDirectionBlend, 0.01f, 0.0f, 1.0f, "%.2f");
      DrawHelp("速度後方から惑星を映す方向へ寄せる上限です。0で補正なし、1でガイド方向まで寄せます。",
         "Maximum blend from velocity-rear direction toward the planet-guide direction: 0 disables correction, 1 allows full correction.");
	  ImGui::DragFloat(Tr("惑星ガイド追従速度", "Planet Guide Follow Speed"), &settings.airbornePlanetDirectionLerpSpeed, 0.1f, 0.0f, 30.0f);
	  DrawHelp("惑星ガイドの方向と、重力方向による有効係数が変化へ追従する通常時の速度です。離陸後は停止・復帰設定に従って0からこの値へ戻ります。",
		 "Normal follow speed for the guide direction and its gravity-gated strength. After takeoff, delay and restore settings bring speed back from zero to this value.");
	  ImGui::DragFloat(Tr("離陸後ガイド停止秒", "Post-takeoff Guide Delay"), &settings.jumpPlanetDirectionDelaySeconds, 0.01f, 0.0f, 5.0f, "%.2f");
	  DrawHelp("離陸直後に惑星ガイド追従速度を0へ保つ時間です。この間、惑星ガイドはカメラ方向を動かしません。",
		 "Time after takeoff during which planet-guide follow speed remains zero and cannot move the camera direction.");
	  ImGui::DragFloat(Tr("惑星ガイド速度復帰秒", "Planet Guide Speed Restore"), &settings.jumpPlanetDirectionRestoreSeconds, 0.01f, 0.0f, 5.0f, "%.2f");
	  DrawHelp("停止時間が終わってから、惑星ガイド追従速度を0から設定値まで滑らかに戻す時間です。0なら即座に戻ります。",
		 "Time used after the delay to restore planet-guide follow speed smoothly from zero to its configured value; zero restores immediately.");
      ImGui::Checkbox(Tr("落下方向のときだけ使う", "Gate Guide by Falling Direction"), &settings.enableAirborneGravityDirectionBoost);
      DrawHelp("有効時は、速度が惑星中心方向に近いときだけ惑星ガイドを効かせます。無効時は空中で常に候補になります。",
         "When enabled, the guide acts only while velocity points toward the planet center; otherwise it is always eligible in air.");
      ImGui::DragFloat(Tr("ガイド開始方向一致度", "Guide Start Alignment"), &settings.airborneGravityDirectionBoostThreshold, 0.01f, 0.0f, 1.0f, "%.2f");
      DrawHelp("速度方向と惑星中心方向の内積がこの値を超えると、ガイドが効き始めます。0は直交、1は完全に同方向です。",
         "The guide starts when velocity alignment with the planet-center direction exceeds this dot-product value; 0 is perpendicular and 1 is identical.");
      ImGui::DragFloat(Tr("ガイド最大方向一致度", "Guide Full Alignment"), &settings.airborneGravityDirectionBoostFullThreshold, 0.01f, 0.0f, 1.0f, "%.2f");
      DrawHelp("方向一致度がこの値に達すると、重力方向による係数が最大になります。開始値より大きくしてください。",
         "Alignment at which the gravity-gated factor reaches full strength; set it above the start value.");
      ImGui::DragFloat(Tr("ガイド強度カーブ", "Guide Strength Curve"), &settings.airborneGravityDirectionBoostBias, 0.05f, 0.01f, 5.0f, "%.2f");
      DrawHelp("開始から最大までの効き方です。1が標準、1より大きいと後半で強まり、1より小さいと早めに強まります。",
         "Shapes the transition from start to full: 1 is neutral, above 1 delays strength, below 1 brings it in earlier.");
   }

   if (ImGui::CollapsingHeader(Tr("向きの追従", "Direction Tracking"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat(Tr("空中速度後方の追従速度", "Airborne Velocity-rear Follow"), &settings.airborneForwardLerpSpeed, 0.1f, 0.0f, 30.0f);
      DrawHelp("空中でカメラの理想後方を、プレイヤー速度の反対方向へ回す速さです。カメラ位置全体の追従とは別です。",
         "How quickly the ideal rear direction turns opposite the player's velocity; separate from final camera-position smoothing.");
      ImGui::DragFloat(Tr("地上後方の追従速度", "Grounded Rear Follow"), &settings.rearLerpSpeed, 0.1f, 0.0f, 30.0f);
      DrawHelp("地上でカメラの理想後方をプレイヤー正面の反対へ回す速さです。",
         "How quickly the ideal rear direction turns behind the player's facing direction on the ground.");
      ImGui::DragFloat(Tr("着地後方速度の切替秒", "Landing Rear-speed Transition"), &settings.landingRearLerpRampSeconds, 0.01f, 0.0f, 5.0f, "%.2f");
      DrawHelp("着地直前の空中後方追従速度から、地上後方追従速度へ切り替える時間です。",
         "Time to transition from the airborne rear-follow speed to the grounded rear-follow speed after landing.");
      ImGui::DragFloat(Tr("重力Upの最大追従角速度", "Gravity Up Angular Speed"), &settings.gravityUpLerpSpeed, 0.1f, 0.1f, 30.0f);
      DrawHelp("惑星切替などで重力Upが変わったとき、カメラ基準Upを追従させる最大角速度（rad/s）です。最終ロール補間とは別です。",
         "Maximum angular speed (rad/s) used to follow a changing gravity Up; separate from final roll smoothing.");
   }

   if (ImGui::CollapsingHeader(Tr("速度演出", "Speed Effects"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat(Tr("通常FOV", "Base FOV"), &settings.fovDefault, 0.001f, 0.1f, 1.5f, "%.3f");
      DrawHelp("空中・速度演出を加える前の基準FOVです。",
         "Base FOV before airborne and speed effects are added.");
      ImGui::DragFloat(Tr("高速時の追加FOV最大", "High-speed Extra FOV"), &settings.fovBoostMax, 0.001f, 0.0f, 0.5f, "%.3f");
      DrawHelp("速度しきい値を超えて走り続けている間に加えるFOVの最大量です。瞬間キックとは異なる持続演出です。",
         "Maximum sustained FOV added while speed remains high; unlike the momentary kick, this persists.");
      ImGui::DragFloat(Tr("FOV出力追従速度", "FOV Output Follow Speed"), &settings.fovLerpSpeed, 0.1f, 0.1f, 20.0f);
      DrawHelp("通常・空中・高速・瞬間キックを合成した目標FOVへ、最終FOVが追従する速度です。",
         "How quickly final FOV follows the target composed from base, airborne, high-speed, and kick effects.");
      ImGui::DragFloat(Tr("高速時の追加距離最大", "High-speed Extra Distance"), &settings.distanceBoostMax, 0.1f, 0.0f, 30.0f);
      DrawHelp("速度しきい値を超えて走り続けている間に追加する後方距離の最大量です。瞬間キックとは異なる持続演出です。",
         "Maximum sustained rear settings.distance added while speed remains high; separate from the momentary kick.");
      ImGui::DragFloat(Tr("速度変化FOVキック最大", "Speed-change FOV Kick Max"), &settings.speedChangeFovKickMax, 0.001f, 0.0f, 0.3f, "%.3f");
      DrawHelp("急加速・急減速した瞬間にばねへ与えるFOV変化の最大量です。旧『加速→FOVキック』と『ターボFOVキック』を統合した項目です。",
         "Maximum momentary FOV impulse on sudden acceleration or deceleration; replaces the former coefficient and turbo-kick pair.");
      ImGui::DragFloat(Tr("速度変化距離キック最大", "Speed-change Distance Kick Max"), &settings.speedChangeDistanceKickMax, 0.05f, 0.0f, 8.0f);
      DrawHelp("急加速・急減速した瞬間にばねへ与える後方距離変化の最大量です。旧2項目を統合しています。",
         "Maximum momentary rear-settings.distance impulse on sudden acceleration or deceleration; replaces the former two controls.");
      ImGui::DragFloat(Tr("キックばね剛性", "Kick Spring Stiffness"), &settings.springStiffness, 1.0f, 1.0f, 300.0f);
      DrawHelp("瞬間キックを元のFOV・距離へ引き戻すばねの強さです。大きいほど戻す力が強くなります。",
         "Restoring force that pulls momentary FOV and settings.distance kicks back to zero; larger values pull harder.");
      ImGui::DragFloat(Tr("キックばね減衰", "Kick Spring Damping"), &settings.springDamping, 0.5f, 0.0f, 100.0f);
      DrawHelp("瞬間キックの揺れを減らす強さです。大きいほど振動が早く収まります。",
         "Damping applied to momentary kicks; larger values settle oscillation sooner.");
      ImGui::DragFloat(Tr("高速演出開始速度", "High-speed Effect Start"), &settings.speedBoostThreshold, 0.5f, 0.0f, 100.0f);
      DrawHelp("持続する高速FOV・距離演出が効き始めるプレイヤー速度です。",
         "Player speed at which sustained high-speed FOV and settings.distance effects begin.");
      ImGui::DragFloat(Tr("高速演出最大速度", "High-speed Effect Full Speed"), &settings.speedBoostMax, 0.5f, 0.0f, 200.0f);
      DrawHelp("持続する高速FOV・距離演出が最大になるプレイヤー速度です。開始速度より大きくしてください。",
         "Player speed at which sustained high-speed effects reach full strength; set it above the start speed.");
   }

   if (ImGui::CollapsingHeader(Tr("最終出力の安定化", "Final Output Stabilization"), ImGuiTreeNodeFlags_DefaultOpen)) {
	  ImGui::DragFloat(Tr("最終カメラ位置の追従速度", "Final Camera Position Follow"), &settings.positionLerpSpeed, 0.5f, 1.0f, 100.0f);
	  DrawHelp("空中・速度演出を合成した最終カメラ距離を滑らかにします。",
		 "Smooths final camera settings.distance after airborne and speed effects are composed.");
	  ImGui::DragFloat(Tr("eye方向の最大角速度", "Eye Direction Max Angular Speed"), &settings.eyeDirectionMaxAngularSpeed, 0.1f, 0.0f, 30.0f);
	  DrawHelp("上流で決定済みのeye方向は通常そのまま使い、急変時だけ1秒あたりの旋回角を制限します。低いジャンプの短い状態往復を吸収します。",
		 "Normally keeps the resolved eye direction unchanged and only caps its angular speed during abrupt changes, absorbing short-hop state reversals.");
      ImGui::DragFloat(Tr("最終ロールの追従速度", "Final Roll Follow"), &settings.rotationLerpSpeed, 0.5f, 0.1f, 100.0f);
      DrawHelp("カメラの視線をプレイヤーへ固定したまま、最終的なRight/Up（ロール）だけを滑らかにします。重力Up補間とは処理段階が異なります。",
         "Smooths only final Right/Up roll while keeping the view aimed at the player; it is downstream from gravity-Up tracking.");
      ImGui::DragFloat(Tr("横向きを戻す範囲", "Horizontal Recovery Range"), &settings.lookAtRecoveryRange, 0.005f, 0.001f, 1.0f, "%.3f");
      DrawHelp("横向きを決めにくい状態から抜けた後、通常の向きへ戻す範囲です。大きくすると、広い範囲を使ってゆっくり戻ります。",
         "Controls how widely the camera eases back to its normal horizontal direction.");
      ImGui::SliderFloat(Tr("戻り始め", "Recovery Start"), &settings.lookAtRecoveryCurveControl1, 0.0f, 1.0f, "%.2f");
      ImGui::SliderFloat(Tr("戻り終わり", "Recovery End"), &settings.lookAtRecoveryCurveControl2, 0.0f, 1.0f, "%.2f");
      float recoveryCurvePreview[65]{};
      for (int index = 0; index < 65; ++index) {
         recoveryCurvePreview[index] = frame.aim.EvaluateLookAtRecoveryCurve(settings, 
            static_cast<float>(index) / 64.0f);
      }
      ImGui::PlotLines(
         "##LookAtRecoveryCurve",
         recoveryCurvePreview,
         65,
         0,
         Tr("横向きが安定するほど、通常の向きへ戻す割合",
            "Return to the normal direction as horizontal stability recovers"),
         0.0f,
         1.0f,
         ImVec2(-1.0f, 100.0f));
      ImGui::Text("%s: %.3f　%s: %.3f",
         Tr("現在の安定度", "Current Stability"),
         frame.aim.GetState().lastLookAtRecoveryInput,
         Tr("通常へ戻す割合", "Return Amount"),
         frame.aim.GetState().lastLookAtRecoveryBlend);
   }

   if (ImGui::CollapsingHeader(Tr("カメラ計測", "Camera Measurement"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox(Tr("ゲーム画面へ計測値を表示", "Show Measurement Overlay"), &settings.showCameraMeasurementOverlay);
      ImGui::Checkbox(Tr("調整と計測の別画面を表示", "Show Tuning and Measurement Window"), &settings.showCameraEvidenceWindow);
      if (ImGui::Button(Tr("計測開始", "Start Measurement"))) {
         recorder.StartCameraMeasurement("manual_current_settings", frame);
      }
      ImGui::SameLine();
      if (ImGui::Button(Tr("停止してCSV保存", "Stop and Save CSV"))) {
         recorder.StopCameraMeasurement(true, settings);
      }
      ImGui::SameLine();
      if (ImGui::Button(Tr("履歴を消去", "Clear History"))) {
         recorder.ClearCameraMeasurement();
      }
      if (ImGui::Button(Tr("60フレーム/秒で上方向を180度変える", "Run 60 FPS 180-degree Up Test"))) {
         recorder.RunFixedGravityUpVerification(settings);
      }
      DrawHelp("実ゲームと同じ向きの変え方を一定間隔で再現し、1フレームの上限と到達までのフレーム数を保存します。",
         "Repeats the in-game direction change at a fixed interval and saves the per-frame limit and arrival frame count.");

      ImGui::Text("%s: %s  %s: %llu  %s: %.3fs",
         Tr("状態", "State"),
         recorder.GetState().cameraMeasurementActive ? Tr("計測中", "Recording") : Tr("停止", "Stopped"),
         Tr("フレーム", "Frames"),
         static_cast<unsigned long long>(recorder.GetState().cameraMeasurementSamples.size()),
         Tr("経過", "Elapsed"),
         recorder.GetState().cameraMeasurementElapsedSeconds);
      ImGui::Text("%s %.3f度/フレーム　%s %.3f度/フレーム",
         Tr("上方向の最大変化", "Maximum Upward Change"),
         recorder.GetState().cameraMeasurementMaxGravityUpStepDegrees,
         Tr("横向きの最大変化", "Maximum Horizontal Change"),
         recorder.GetState().cameraMeasurementMaxRightStepDegrees);
      ImGui::Text("%s: %llu件",
         Tr("計算失敗", "Calculation Failures"),
         static_cast<unsigned long long>(recorder.GetState().cameraMeasurementInvalidCount));

      if (!recorder.GetState().cameraMeasurementSamples.empty()) {
         const size_t historyCount = std::min<size_t>(recorder.GetState().cameraMeasurementSamples.size(), 360);
         const size_t historyStart = recorder.GetState().cameraMeasurementSamples.size() - historyCount;
         std::vector<float> gravityUpHistory(historyCount);
         std::vector<float> rightHistory(historyCount);
         std::vector<float> targetErrorHistory(historyCount);
         for (size_t index = 0; index < historyCount; ++index) {
            const CameraMeasurementSample& sample = recorder.GetState().cameraMeasurementSamples[historyStart + index];
            gravityUpHistory[index] = sample.gravityUpStepDegrees;
            rightHistory[index] = sample.cameraRightStepDegrees;
            targetErrorHistory[index] = sample.targetGravityUpErrorDegrees;
         }
         char gravityOverlay[96]{};
         char rightOverlay[96]{};
         std::snprintf(
            gravityOverlay,
            sizeof(gravityOverlay),
            "上方向の変化　最大 %.3f度/フレーム",
            recorder.GetState().cameraMeasurementMaxGravityUpStepDegrees);
         std::snprintf(
            rightOverlay,
            sizeof(rightOverlay),
            "横向きの変化　最大 %.3f度/フレーム",
            recorder.GetState().cameraMeasurementMaxRightStepDegrees);
         ImGui::PlotLines(
            "##GravityUpStepHistory",
            gravityUpHistory.data(),
            static_cast<int>(gravityUpHistory.size()),
            0,
            gravityOverlay,
            0.0f,
            std::max(1.0f, recorder.GetState().cameraMeasurementMaxGravityUpStepDegrees * 1.1f),
            ImVec2(-1.0f, 90.0f));
         ImGui::PlotLines(
            "##RightStepHistory",
            rightHistory.data(),
            static_cast<int>(rightHistory.size()),
            0,
            rightOverlay,
            0.0f,
            std::max(1.0f, recorder.GetState().cameraMeasurementMaxRightStepDegrees * 1.1f),
            ImVec2(-1.0f, 90.0f));
         ImGui::PlotLines(
            "##GravityUpTargetErrorHistory",
            targetErrorHistory.data(),
            static_cast<int>(targetErrorHistory.size()),
            0,
            Tr("目標の上方向まで残っている角度", "Angle Remaining to the Target Up Direction"),
            0.0f,
            180.0f,
            ImVec2(-1.0f, 90.0f));
      }
      if (!recorder.GetState().lastCameraMeasurementPath.empty()) {
         ImGui::TextWrapped("%s: %s", Tr("保存先", "Saved To"), recorder.GetState().lastCameraMeasurementPath.c_str());
      }
   }

   ImGui::Separator();
   ImGui::Text("%s: %s", Tr("空中", "Airborne"), frame.input.isAirborne ? Tr("はい", "true") : Tr("いいえ", "false"));
   ImGui::Text("%s: %.2f", Tr("空中ブレンド", "Airborne Blend"), frame.transition.GetState().currentAirborneBlend);
   ImGui::Text("%s: %.2f", Tr("画面位置ブレンド", "Player Framing Blend"), frame.transition.GetState().currentPlayerFramingBlend);
   ImGui::Text("%s: %s", Tr("着地予測", "Landing Prediction"), frame.input.landingPredictionValid ? Tr("有効", "valid") : Tr("なし", "none"));
   ImGui::Text("%s: %.2f  %s: %.2f",
	  Tr("着地予測残り秒", "Predicted Impact Seconds"),
	  frame.input.predictedLandingSeconds,
	  Tr("着地前ブレンド", "Pre-landing Blend"),
	  frame.transition.GetState().currentPreLandingBlend);
   ImGui::Text("%s: %s  %s: %.2f",
	  Tr("地上復帰スナップショット", "Ground Release Snapshot"),
	  frame.transition.GetState().isLandingReleaseActive ? Tr("有効", "active") : Tr("なし", "none"),
	  Tr("適用率", "Guide Blend"),
	  frame.transition.ComputePreLandingGuideBlend(frame.input));
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("予測着地Up", "Predicted Landing Up"),
	  frame.input.predictedLandingUp.x, frame.input.predictedLandingUp.y, frame.input.predictedLandingUp.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("予測着地後方", "Predicted Landing Rear"),
	  frame.input.predictedLandingBackward.x, frame.input.predictedLandingBackward.y, frame.input.predictedLandingBackward.z);
	ImGui::Text("%s: %.2f", Tr("惑星補間重力係数", "Planet Blend Gravity Factor"), frame.planet.GetState().currentPlanetDirectionGravityFactor);
	ImGui::Text("%s: %.2f", Tr("現在の惑星ガイド追従速度", "Current Planet Guide Follow Speed"), frame.planet.ComputePlanetDirectionFollowSpeed(frame.input, settings));
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("目標GravityUp", "Target GravityUp"), frame.input.gravityUp.x, frame.input.gravityUp.y, frame.input.gravityUp.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("現在GravityUp", "Current GravityUp"), frame.gravity.GetState().currentGravityUp.x, frame.gravity.GetState().currentGravityUp.y, frame.gravity.GetState().currentGravityUp.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("追従前方向", "Follow Forward"), frame.input.followForward.x, frame.input.followForward.y, frame.input.followForward.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("プレイヤー速度ベクトル", "Player Velocity"), frame.input.playerVelocity.x, frame.input.playerVelocity.y, frame.input.playerVelocity.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("空中補助進行方向", "Air Move Fallback"), frame.input.airborneMoveForward.x, frame.input.airborneMoveForward.y, frame.input.airborneMoveForward.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("注視惑星中心", "Look Planet Center"), frame.input.planetCenter.x, frame.input.planetCenter.y, frame.input.planetCenter.z);
   ImGui::Text("%s: %.2f  %s: %.3f", Tr("プレイヤー速度", "Player Speed"), frame.input.playerSpeed, Tr("現在FOV", "Current FOV"), frame.speed.GetState().currentFov);
}
#endif

} // namespace App
