#include "pch.h"
#include "TPSCameraController.h"
#include "../GameObject/Player/Player.h"
#include "../GameObject/Planet/Planet.h"
#include "Framework/EngineContext.h"
#include "Utility/MathUtils.h"
#include "Collision/Collision.h"
#include <cmath>
#include <numbers>
#include <algorithm>

#ifdef _DEUSE_IMGUIBUG
#include "../../../externals/imgui/imgui.h"
#endif

using namespace GameEngine;

TPSCameraController::TPSCameraController() {
   rotationMatrix_ = MakeIdentity4x4();

   // キーコンフィグ設定
   keyConfig_ = std::make_unique<GameEngine::KeyConfig>();

   // キーボード設定
   keyConfig_->BindKey("CameraLeft", GameEngine::KeyCode::Left);
   keyConfig_->BindKey("CameraRight", GameEngine::KeyCode::Right);
   keyConfig_->BindKey("CameraUp", GameEngine::KeyCode::Up);
   keyConfig_->BindKey("CameraDown", GameEngine::KeyCode::Down);

   // ゲームパッド設定
   keyConfig_->BindRightStick("CameraLook");  // 右スティックでカメラ操作
}

void TPSCameraController::Initialize(Camera* camera, Player* target) {
   camera_ = camera;
   target_ = target;

   if (target_) {
	  pivotTarget_ = target_->GetWorldPosition();

	  // 初期カメラ位置を設定
	  Quaternion playerQuat = target_->GetRotationQuaternion();
	  Vector3 playerUp = RotateVector(Vector3(0.0f, 1.0f, 0.0f), playerQuat);
	  Quaternion yawRotation = MakeRotateAxisAngleQuaternion(playerUp, localYaw_);
	  Vector3 playerRight = RotateVector(Vector3(1.0f, 0.0f, 0.0f), playerQuat);
	  Vector3 rotatedRight = RotateVector(playerRight, yawRotation);
	  Quaternion pitchRotation = MakeRotateAxisAngleQuaternion(rotatedRight, localPitch_);
	  Quaternion finalRotation = pitchRotation * yawRotation * playerQuat;
	  Vector3 cameraDirection = RotateVector(Vector3(0.0f, 0.0f, 1.0f), finalRotation);
	  currentEye_ = pivotTarget_ + cameraDirection * distance_;

	  // 初期回転を設定
	  currentCameraRotation_ = finalRotation;
	  targetCameraRotation_ = finalRotation;
	  previousPlayerQuat_ = playerQuat;

	  // 初期惑星を設定
	  previousPlanet_ = target_->GetCurrentPlanet();
   }

   localYaw_ = 1.0f;
   localPitch_ = 0.0f;
}

void TPSCameraController::Update() {
   if (!camera_ || !target_) return;

   // target_のモデルが有効かチェック
   if (!target_->GetModel()) {
      return;
   }

   float dt = EngineContext::GetDeltaTime();

   // プレイヤーの位置をピボットターゲットとして更新
   pivotTarget_ = target_->GetWorldPosition();

   // プレイヤーの座標系を取得
   Quaternion playerQuat = target_->GetRotationQuaternion();
   Planet* currentPlanet = target_->GetCurrentPlanet();

   // 惑星切り替え検知
   bool planetChanged = (currentPlanet != previousPlanet_ && previousPlanet_ != nullptr && currentPlanet != nullptr);

   if (planetChanged) {
      // 惑星が切り替わった場合、現在のカメラ位置を保持しつつ新しい座標系へ変換
      
      // 現在のカメラの実際の位置（ワールド空間）を保存
      Vector3 currentWorldCameraPos = currentEye_;
      
      // 現在のプレイヤーからカメラへのベクトル
      Vector3 currentCameraOffset = currentWorldCameraPos - pivotTarget_;
      float currentActualDistance = currentCameraOffset.Length();
      
      // 距離を保存（遷移後に徐々に元の距離に戻す）
      savedDistance_ = currentActualDistance;
      
      // 現在のカメラ回転クォータニオンを保存
      // これをそのまま使用して滑らかに遷移
      currentCameraRotation_ = currentCameraRotation_.Normalize();
      targetCameraRotation_ = currentCameraRotation_;
      
      // 新しいプレイヤーの上方向を計算
      Vector3 newPlayerUp = RotateVector(Vector3(0.0f, 1.0f, 0.0f), playerQuat);
      Vector3 cameraDir = RotateVector(Vector3(0.0f, 0.0f, 1.0f), currentCameraRotation_);
      
      // 目標ロール角を即座に計算（固定値として保存）
      Vector3 projectedPlayerUp = newPlayerUp - cameraDir * cameraDir.Dot(newPlayerUp);
      if (projectedPlayerUp.Length() > 0.001f) {
         projectedPlayerUp = projectedPlayerUp.Normalize();
         
         Vector3 cameraUpNoRoll = RotateVector(Vector3(0.0f, 1.0f, 0.0f), currentCameraRotation_);
         Vector3 projectedCameraUp = cameraUpNoRoll - cameraDir * cameraDir.Dot(cameraUpNoRoll);
         
         if (projectedCameraUp.Length() > 0.001f) {
            projectedCameraUp = projectedCameraUp.Normalize();
            
            float cosAngle = std::clamp(projectedCameraUp.Dot(projectedPlayerUp), -1.0f, 1.0f);
            float newTargetRoll = std::acos(cosAngle);
            
            Vector3 cross = projectedCameraUp.Cross(projectedPlayerUp);
            if (cross.Dot(cameraDir) > 0.0f) {
               newTargetRoll = -newTargetRoll;
            }
            
            // ロール角を制限
            constexpr float maxRoll = std::numbers::pi_v<float> / 3.0f;
            targetRollForTransition_ = std::clamp(newTargetRoll, -maxRoll, maxRoll);
         } else {
            targetRollForTransition_ = 0.0f;
         }
      } else {
         targetRollForTransition_ = 0.0f;
      }
      
      // ロール遷移を開始
      startRoll_ = roll_;
      isRollTransitioning_ = true;
      rollTransitionTimer_ = 0.0f;
      
      // 惑星遷移状態に入る
      isPlanetTransitioning_ = true;
      transitionTimer_ = 0.0f;

      previousPlanet_ = currentPlanet;
   }
   
   // 惑星遷移中のタイマー更新
   if (isPlanetTransitioning_) {
      transitionTimer_ += dt;
      if (transitionTimer_ >= transitionDuration_) {
         isPlanetTransitioning_ = false;
         transitionTimer_ = 0.0f;
         
         // 遷移終了時に、現在のカメラ回転から正しいlocalYaw/Pitchを逆算
         Vector3 currentCameraDir = RotateVector(Vector3(0.0f, 0.0f, 1.0f), currentCameraRotation_);
         
         // 新しいプレイヤーの座標系でカメラ方向をローカル座標に変換
         Quaternion invPlayerQuat = playerQuat.Conjugate();
         Vector3 localCameraDir = RotateVector(currentCameraDir, invPlayerQuat);
         
         // ローカル座標系でのYawとPitchを計算
         Vector3 xzProjection = Vector3(localCameraDir.x, 0.0f, localCameraDir.z);
         if (xzProjection.Length() > 0.001f) {
            xzProjection = xzProjection.Normalize();
            
            // Z軸からの角度（Yaw）
            // atan2(x, z)でZ軸正方向を0度とする
            // プレイヤーの背後（-Z方向）がYaw=πになるように調整
            localYaw_ = std::atan2(xzProjection.x, xzProjection.z);
            
            // Y成分からPitchを計算
            float newPitch = std::asin(std::clamp(-localCameraDir.y, -1.0f, 1.0f));
            localPitch_ = newPitch;
         }
      }
   }
   
   // ロール遷移中のタイマー更新
   if (isRollTransitioning_) {
      rollTransitionTimer_ += dt;
      if (rollTransitionTimer_ >= rollTransitionDuration_) {
         isRollTransitioning_ = false;
         rollTransitionTimer_ = 0.0f;
         roll_ = targetRollForTransition_;
      }
   }

   // キー入力でカメラを回転させているかどうかをチェック
   isManualRotating_ = false;
   
   float cameraInputX = 0.0f;
   float cameraInputY = 0.0f;

   // キーボード入力
   if (keyConfig_->IsPressed("CameraLeft")) {
      cameraInputX += 1.0f;
      isManualRotating_ = true;
   }
   if (keyConfig_->IsPressed("CameraRight")) {
      cameraInputX -= 1.0f;
      isManualRotating_ = true;
   }
   if (keyConfig_->IsPressed("CameraUp")) {
      cameraInputY -= 1.0f;
      isManualRotating_ = true;
   }
   if (keyConfig_->IsPressed("CameraDown")) {
      cameraInputY += 1.0f;
      isManualRotating_ = true;
   }
   
   // ゲームパッドの右スティック入力（キーボード入力が無い場合のみ）
   if (cameraInputX == 0.0f && cameraInputY == 0.0f) {
      Vector2 stickInput = keyConfig_->GetStickVector("CameraLook", 0, 0.26f);
      if (stickInput.x != 0.0f || stickInput.y != 0.0f) {
         cameraInputX = stickInput.x;
         cameraInputY = stickInput.y;
         isManualRotating_ = true;
      }
   }

   // カメラ回転を適用
   if (cameraInputX != 0.0f) {
      localYaw_ += cameraInputX * rotateSpeed_;
      // キー入力があれば惑星遷移を終了
      isPlanetTransitioning_ = false;
   }
   if (cameraInputY != 0.0f) {
      localPitch_ += cameraInputY * rotateSpeed_;
      isPlanetTransitioning_ = false;
   }

   // ピッチの制限（±90度まで）
   constexpr float kPitchLimit = std::numbers::pi_v<float> / 2.5f;
   localPitch_ = std::clamp(localPitch_, -kPitchLimit, kPitchLimit * -0.3f);

   // プレイヤーのローカル座標軸をワールド空間に変換
   Vector3 playerUp = RotateVector(Vector3(0.0f, 1.0f, 0.0f), playerQuat);
   Vector3 playerRight = RotateVector(Vector3(1.0f, 0.0f, 0.0f), playerQuat);
   Vector3 playerForward = RotateVector(Vector3(0.0f, 0.0f, 1.0f), playerQuat);

   // プレイヤーのローカル座標系での回転を作成
   // 1. まずプレイヤーの頭の方向（Up）周りにYaw回転
   Quaternion yawRotation = MakeRotateAxisAngleQuaternion(playerUp, localYaw_);

   // 2. Yaw回転後の右方向を計算
   Vector3 rotatedRight = RotateVector(playerRight, yawRotation);

   // 3. その右方向周りにPitch回転
   Quaternion pitchRotation = MakeRotateAxisAngleQuaternion(rotatedRight, localPitch_);

   // 4. 最終的な目標回転を合成
   Quaternion newTargetRotation = pitchRotation * yawRotation * playerQuat;
   
   // 惑星遷移中でない場合のみ、目標回転を更新
   if (!isPlanetTransitioning_ || isManualRotating_) {
      targetCameraRotation_ = newTargetRotation;
   }

   // 回転速度を調整（惑星遷移中は遅く、キー入力中は速く）
   float currentRotationSpeed = cameraRotationSpeed_;
   if (isManualRotating_) {
      currentRotationSpeed = 50.0f;
   } else if (isPlanetTransitioning_) {
      // 遷移の進行度に応じて速度を調整（最初は遅く、徐々に速く）
      float transitionProgress = transitionTimer_ / transitionDuration_;
      currentRotationSpeed = 2.0f + (cameraRotationSpeed_ - 2.0f) * transitionProgress;
   }
   
   float rotationInterpTemp = currentRotationSpeed * dt;
   float rotationInterp = std::clamp(rotationInterpTemp, 0.0f, 1.0f);

   // 正しいSlerp補間を使用
   currentCameraRotation_ = Slerp(currentCameraRotation_, targetCameraRotation_, rotationInterp);

   // カメラのオフセット方向を計算
   Vector3 cameraDirection = RotateVector(Vector3(0.0f, 0.0f, 1.0f), currentCameraRotation_);
   
   // 距離を調整（惑星遷移中は保存した距離から徐々に元の距離に戻す）
   float targetDistance = distance_;
   if (isPlanetTransitioning_ && savedDistance_ > 0.0f) {
      float transitionProgress = transitionTimer_ / transitionDuration_;
      // イージング関数を使用して滑らかに遷移
      float easedProgress = transitionProgress * transitionProgress * (3.0f - 2.0f * transitionProgress); // smoothstep
      targetDistance = savedDistance_ + (distance_ - savedDistance_) * easedProgress;
   }
   
   Vector3 offset = cameraDirection * targetDistance;
   Vector3 targetEye = pivotTarget_ + offset;

   // 追従速度を調整（惑星遷移中は遅く、キー入力中は速く）
   float currentFollowSpeed = followSpeed_;
   if (isManualRotating_) {
      currentFollowSpeed = followSpeed_ * 3.0f;
   } else if (isPlanetTransitioning_) {
      // 遷移中は非常にゆっくり追従
      currentFollowSpeed = followSpeed_ * 0.3f;
   }
   
   float interpolationFactorTemp = currentFollowSpeed * dt;
   float interpolationFactor = std::clamp(interpolationFactorTemp, 0.0f, 1.0f);

   // カメラ位置を補間
   Vector3 eyeDiff = targetEye - currentEye_;
   currentEye_ += eyeDiff * interpolationFactor;

   // 惑星との衝突判定を行い、カメラ位置を調整
   Vector3 eye = ResolveCollisionWithPlanets(currentEye_, pivotTarget_);
   currentEye_ = eye;  // 調整後の位置を保存

   // カメラの前方向（プレイヤーへの視線方向の逆）をキャッシュ
   cameraForward_ = (cameraDirection * -1.0f).Normalize();

   // カメラの右方向をキャッシュ
   cameraRight_ = RotateVector(Vector3(1.0f, 0.0f, 0.0f), currentCameraRotation_).Normalize();

 // 1. ロール遷移フラグによる条件分岐を廃止し、常に目標値を計算する
   float targetRoll = 0.0f;
   Vector3 cameraUpNoRoll = RotateVector(Vector3(0.0f, 1.0f, 0.0f), currentCameraRotation_);

   // 目標ロール角をリアルタイムに計算
   Vector3 projectedPlayerUp = playerUp - cameraDirection * cameraDirection.Dot(playerUp);
   if (projectedPlayerUp.Length() > 0.001f) {
      projectedPlayerUp = projectedPlayerUp.Normalize();
      Vector3 projectedCameraUp = cameraUpNoRoll - cameraDirection * cameraDirection.Dot(cameraUpNoRoll);
      
      if (projectedCameraUp.Length() > 0.001f) {
         projectedCameraUp = projectedCameraUp.Normalize();
         float cosAngle = std::clamp(projectedCameraUp.Dot(projectedPlayerUp), -1.0f, 1.0f);
         targetRoll = std::acos(cosAngle);
         Vector3 cross = projectedCameraUp.Cross(projectedPlayerUp);
         if (cross.Dot(cameraDirection) > 0.0f) {
            targetRoll = -targetRoll;
         }
         // ロール制限
         constexpr float maxRoll = std::numbers::pi_v<float> / 3.0f;
         targetRoll = std::clamp(targetRoll, -maxRoll, maxRoll);
      }
   }

   // 2. 補間速度を状況に応じて動的に変える
   float currentRollInterpSpeed = rollSpeed_; // 通常時の速度

   if (isManualRotating_) {
      // 手動操作中はロールを戻す速度を速くする、または0に固定
      targetRoll = 0.0f;
      currentRollInterpSpeed = 20.0f; 
   } 
   else if (isPlanetTransitioning_) {
      // 惑星遷移中は、本体の回転に合わせてロールもゆっくり、かつ確実に追従させる
      // 遷移の進行度 (transitionTimer_ / transitionDuration_) に応じて速度を上げるとより滑らか
      float progress = transitionTimer_ / transitionDuration_;
      currentRollInterpSpeed = 2.0f + (rollSpeed_ - 2.0f) * progress;
   }

   // 3. 毎フレーム補間を実行（これが二段階現象を防ぐ鍵）
   float rollDiff = targetRoll - roll_;
   // 最短経路で回転するように調整（-π〜πの範囲で考える場合）
   if (rollDiff > std::numbers::pi_v<float>) rollDiff -= 2.0f * std::numbers::pi_v<float>;
   if (rollDiff < -std::numbers::pi_v<float>) rollDiff += 2.0f * std::numbers::pi_v<float>;

   roll_ += rollDiff * std::clamp(currentRollInterpSpeed * dt, 0.0f, 1.0f);

   // ロールを適用した最終的なカメラの上方向
   Quaternion rollRotation = MakeRotateAxisAngleQuaternion(cameraDirection, roll_);
   Vector3 finalCameraUp = RotateVector(cameraUpNoRoll, rollRotation);

   // ビュー行列の作成
   Matrix4x4 viewMatrix = MakeLookAtMatrix(eye, pivotTarget_, finalCameraUp);
   Matrix4x4 projectionMatrix = camera_->GetProjectionMatrix();
   Matrix4x4 viewProjectionMatrix = viewMatrix * projectionMatrix;

   camera_->SetViewProjectionMatrix(viewProjectionMatrix);

   // カメラのトランスフォームも更新
   Matrix4x4 worldMatrix = viewMatrix.Inverse();

   Transform transform;
   transform.scale = { 1, 1, 1 };
   transform.translation = eye;

   if (camera_->IsUsingQuaternion()) {
      Matrix4x4 rotationOnly = worldMatrix;
      rotationOnly.m[3][0] = 0.0f;
      rotationOnly.m[3][1] = 0.0f;
      rotationOnly.m[3][2] = 0.0f;
      rotationOnly.m[0][3] = 0.0f;
      rotationOnly.m[1][3] = 0.0f;
      rotationOnly.m[2][3] = 0.0f;
      rotationOnly.m[3][3] = 1.0f;

      Quaternion quaternion = MatrixToQuaternion(rotationOnly);
      camera_->SetQuaternion(quaternion);
      transform.rotation = { 0, 0, 0 };
   } else {
      Vector3 euler = ExtractYawPitchRoll(worldMatrix);
      transform.rotation = euler;
   }

   camera_->SetTransform(transform);
   camera_->SetCameraForGpuData();

   // 前回のプレイヤーQuaternionを保存
   previousPlayerQuat_ = playerQuat;

#ifdef USE_IMGUI
   // デバッグ情報表示
   ImGui::Begin("TPS Camera Debug");
   ImGui::Text("Local Yaw: %.3f (deg: %.1f)", localYaw_, localYaw_ * 180.0f / std::numbers::pi_v<float>);
   ImGui::Text("Local Pitch: %.3f (deg: %.1f)", localPitch_, localPitch_ * 180.0f / std::numbers::pi_v<float>);
   ImGui::Text("Roll: %.3f (deg: %.1f)", roll_, roll_ * 180.0f / std::numbers::pi_v<float>);
   ImGui::Text("Target Roll: %.3f (deg: %.1f)", targetRoll, targetRoll * 180.0f / std::numbers::pi_v<float>);
   ImGui::Text("Distance: %.2f", distance_);
   ImGui::Text("Target Distance: %.2f", targetDistance);
   ImGui::Text("Saved Distance: %.2f", savedDistance_);
   ImGui::Text("Pivot: (%.2f, %.2f, %.2f)", pivotTarget_.x, pivotTarget_.y, pivotTarget_.z);
   ImGui::Text("Eye: (%.2f, %.2f, %.2f)", eye.x, eye.y, eye.z);
   ImGui::Text("Current Eye: (%.2f, %.2f, %.2f)", currentEye_.x, currentEye_.y, currentEye_.z);
   ImGui::Text("Planet Changed: %s", planetChanged ? "YES" : "NO");
   ImGui::Text("Planet Transitioning: %s", isPlanetTransitioning_ ? "YES" : "NO");
   ImGui::Text("Transition Progress: %.2f%%", isPlanetTransitioning_ ? (transitionTimer_ / transitionDuration_ * 100.0f) : 0.0f);
   ImGui::Text("Manual Rotating: %s", isManualRotating_ ? "YES" : "NO");
   
   // ロール遷移情報
   ImGui::Separator();
   ImGui::Text("Roll Transitioning: %s", isRollTransitioning_ ? "YES" : "NO");
   if (isRollTransitioning_) {
      float rollProgress = rollTransitionTimer_ / rollTransitionDuration_;
      ImGui::Text("Roll Transition Progress: %.2f%%", rollProgress * 100.0f);
      ImGui::Text("Start Roll: %.3f (deg: %.1f)", startRoll_, startRoll_ * 180.0f / std::numbers::pi_v<float>);
      ImGui::Text("Target Roll (Transition): %.3f (deg: %.1f)", targetRollForTransition_, targetRollForTransition_ * 180.0f / std::numbers::pi_v<float>);
      ImGui::Text("Current Roll: %.3f (deg: %.1f)", roll_, roll_ * 180.0f / std::numbers::pi_v<float>);
      
      // イージング値を表示
      float t = std::clamp(rollProgress, 0.0f, 1.0f);
      float easedT;
      if (t < 0.5f) {
         easedT = 8.0f * t * t * t * t;
      } else {
         float f = t - 1.0f;
         easedT = 1.0f - 8.0f * f * f * f * f;
      }
      ImGui::Text("Eased Progress: %.2f%%", easedT * 100.0f);
   } else {
      // 通常時のロール情報
      ImGui::Text("Target Roll (Normal): %.3f (deg: %.1f)", targetRoll, targetRoll * 180.0f / std::numbers::pi_v<float>);
      ImGui::Text("Current Roll: %.3f (deg: %.1f)", roll_, roll_ * 180.0f / std::numbers::pi_v<float>);
      ImGui::Text("Roll Diff: %.3f (deg: %.1f)", (targetRoll - roll_), (targetRoll - roll_) * 180.0f / std::numbers::pi_v<float>);
   }
   
   // Quaternion情報を追加
   ImGui::Separator();
   ImGui::Text("Current Camera Rotation:");
   ImGui::Text("  x: %.3f, y: %.3f", currentCameraRotation_.x, currentCameraRotation_.y);
   ImGui::Text("  z: %.3f, w: %.3f", currentCameraRotation_.z, currentCameraRotation_.w);
   ImGui::Text("Target Camera Rotation:");
   ImGui::Text("  x: %.3f, y: %.3f", targetCameraRotation_.x, targetCameraRotation_.y);
   ImGui::Text("  z: %.3f, w: %.3f", targetCameraRotation_.z, targetCameraRotation_.w);
   
   float quatDot = currentCameraRotation_.Dot(targetCameraRotation_);
   ImGui::Text("Quat Dot Product: %.3f", quatDot);
   
   // カメラ方向のデバッグ情報
   ImGui::Separator();
   ImGui::Text("Camera Direction (World):");
   ImGui::Text("  (%.2f, %.2f, %.2f)", cameraDirection.x, cameraDirection.y, cameraDirection.z);
   
   // ローカル座標系でのカメラ方向を計算して表示
   Quaternion invPlayerQuat = playerQuat.Conjugate();
   Vector3 localCameraDir = RotateVector(cameraDirection, invPlayerQuat);
   ImGui::Text("Camera Direction (Local):");
   ImGui::Text("  (%.2f, %.2f, %.2f)", localCameraDir.x, localCameraDir.y, localCameraDir.z);

   ImGui::Separator();
   ImGui::Text("Player Axes:");
   ImGui::Text("Up: (%.2f, %.2f, %.2f)", playerUp.x, playerUp.y, playerUp.z);
   ImGui::Text("Right: (%.2f, %.2f, %.2f)", playerRight.x, playerRight.y, playerRight.z);
   ImGui::Text("Forward: (%.2f, %.2f, %.2f)", playerForward.x, playerForward.y, playerForward.z);

   ImGui::Separator();
   ImGui::SliderFloat("Roll Speed", &rollSpeed_, 0.1f, 20.0f);
   ImGui::SliderFloat("Rotate Speed", &rotateSpeed_, 0.01f, 0.2f);
   ImGui::SliderFloat("Follow Speed", &followSpeed_, 1.0f, 20.0f);
   ImGui::SliderFloat("Camera Rotation Speed", &cameraRotationSpeed_, 1.0f, 20.0f);
   ImGui::SliderFloat("Transition Duration", &transitionDuration_, 0.1f, 2.0f);
   ImGui::SliderFloat("Roll Transition Duration", &rollTransitionDuration_, 0.1f, 2.0f);

   // リセットボタン
   if (ImGui::Button("Reset Camera")) {
	  localYaw_ = 0.0f;
	  localPitch_ = -std::numbers::pi_v<float> / 2.0f;
	  roll_ = 0.0f;
   }

   // キー入力状態
   ImGui::Separator();
   ImGui::Text("Key Input:");
   ImGui::Text("Left: %s", EngineContext::IsKeyPressed(KeyCode::Left) ? "ON" : "OFF");
   ImGui::Text("Right: %s", EngineContext::IsKeyPressed(KeyCode::Right) ? "ON" : "OFF");
   ImGui::Text("Up: %s", EngineContext::IsKeyPressed(KeyCode::Up) ? "ON" : "OFF");
   ImGui::Text("Down: %s", EngineContext::IsKeyPressed(KeyCode::Down) ? "ON" : "OFF");
   ImGui::End();
#endif
}

Vector3 TPSCameraController::GetCameraForward() const {
   return cameraForward_;
}

Vector3 TPSCameraController::GetCameraRight() const {
   return cameraRight_;
}

Vector3 TPSCameraController::ResolveCollisionWithPlanets([[maybe_unused]] const Vector3& desiredEye, const Vector3& pivot) {
   if (!target_ || !target_->GetModel()) return desiredEye;
   
   Vector3 adjustedEye = desiredEye;
   
   // プレイヤーからカメラへの方向とレイ
   Vector3 toCameraDir = (desiredEye - pivot);
   float desiredDistance = toCameraDir.Length();
   
   if (desiredDistance < 0.001f) {
      return adjustedEye;
   }
   
   toCameraDir = toCameraDir.Normalize();
   
   // Rayを作成（プレイヤーからカメラへ）
   GameEngine::Collider::Ray ray;
   ray.origin = pivot;
   ray.diff = toCameraDir * desiredDistance;
   
   float minDistance = desiredDistance;
   bool hitDetected = false;
   
   // 各惑星との衝突をチェック
   for (Planet* planet : planets_) {
      if (!planet) continue;
      
      Vector3 planetCenter = planet->GetWorldPosition();
      float planetRadius = planet->GetPlanetRadius();
      
      // 惑星の球体コライダーを作成
      GameEngine::Collider::Sphere planetSphere;
      planetSphere.center = planetCenter;
      planetSphere.radius = planetRadius + cameraMinDistance_;  // マージンを含める
      
      // Rayと惑星の衝突判定
      if (GameEngine::Collision::IsCollision(ray, planetSphere)) {
         // 衝突点までの距離を計算
         Vector3 toPlanet = planetCenter - pivot;
         float distToPlanetCenter = toPlanet.Length();
         
         // Rayが惑星に当たる最短距離を計算（二次方程式）
         // プレイヤーから惑星表面（マージン含む）までの距離
         float distToSurface = distToPlanetCenter - (planetRadius + cameraMinDistance_);
         
         // プレイヤーが惑星の外側にいる場合のみ調整
         if (distToSurface > 0.0f) {
            // Rayの方向での距離を計算
            float projectedDist = toPlanet.Dot(toCameraDir);
            
            if (projectedDist > 0.0f) {
               // 球との交点までの距離を計算（二次方程式）
               float perpDist = (toPlanet - toCameraDir * projectedDist).Length();
               float sphereRadiusWithMargin = planetRadius + cameraMinDistance_;
                
               if (perpDist < sphereRadiusWithMargin) {
                  float distToIntersection = projectedDist - std::sqrt(sphereRadiusWithMargin * sphereRadiusWithMargin - perpDist * perpDist);
                  
                  if (distToIntersection > 0.0f && distToIntersection < minDistance) {
                     minDistance = distToIntersection * 0.95f;  // 少し手前で止める
                     hitDetected = true;
                  }
               }
            }
         }
      }
   }
   
   // 惑星に衝突している場合は距離を調整
   if (hitDetected) {
      float finalDistance = (minDistance > 2.0f) ? minDistance : 2.0f;  // 最小距離2.0fを確保
      adjustedEye = pivot + toCameraDir * finalDistance;
      
#ifdef USE_IMGUI
      // デバッグライン描画：衝突時は赤
      GameEngine::EngineContext::DrawLine(pivot, adjustedEye, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
#endif
   } else {
#ifdef USE_IMGUI
      // デバッグライン描画：通常時は緑
      GameEngine::EngineContext::DrawLine(pivot, adjustedEye, Vector4(0.0f, 1.0f, 0.0f, 0.3f));
#endif
   }
   
   return adjustedEye;
}
