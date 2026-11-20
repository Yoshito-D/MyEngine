#include "GameProject/App/Component/Camera/RearFollow/RearCameraDebugView.h"

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include "GameEngine/Math/Functions/MathConstants.h"
#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameProject/App/Component/Camera/RearFollow/RearCameraAimSolver.h"
#include "imgui.h"
#include <cstdint>

namespace App {

void RearCameraDebugView::DrawInspector(RearCameraSettings& settings, bool& enabled) {
   auto Tr = GameEngine::LocalizeEditorText;
   auto DrawHelp = [Tr](const char* japanese, const char* english) {
      ImGui::SameLine();
      ImGui::TextDisabled("(?)");

      if (ImGui::IsItemHovered()) {
         ImGui::SetTooltip("%s", Tr(japanese, english));
      }
   };

   ImGui::Checkbox(Tr("有効", "Enabled"), &enabled);

   ImGui::TextDisabled(
      "%s", Tr("(?) にカーソルを合わせると役割を表示します。補間速度は大きいほど速く、秒数は大きいほど遅く変化します。",
               "Hover (?) for details. Larger blend speeds react faster; larger durations change more slowly."));

   if (ImGui::CollapsingHeader(Tr("基本構図", "Base Framing"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat(Tr("基準後方距離 (m)", "Base Rear Distance (m)"), &settings.distance, 0.1f, 1.0f, 100.0f);
      DrawHelp("通常時にプレイヤーからカメラまで離す後方距離です。空中距離や速度演出の加算前の基準値です。",
         "Base settings.distance behind the player before airborne and speed-effect offsets are added.");
      ImGui::DragFloat(Tr("カメラ高さ (m)", "Camera Height (m)"), &settings.height, 0.1f, -20.0f, 50.0f);
      DrawHelp("カメラ位置を現在のUp方向へずらす量です。注視点の高さではありません。",
         "Moves the camera position along the current Up direction; it does not move the look target.");
      ImGui::DragFloat(Tr("地上注視点高さ (m)", "Grounded Look Target Height (m)"), &settings.groundedTargetHeight,
         0.05f, -5.0f, 10.0f);
      DrawHelp("地上でカメラが見る点をプレイヤー中心から上へずらします。値を上げるとプレイヤーが画面下側に寄ります。",
         "Raises the grounded look target above the player center, placing the player lower in the frame.");
      ImGui::DragFloat(Tr("離陸構図切替秒", "Takeoff Framing Duration"), &settings.takeoffFramingBlendSeconds, 0.01f,
         0.0f, 5.0f, "%.2f");
      DrawHelp("離陸時に、地上の画面位置から空中の中央構図へ切り替える時間です。距離やFOVには影響しません。",
         "Time to move from grounded framing to centered airborne framing; it does not affect settings.distance or FOV.");
      ImGui::DragFloat(Tr("着地構図切替秒", "Landing Framing Duration"), &settings.landingFramingBlendSeconds, 0.01f,
         0.0f, 5.0f, "%.2f");
      DrawHelp("着地時に、空中の中央構図から地上の画面位置へ戻す時間です。",
         "Time to return from centered airborne framing to grounded framing.");
   }

   if (ImGui::CollapsingHeader(Tr("空中の見え方", "Airborne Presentation"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat(Tr("空中時の追加距離 (m)", "Airborne Extra Distance (m)"), &settings.airborneDistanceOffset,
         0.1f, -30.0f, 30.0f);
      DrawHelp("空中で基準後方距離へ加える量です。周囲を広く見せます。",
         "Extra rear settings.distance added while airborne to show more surroundings.");
      GameEngine::ImGuiHelper::DragRadiansAsDegrees(Tr("空中時の追加FOV (deg)", "Airborne Extra FOV (deg)"),
         settings.airborneFovOffset, 0.001f * GameEngine::MathConstants::kRadiansToDegrees, 0.0f,
         0.5f * GameEngine::MathConstants::kRadiansToDegrees);
      DrawHelp("空中で通常FOVへ加える量です。速度によるFOV演出とは別です。",
         "FOV added while airborne, independently of speed-based FOV effects.");
      ImGui::DragFloat(Tr("空中表示切替速度 (1/s)", "Airborne State Blend Speed (1/s)"),
         &settings.airborneBlendLerpSpeed, 0.1f, 0.1f, 30.0f);
      DrawHelp(
         "空中距離・空中FOV・惑星ガイドの有効量を地上と空中の間で切り替える速度です。画面内のプレイヤー位置は構図切替秒で調整します。",
         "Blends airborne settings.distance, FOV, and planet-guide strength. Player screen placement uses the framing durations instead.");
   }

   if (ImGui::CollapsingHeader(Tr("着地前カメラ", "Pre-landing Camera"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox(Tr("着地前補間を使う", "Enable Pre-landing Blend"), &settings.enablePreLandingCamera);
      DrawHelp("現在軌道が地表へ到達すると予測できたとき、接触前から着地Up・着地後方・地形注視へ移行します。",
         "When the current trajectory is predicted to reach the surface, prepares landing Up, rear direction, and terrain framing before contact.");
      ImGui::DragFloat(Tr("着地予測開始秒", "Prediction Lead Time"), &settings.preLandingPredictionSeconds, 0.05f, 0.0f,
         5.0f, "%.2f");
      DrawHelp("予測接触までの残り時間がこの値を下回ると、着地方向への補間を開始します。",
         "Starts blending toward the landing frame when predicted time to contact falls below this value.");
      ImGui::DragFloat(Tr("着地補間完了秒", "Full Blend Before Impact"), &settings.preLandingFullBlendSeconds, 0.01f,
         0.0f, 1.0f, "%.2f");
      DrawHelp("接触の何秒前までに着地方向への補間を完了させるかを指定します。予測開始秒より小さくしてください。",
         "How long before contact the landing blend should be complete; keep it below the prediction lead time.");
      ImGui::DragFloat(Tr("着地補間の追従速度 (1/s)", "Pre-landing Blend Follow (1/s)"),
         &settings.preLandingBlendLerpSpeed, 0.1f, 0.0f, 30.0f);
      DrawHelp(
         "残り時間から求めた補間量へ実際の着地前ブレンドが追従する速度です。小さくすると切り替わりがより穏やかになります。",
         "How quickly the actual pre-landing blend follows its time-based target; lower values make the transition more gradual.");
      ImGui::DragFloat(Tr("着地補間解除速度 (1/s)", "Pre-landing Release Speed (1/s)"),
         &settings.preLandingReleaseLerpSpeed, 0.1f, 0.0f, 30.0f);
      DrawHelp("予測が外れた場合と着地後に、保持していた着地前補間を解除する速度です。",
         "How quickly the retained pre-landing blend is released after a missed prediction or actual landing.");
      ImGui::DragFloat(Tr("地形注視の先読み距離 (m)", "Terrain Look-ahead Distance (m)"),
         &settings.preLandingTerrainLookAhead, 0.1f, 0.0f, 30.0f);
      DrawHelp("予測接触地点から着地後の進行方向へ、注視候補を先読みする距離です。",
         "Distance ahead of the predicted contact point used as the terrain look target.");
      ImGui::DragFloat(Tr("地形注視の最大補正率", "Terrain Look Blend"), &settings.preLandingTerrainLookBlend, 0.01f,
         0.0f, 1.0f, "%.2f");
      DrawHelp("プレイヤー注視点から予測地形注視点へ寄せる最大割合です。",
         "Maximum blend from the player framing target toward the predicted terrain target.");
      ImGui::DragFloat(Tr("地表外側の最低高さ (m)", "Minimum Outward Height (m)"), &settings.preLandingMinOutwardHeight,
         0.1f, 0.0f, 20.0f);
      DrawHelp("着地前にカメラをプレイヤーと惑星の間へ入れないため、予測地表法線方向へ確保する最低高さです。",
         "Minimum offset along the predicted surface normal that keeps the camera from moving between the player and planet.");
   }

   if (ImGui::CollapsingHeader(Tr("空中の惑星方向ガイド", "Airborne Planet Guide"))) {
      ImGui::Checkbox(
         Tr("惑星方向ガイドを使う", "Enable Planet Direction Guide"), &settings.enableAirbornePlanetDirectionGuide);
      DrawHelp(
         "空中でカメラ位置を近傍惑星と反対側へ少し回り込ませ、プレイヤーの奥に惑星を映しやすくします。注視点はプレイヤーのままです。",
         "Orbits the camera slightly away from the nearby planet so the planet is easier to see behind the player; the player remains the look target.");
      ImGui::DragFloat(Tr("惑星方向への最大補正率", "Maximum Planet Correction"),
         &settings.airbornePlanetDirectionBlend, 0.01f, 0.0f, 1.0f, "%.2f");
      DrawHelp("速度後方から惑星を映す方向へ寄せる上限です。0で補正なし、1でガイド方向まで寄せます。",
         "Maximum blend from velocity-rear direction toward the planet-guide direction: 0 disables correction, 1 allows full correction.");
      GameEngine::ImGuiHelper::DragRadiansAsDegrees(
         Tr("惑星ガイド追従速度 (deg/s)", "Planet Guide Follow Speed (deg/s)"),
         settings.airbornePlanetDirectionLerpSpeed, 0.1f * GameEngine::MathConstants::kRadiansToDegrees, 0.0f,
         30.0f * GameEngine::MathConstants::kRadiansToDegrees);
      DrawHelp(
         "惑星ガイドの方向と、重力方向による有効係数が変化へ追従する通常時の速度です。離陸後は停止・復帰設定に従って0からこの値へ戻ります。",
         "Normal follow speed for the guide direction and its gravity-gated strength. After takeoff, delay and restore settings bring speed back from zero to this value.");
      ImGui::DragFloat(Tr("離陸後ガイド停止秒", "Post-takeoff Guide Delay"), &settings.jumpPlanetDirectionDelaySeconds,
         0.01f, 0.0f, 5.0f, "%.2f");
      DrawHelp("離陸直後に惑星ガイド追従速度を0へ保つ時間です。この間、惑星ガイドはカメラ方向を動かしません。",
         "Time after takeoff during which planet-guide follow speed remains zero and cannot move the camera direction.");
      ImGui::DragFloat(Tr("惑星ガイド速度復帰秒", "Planet Guide Speed Restore"),
         &settings.jumpPlanetDirectionRestoreSeconds, 0.01f, 0.0f, 5.0f, "%.2f");
      DrawHelp("停止時間が終わってから、惑星ガイド追従速度を0から設定値まで滑らかに戻す時間です。0なら即座に戻ります。",
         "Time used after the delay to restore planet-guide follow speed smoothly from zero to its configured value; zero restores immediately.");
      ImGui::Checkbox(Tr("落下方向のときだけ使う", "Gate Guide by Falling Direction"),
         &settings.enableAirborneGravityDirectionBoost);
      DrawHelp("有効時は、速度が惑星中心方向に近いときだけ惑星ガイドを効かせます。無効時は空中で常に候補になります。",
         "When enabled, the guide acts only while velocity points toward the planet center; otherwise it is always eligible in air.");
      ImGui::DragFloat(Tr("ガイド開始方向一致度", "Guide Start Alignment"),
         &settings.airborneGravityDirectionBoostThreshold, 0.01f, 0.0f, 1.0f, "%.2f");
      DrawHelp("速度方向と惑星中心方向の内積がこの値を超えると、ガイドが効き始めます。0は直交、1は完全に同方向です。",
         "The guide starts when velocity alignment with the planet-center direction exceeds this dot-product value; 0 is perpendicular and 1 is identical.");
      ImGui::DragFloat(Tr("ガイド最大方向一致度", "Guide Full Alignment"),
         &settings.airborneGravityDirectionBoostFullThreshold, 0.01f, 0.0f, 1.0f, "%.2f");
      DrawHelp("方向一致度がこの値に達すると、重力方向による係数が最大になります。開始値より大きくしてください。",
         "Alignment at which the gravity-gated factor reaches full strength; set it above the start value.");
      ImGui::DragFloat(Tr("ガイド強度カーブ", "Guide Strength Curve"), &settings.airborneGravityDirectionBoostBias,
         0.05f, 0.01f, 5.0f, "%.2f");
      DrawHelp("開始から最大までの効き方です。1が標準、1より大きいと後半で強まり、1より小さいと早めに強まります。",
         "Shapes the transition from start to full: 1 is neutral, above 1 delays strength, below 1 brings it in earlier.");
   }

   if (ImGui::CollapsingHeader(Tr("向きの追従", "Direction Tracking"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat(Tr("空中速度後方の追従速度 (1/s)", "Airborne Velocity-rear Follow (1/s)"),
         &settings.airborneForwardLerpSpeed, 0.1f, 0.0f, 30.0f);
      DrawHelp("空中でカメラの理想後方を、プレイヤー速度の反対方向へ回す速さです。カメラ位置全体の追従とは別です。",
         "How quickly the ideal rear direction turns opposite the player's velocity; separate from final camera-position smoothing.");
      ImGui::DragFloat(
         Tr("地上後方の追従速度 (1/s)", "Grounded Rear Follow (1/s)"), &settings.rearLerpSpeed, 0.1f, 0.0f, 30.0f);
      DrawHelp("地上でカメラの理想後方をプレイヤー正面の反対へ回す速さです。",
         "How quickly the ideal rear direction turns behind the player's facing direction on the ground.");
      ImGui::DragFloat(Tr("着地後方速度の切替秒", "Landing Rear-speed Transition"),
         &settings.landingRearLerpRampSeconds, 0.01f, 0.0f, 5.0f, "%.2f");
      DrawHelp("着地直前の空中後方追従速度から、地上後方追従速度へ切り替える時間です。",
         "Time to transition from the airborne rear-follow speed to the grounded rear-follow speed after landing.");
      GameEngine::ImGuiHelper::DragRadiansAsDegrees(
         Tr("重力Upの最大追従角速度 (deg/s)", "Gravity Up Angular Speed (deg/s)"), settings.gravityUpLerpSpeed,
         0.1f * GameEngine::MathConstants::kRadiansToDegrees, 0.1f * GameEngine::MathConstants::kRadiansToDegrees,
         30.0f * GameEngine::MathConstants::kRadiansToDegrees);
      DrawHelp(
         "惑星切替などで重力Upが変わったとき、カメラ基準Upを追従させる最大角速度（deg/s）です。最終ロール補間とは別です。",
         "Maximum angular speed (deg/s) used to follow a changing gravity Up; separate from final roll smoothing.");
   }

   if (ImGui::CollapsingHeader(Tr("速度演出", "Speed Effects"), ImGuiTreeNodeFlags_DefaultOpen)) {
      GameEngine::ImGuiHelper::DragRadiansAsDegrees(Tr("通常FOV (deg)", "Base FOV (deg)"), settings.fovDefault,
         0.001f * GameEngine::MathConstants::kRadiansToDegrees, 0.1f * GameEngine::MathConstants::kRadiansToDegrees,
         1.5f * GameEngine::MathConstants::kRadiansToDegrees);
      DrawHelp("空中・速度演出を加える前の基準FOVです。編集した値は次のカメラ更新で直接反映します。",
         "Base FOV before airborne and speed effects; edits apply directly on the next camera update.");
      GameEngine::ImGuiHelper::DragRadiansAsDegrees(Tr("高速時の追加FOV最大 (deg)", "High-speed Extra FOV (deg)"),
         settings.fovBoostMax, 0.001f * GameEngine::MathConstants::kRadiansToDegrees, 0.0f,
         0.5f * GameEngine::MathConstants::kRadiansToDegrees);
      DrawHelp("速度しきい値を超えて走り続けている間に加えるFOVの最大量です。瞬間キックとは異なる持続演出です。",
         "Maximum sustained FOV added while speed remains high; unlike the momentary kick, this persists.");
      ImGui::DragFloat(
         Tr("FOV演出の追従速度 (1/s)", "FOV Effect Follow Speed (1/s)"), &settings.fovLerpSpeed, 0.1f, 0.1f, 20.0f);
      DrawHelp("空中・高速・瞬間キックによる追加FOVの追従速度です。通常FOVの編集には影響しません。",
         "How quickly added FOV follows airborne, high-speed, and kick effects; base FOV edits are applied directly.");
      ImGui::DragFloat(Tr("高速時の追加距離最大 (m)", "High-speed Extra Distance (m)"), &settings.distanceBoostMax,
         0.1f, 0.0f, 30.0f);
      DrawHelp("速度しきい値を超えて走り続けている間に追加する後方距離の最大量です。瞬間キックとは異なる持続演出です。",
         "Maximum sustained rear settings.distance added while speed remains high; separate from the momentary kick.");
      GameEngine::ImGuiHelper::DragRadiansAsDegrees(
         Tr("速度変化FOVキック最大 (deg)", "Speed-change FOV Kick Max (deg)"), settings.speedChangeFovKickMax,
         0.001f * GameEngine::MathConstants::kRadiansToDegrees, 0.0f,
         0.3f * GameEngine::MathConstants::kRadiansToDegrees);
      DrawHelp(
         "急加速・急減速した瞬間にばねへ与えるFOV変化の最大量です。旧『加速→FOVキック』と『ターボFOVキック』を統合した項目です。",
         "Maximum momentary FOV impulse on sudden acceleration or deceleration; replaces the former coefficient and turbo-kick pair.");
      ImGui::DragFloat(Tr("速度変化距離キック最大 (m)", "Speed-change Distance Kick Max (m)"),
         &settings.speedChangeDistanceKickMax, 0.05f, 0.0f, 8.0f);
      DrawHelp("急加速・急減速した瞬間にばねへ与える後方距離変化の最大量です。旧2項目を統合しています。",
         "Maximum momentary rear-settings.distance impulse on sudden acceleration or deceleration; replaces the former two controls.");
      ImGui::DragFloat(
         Tr("キックばね剛性 (1/s²)", "Kick Spring Stiffness (1/s²)"), &settings.springStiffness, 1.0f, 1.0f, 300.0f);
      DrawHelp("瞬間キックを元のFOV・距離へ引き戻すばねの強さです。大きいほど戻す力が強くなります。",
         "Restoring force that pulls momentary FOV and settings.distance kicks back to zero; larger values pull harder.");
      ImGui::DragFloat(
         Tr("キックばね減衰 (1/s)", "Kick Spring Damping (1/s)"), &settings.springDamping, 0.5f, 0.0f, 100.0f);
      DrawHelp("瞬間キックの揺れを減らす強さです。大きいほど振動が早く収まります。",
         "Damping applied to momentary kicks; larger values settle oscillation sooner.");
      ImGui::DragFloat(Tr("高速演出開始速度 (m/s)", "High-speed Effect Start (m/s)"), &settings.speedBoostThreshold,
         0.5f, 0.0f, 100.0f);
      DrawHelp("持続する高速FOV・距離演出が効き始めるプレイヤー速度です。",
         "Player speed at which sustained high-speed FOV and settings.distance effects begin.");
      ImGui::DragFloat(Tr("高速演出最大速度 (m/s)", "High-speed Effect Full Speed (m/s)"), &settings.speedBoostMax,
         0.5f, 0.0f, 200.0f);
      DrawHelp("持続する高速FOV・距離演出が最大になるプレイヤー速度です。開始速度より大きくしてください。",
         "Player speed at which sustained high-speed effects reach full strength; set it above the start speed.");
   }

   if (ImGui::CollapsingHeader(Tr("最終出力の安定化", "Final Output Stabilization"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat(Tr("最終カメラ位置の追従速度 (1/s)", "Final Camera Position Follow (1/s)"),
         &settings.positionLerpSpeed, 0.5f, 1.0f, 100.0f);
      DrawHelp("空中・速度演出を合成した最終カメラ距離を滑らかにします。",
         "Smooths final camera settings.distance after airborne and speed effects are composed.");
      GameEngine::ImGuiHelper::DragRadiansAsDegrees(
         Tr("eye方向の最大角速度 (deg/s)", "Eye Direction Max Angular Speed (deg/s)"),
         settings.eyeDirectionMaxAngularSpeed, 0.1f * GameEngine::MathConstants::kRadiansToDegrees, 0.0f,
         30.0f * GameEngine::MathConstants::kRadiansToDegrees);
      DrawHelp(
         "上流で決定済みのeye方向は通常そのまま使い、急変時だけ1秒あたりの旋回角を制限します。低いジャンプの短い状態往復を吸収します。",
         "Normally keeps the resolved eye direction unchanged and only caps its angular speed during abrupt changes, absorbing short-hop state reversals.");
      ImGui::DragFloat(
         Tr("最終ロールの追従速度 (1/s)", "Final Roll Follow (1/s)"), &settings.rotationLerpSpeed, 0.5f, 0.1f, 100.0f);
      DrawHelp(
         "カメラの視線をプレイヤーへ固定したまま、最終的なRight/Up（ロール）だけを滑らかにします。重力Up補間とは処理段階が異なります。",
         "Smooths only final Right/Up roll while keeping the view aimed at the player; it is downstream from gravity-Up tracking.");
      ImGui::DragFloat(Tr("横向きを戻す範囲", "Horizontal Recovery Range"), &settings.lookAtRecoveryRange, 0.005f,
         0.001f, 1.0f, "%.3f");
      DrawHelp(
         "横向きを決めにくい状態から抜けた後、通常の向きへ戻す範囲です。大きくすると、広い範囲を使ってゆっくり戻ります。",
         "Controls how widely the camera eases back to its normal horizontal direction.");
      ImGui::SliderFloat(Tr("戻り始め", "Recovery Start"), &settings.lookAtRecoveryCurveControl1, 0.0f, 1.0f, "%.2f");
      ImGui::SliderFloat(Tr("戻り終わり", "Recovery End"), &settings.lookAtRecoveryCurveControl2, 0.0f, 1.0f, "%.2f");
      float recoveryCurvePreview[65]{};

      for (std::int32_t index = 0; index < 65; ++index) {
         recoveryCurvePreview[index] =
            RearCameraAimSolver::EvaluateLookAtRecoveryCurve(settings, static_cast<float>(index) / 64.0f);
      }

      ImGui::PlotLines("##LookAtRecoveryCurve", recoveryCurvePreview, 65, 0,
         Tr("横向きが安定するほど、通常の向きへ戻す割合",
            "Return to the normal direction as horizontal stability recovers"),
         0.0f, 1.0f, ImVec2(-1.0f, 100.0f));
   }
}

} // namespace App

#endif
