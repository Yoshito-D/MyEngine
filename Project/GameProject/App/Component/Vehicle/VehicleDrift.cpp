#include "VehicleDrift.h"
#include "VehicleGroundMover.h"
#include "../Gravity/GravityBody.h"
#include "Object/Object.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {

// ================================================================
// public
// ================================================================

void VehicleDrift::Apply(bool driftInput, float steerInput,
                         const Vector3& gravityUp, float deltaTime) {
   // ----------------------------------------------------------------
   // 1. ドリフト開始 / 継続 / 終了の状態管理
   // ----------------------------------------------------------------
   if (!isDrifting_) {
      // ドリフト中でない場合: 開始条件を毎フレーム判定する。
      // ShouldStartDrift() はモードに応じてボタン or ステア継続時間で判定する。
      if (ShouldStartDrift(driftInput, steerInput)) {
         isDrifting_ = true;
         driftTimer_ = 0.0f;
         // ドリフト方向を記憶する。継続判定とミニターボ判定に使う。
         driftSign_  = (steerInput >= 0.0f) ? 1.0f : -1.0f;

         // ドリフト開始時の横速度を現在の GravityBody の速度から初期化する。
         // 0 にリセットすると現在速度との差が急激な変化を起こし「カクッ」となるため、
         // 実際の横速度をそのまま引き継ぎ lateralBuildupRate で滑らかに目標値へ近づける。
         auto* gravBody   = GetOwner().GetComponent<GravityBody>();
         auto* gndMover   = GetOwner().GetComponent<VehicleGroundMover>();
         if (gravBody && gndMover) {
            Vector3 fwd              = gndMover->GetFlatForward();
            Vector3 right            = gravityUp.Cross(fwd).Normalize();
            slideLateralSpeed_       = gravBody->GetVelocity().Dot(right);
         } else {
            slideLateralSpeed_ = 0.0f;
         }
      }
   } else {
      // ドリフト中: 継続条件を満たさなければこのフレームで終了する。
      if (ShouldContinueDrift(driftInput, steerInput)) {
         // 継続中: ドリフトタイマーを加算する。
         // ミニターボ発動の最低継続時間 miniTurboMinTime の判定に使われる。
         driftTimer_ += deltaTime;
      } else {
         // 終了: ミニターボ発動を試みて状態をリセットする。
         // slideLateralSpeed_ はリセットしない。
         // これにより終了後も余韻として横速度が残り「タイヤがグリップを取り戻す」感が出る。
         TryFireMiniTurbo();
         isDrifting_     = false;
         driftTimer_     = 0.0f;
         sustainedTimer_ = 0.0f;
         prevSteerSign_  = 0.0f;
      }
   }

   // ----------------------------------------------------------------
   // 2. SustainedSteer モード用の継続タイマー更新
   //    ドリフト中でないときのみ蓄積する（ドリフト中は開始判定不要）。
   // ----------------------------------------------------------------
   if (!isDrifting_ && driftMode == DriftMode::SustainedSteer) {
      float steerSign = (steerInput >  driftSteerDeadZone) ?  1.0f :
                        (steerInput < -driftSteerDeadZone) ? -1.0f : 0.0f;

      if (steerSign != 0.0f && steerSign == prevSteerSign_) {
         // 同方向ステアが継続中 → タイマーを加算する。
         // sustainedSteerTime を超えた瞬間に ShouldStartDrift() が true を返す。
         sustainedTimer_ += deltaTime;
      } else {
         // 入力消失 or 反転 → タイマーリセット。
         // リセットしないと「少し入れて離す」を繰り返しても蓄積されてしまう。
         sustainedTimer_ = 0.0f;
         prevSteerSign_  = steerSign;
      }
   }

   // ----------------------------------------------------------------
   // 3. 横速度の適用（ドリフト中・終了後余韻どちらも処理）
   // ----------------------------------------------------------------
   if (isDrifting_) {
      // ドリフト中: steerInput の符号方向に横速度を蓄積しながら書き込む。
      ApplySlideVelocity(steerInput, gravityUp, deltaTime);
   } else if (std::abs(slideLateralSpeed_) > 0.01f) {
      // ドリフト終了後: 横速度が残っていれば減衰させながら書き込む。
      // これが「グリップを取り戻す」余韻に相当する。
      ApplyPostDriftBleed(gravityUp, deltaTime);
   } else {
      // 完全に停止したらゼロにスナップする。
      slideLateralSpeed_ = 0.0f;
   }
}

// ================================================================
// private  入力判定ヘルパー
// ================================================================

bool VehicleDrift::ShouldStartDrift(bool driftInput, float steerInput) const {
   // ステア入力がデッドゾーン以下なら直進中とみなしてドリフトしない。
   // 入力ゼロでドリフトが始まると予期せぬ横滑りが発生するため。
   if (std::abs(steerInput) < driftSteerDeadZone) { return false; }

   switch (driftMode) {
   case DriftMode::ButtonMode:
      // ボタンを押し続けている間だけ開始条件を満たす。
      return driftInput;

   case DriftMode::SustainedSteer:
      // sustainedTimer_ が sustainedSteerTime を超えたら条件を満たす。
      // sustainedTimer_ は同方向ステアが継続している間だけ加算されている（Apply 参照）。
      return (sustainedTimer_ >= sustainedSteerTime);
   }
   return false;
}

bool VehicleDrift::ShouldContinueDrift(bool driftInput, float steerInput) const {
   switch (driftMode) {
   case DriftMode::ButtonMode:
      // ボタンを離したら終了。
      return driftInput;

   case DriftMode::SustainedSteer:
      // ドリフト開始方向 (driftSign_) と現在の入力符号が一致していれば継続する。
      // 入力が消えるか反転したらドリフト終了。
      {
         float steerSign = (steerInput >  driftSteerDeadZone) ?  1.0f :
                           (steerInput < -driftSteerDeadZone) ? -1.0f : 0.0f;
         return (steerSign == driftSign_);
      }
   }
   return false;
}

// ================================================================
// private  物理ヘルパー
// ================================================================

void VehicleDrift::ApplySlideVelocity(float steerInput,
                                      const Vector3& gravityUp, float deltaTime) {
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   if (!gravityBody) { return; }
   auto* groundMover = GetOwner().GetComponent<VehicleGroundMover>();
   if (!groundMover) { return; }

   // ---- 水平方向ベクトルの取得 ----
   // flatForward は VehicleGroundMover が毎フレーム更新する水平前進方向。
   Vector3 flatForward = groundMover->GetFlatForward();
   // flatRight = gravityUp × flatForward（右手系で右方向）
   Vector3 flatRight   = gravityUp.Cross(flatForward).Normalize();

   float speed = groundMover->GetCurrentSpeed();

   // ---- 目標横速度の計算 ----
   // ドリフト中、車体はカーブ内側を向くが速度ベクトルは慣性で外側に流れる。
   // これがリアルなドリフトの「カーブが膨らむ」挙動の本質であるため、
   // 横速度の方向はドリフト方向（driftSign_）と逆向き（外側方向）にする。
   // 例: 右旋回（driftSign_=+1）のとき車体は右を向くが、速度は左（外側）へ流れる。
   float targetLateral = speed * slideRatio * (-driftSign_);

   // ---- 横速度を滑らかに目標値へ近づける（線形補間） ----
   // 瞬間的にジャンプさせると「カクッ」とした挙動になるため、
   // 毎フレーム lateralBuildupRate の速さで目標値へ lerp する。
   float maxStep = lateralBuildupRate * deltaTime;
   float diff    = targetLateral - slideLateralSpeed_;
   if (std::abs(diff) <= maxStep) {
      slideLateralSpeed_ = targetLateral;       // 十分近ければスナップ
   } else {
      slideLateralSpeed_ += std::copysign(maxStep, diff);  // 符号を保って step 移動
   }

   // ---- 速度ベクトルの合成 ----
   // 前方成分 : 速度の (1 - slideRatio) 分を前方に残す。
   // 横方向成分: slideLateralSpeed_ を flatRight 方向に加える。
   // 垂直成分  : 重力加速やジャンプの垂直速度を保持する（なければ落下が壊れる）。
   float   verticalSpeed = gravityBody->GetVelocity().Dot(gravityUp);
   Vector3 forwardComp   = flatForward * (speed * (1.0f - slideRatio));
   Vector3 slideComp     = flatRight   * slideLateralSpeed_;
   Vector3 verticalComp  = gravityUp   * verticalSpeed;

   gravityBody->SetVelocity(forwardComp + slideComp + verticalComp);
}

void VehicleDrift::ApplyPostDriftBleed(const Vector3& gravityUp, float deltaTime) {
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   if (!gravityBody) { return; }
   auto* groundMover = GetOwner().GetComponent<VehicleGroundMover>();
   if (!groundMover) { return; }

   Vector3 flatForward = groundMover->GetFlatForward();
   Vector3 flatRight   = gravityUp.Cross(flatForward).Normalize();

   // 横速度を指数減衰で 0 へ戻す。
   // exp(-rate * dt) は時定数 1/rate の厳密解で dt に対して無条件安定。
   // postDriftBleedRate が大きいほど素早くグリップが戻る。
   slideLateralSpeed_ *= std::exp(-postDriftBleedRate * deltaTime);
   if (std::abs(slideLateralSpeed_) < 0.01f) { slideLateralSpeed_ = 0.0f; }

   float   speed         = groundMover->GetCurrentSpeed();
   float   verticalSpeed = gravityBody->GetVelocity().Dot(gravityUp);
   Vector3 forwardComp   = flatForward * speed;
   Vector3 slideComp     = flatRight   * slideLateralSpeed_;
   Vector3 verticalComp  = gravityUp   * verticalSpeed;

   gravityBody->SetVelocity(forwardComp + slideComp + verticalComp);
}

void VehicleDrift::TryFireMiniTurbo() {
   // miniTurboEnabled = false のときはブーストしない。
   if (!miniTurboEnabled) { return; }

   // 継続時間が miniTurboMinTime に満たない場合もブーストしない。
   // 短時間ドリフトで速度が上がるのを防ぐための閾値。
   if (driftTimer_ < miniTurboMinTime) { return; }

   auto* groundMover = GetOwner().GetComponent<VehicleGroundMover>();
   if (!groundMover) { return; }

   // autoSpeed を基準として miniTurboBoost 分を上乗せする。
   // 基準値からの加算にすることで毎回同じ量のブーストが得られる。
   groundMover->SetCurrentSpeed(groundMover->autoSpeed + miniTurboBoost);
}

// ================================================================
// ImGui / Serialize
// ================================================================

#ifdef USE_IMGUI
void VehicleDrift::DrawInspector() {
   if (!ImGui::CollapsingHeader("VehicleDrift")) { return; }
   ImGui::Separator();

   const char* modeNames[] = { "ButtonMode", "SustainedSteer" };
   int modeIdx = static_cast<int>(driftMode);
   if (ImGui::Combo("Drift Mode", &modeIdx, modeNames, 2)) {
      driftMode = static_cast<DriftMode>(modeIdx);
   }
   ImGui::DragFloat("Slide Ratio",           &slideRatio,          0.01f, 0.0f,   1.0f);
   ImGui::DragFloat("Lateral Buildup Rate",  &lateralBuildupRate,  0.1f,  0.5f,  20.0f);
   ImGui::DragFloat("Post Drift Bleed Rate", &postDriftBleedRate,  0.1f,  0.5f,  20.0f);
   ImGui::DragFloat("Drift Steer Mult",      &driftSteerMult,      0.05f, 0.5f,   5.0f);
   ImGui::Checkbox ("Mini Turbo Enabled",    &miniTurboEnabled);
   if (miniTurboEnabled) {
      ImGui::DragFloat("Mini Turbo Boost",    &miniTurboBoost,     0.1f,  0.0f, 200.0f);
      ImGui::DragFloat("Mini Turbo Min Time", &miniTurboMinTime,   0.05f, 0.0f,   5.0f);
   }
   ImGui::DragFloat("Sustained Steer Time",  &sustainedSteerTime,  0.05f, 0.1f,   3.0f);
   ImGui::DragFloat("Drift Steer DeadZone",  &driftSteerDeadZone,  0.01f, 0.0f,   1.0f);
   ImGui::Spacing();
   ImGui::Text("Drifting: %s  DriftTimer: %.2f", isDrifting_ ? "yes" : "no", driftTimer_);
   ImGui::Text("SustainedTimer: %.2f  LateralSpeed: %.2f", sustainedTimer_, slideLateralSpeed_);
}
#endif

nlohmann::json VehicleDrift::Serialize() const {
   nlohmann::json json;
   json["driftMode"]          = static_cast<int>(driftMode);
   json["slideRatio"]         = slideRatio;
   json["lateralBuildupRate"] = lateralBuildupRate;
   json["postDriftBleedRate"] = postDriftBleedRate;
   json["driftSteerMult"]     = driftSteerMult;
   json["miniTurboEnabled"]   = miniTurboEnabled;
   json["miniTurboBoost"]     = miniTurboBoost;
   json["miniTurboMinTime"]   = miniTurboMinTime;
   json["sustainedSteerTime"] = sustainedSteerTime;
   json["driftSteerDeadZone"] = driftSteerDeadZone;
   return json;
}

void VehicleDrift::Deserialize(const nlohmann::json& data) {
   if (data.contains("driftMode"))          { driftMode          = static_cast<DriftMode>(data["driftMode"].get<int>()); }
   if (data.contains("slideRatio"))         { slideRatio         = data["slideRatio"]; }
   if (data.contains("lateralBuildupRate")) { lateralBuildupRate = data["lateralBuildupRate"]; }
   if (data.contains("postDriftBleedRate")) { postDriftBleedRate = data["postDriftBleedRate"]; }
   if (data.contains("driftSteerMult"))     { driftSteerMult     = data["driftSteerMult"]; }
   if (data.contains("miniTurboEnabled"))   { miniTurboEnabled   = data["miniTurboEnabled"]; }
   if (data.contains("miniTurboBoost"))     { miniTurboBoost     = data["miniTurboBoost"]; }
   if (data.contains("miniTurboMinTime"))   { miniTurboMinTime   = data["miniTurboMinTime"]; }
   if (data.contains("sustainedSteerTime")) { sustainedSteerTime = data["sustainedSteerTime"]; }
   if (data.contains("driftSteerDeadZone")) { driftSteerDeadZone = data["driftSteerDeadZone"]; }
}

} // namespace App