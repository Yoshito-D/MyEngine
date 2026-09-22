#include "RearCameraPositionSolver.h"
#include "PlayerRearFollowCamera.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "RearCameraMath.h"

namespace App {
using namespace RearCameraMath;

void RearCameraPositionSolver::MutateCameraState(GameEngine::CameraState& state, float deltaTime) {
   auto* camera = GetRearCamera();
   if (!camera) return;
   const auto& transition = *owner_->GetComponent<RearCameraTransition>();
   const auto& gravity = *owner_->GetComponent<RearCameraGravityUp>();
   const auto& direction = *owner_->GetComponent<RearCameraDirectionTracker>();
   const auto& speed = *owner_->GetComponent<RearCameraSpeedEffects>();
   const auto& aim = *owner_->GetComponent<RearCameraAimSolver>();
   state.transform.translation = Update(camera->GetInput(), *camera, transition.GetState(), direction.GetState(),
      speed.GetState(), aim.GetState(), state, gravity.GetState().currentGravityUp, speed.GetBoostAlpha(), deltaTime);
}

/// @brief eye 位置を計算する（後退距離に加速ブーストを加味）
/// @details カメラは pivot の後方（currentBackward 方向）に distance 離れた場所に置く。
///          加速中は distanceBoostMax * boostAlpha 分さらに後退させることで
///          「カメラが引けて世界が広がる」視覚的な加速感を演出する。
///          空中では currentAirborneBlend に応じてさらに後退し、周囲を広く映す。
/// @param up カメラ位置の高さ方向
/// @param boostAlpha 加速度合い [0,1]
GameEngine::Vector3 RearCameraPositionSolver::ComputeEye(
   const RearCameraInput& input,
   const RearCameraSettings& settings,
   const RearCameraTransitionState& transition,
   const RearCameraDirectionState& direction,
   const RearCameraSpeedState& speed,
   const GameEngine::Vector3& up,
   float boostAlpha) const {
   // 加速と空中状態の後退量を合成する。
   float boostedDistance =
	  settings.distance
	  + settings.distanceBoostMax * boostAlpha
	  + speed.springDistanceOffset
	  + settings.airborneDistanceOffset * transition.currentAirborneBlend;

   // 高さを後方ベクトルへそのまま加えると、上昇中に backward と Up が逆向きになった場面で
   // settings.distance と settings.height が相殺し、カメラがプレイヤーへ急接近する。
   // Up のうち後方と直交する成分だけを高さへ使い、後方距離を常に独立して確保する。
   GameEngine::Vector3 safeBackward = NormalizeOrFallback(direction.currentBackward, -up);
   GameEngine::Vector3 orthogonalUp = up - safeBackward * up.Dot(safeBackward);
   float safeRearDistance = std::max(0.0f, boostedDistance);
   GameEngine::Vector3 eyeOffset =
	  safeBackward * safeRearDistance
	  + orthogonalUp * settings.height;

   // settings.distance はカメラの基準距離なので、速度Springや方向合成の結果でも下回らせない。
   float minimumEyeDistance = std::max(1.0f, settings.distance);
   float eyeOffsetLength = eyeOffset.Length();
   if (eyeOffsetLength < minimumEyeDistance) {
	  GameEngine::Vector3 fallbackDirection =
		 eyeOffsetLength > 1e-4f
		 ? eyeOffset * (1.0f / eyeOffsetLength)
		 : safeBackward;
	  eyeOffset = fallbackDirection * minimumEyeDistance;
   }
   GameEngine::Vector3 eye = input.pivotTarget + eyeOffset;

   // 着地前は予測地表の外向き法線に対して最低限の高さを確保する。
   // これにより、速度後方から着地接線後方へ移る途中でもカメラがプレイヤーと惑星の間へ入らない。
   if (input.isAirborne && transition.currentPreLandingBlend > 1e-4f) {
	  GameEngine::Vector3 landingUp = NormalizeOrFallback(input.predictedLandingUp, up);
	  float minimumOutwardHeight =
		 std::max(0.0f, settings.preLandingMinOutwardHeight) * transition.currentPreLandingBlend;
	  float outwardHeight = (eye - input.pivotTarget).Dot(landingUp);
	  if (outwardHeight < minimumOutwardHeight) {
		 eye += landingUp * (minimumOutwardHeight - outwardHeight);
	  }
   }

   return eye;
}

/// @brief 目標 eye オフセット（ピボット相対）の方向変化を制限し、距離を補間する
/// @details 【なぜ相対オフセットで補間するか】
///          絶対座標で補間すると、ピボット（プレイヤー）が移動するたびに
///          理想 eye も一緒にワールド空間を動く。この場合の補間パスは
///          「前フレームの eye（旧ピボット後方）→ 今フレームの eye（新ピボット後方）」
///          という直線を通るため、一瞬プレイヤーを突き抜けるルートになり得る。
///
///          現在の実装では方向を UpdateBackwardVector で一度だけ補間し、
///          ここでは通常の方向変化へ遅延を足さず、急変時だけ最大角速度を制限する。
///          ピボット相対距離は指数平滑で目標へ近づける。
///          補間後はUp方向の高さ下限を適用し、カメラが基準面より下へ沈みすぎることを防ぐ。
GameEngine::Vector3 RearCameraPositionSolver::SmoothEye(
   const RearCameraInput& input,
   const RearCameraSettings& settings,
   const RearCameraTransitionState& transition,
   const RearCameraAimState& aim,
   const GameEngine::Vector3& targetEye,
   const GameEngine::Vector3& up,
   float deltaTime) {
   using namespace GameEngine;

   // ピボットから見た理想オフセット
   Vector3 targetOffset = targetEye - input.pivotTarget;
   Vector3 safeUp = NormalizeOrFallback(up, { 0.0f, 1.0f, 0.0f });

   if (!state_.isEyeInitialized) {
	  // 初回はスナップ（補間履歴なし）
	  state_.currentEyeOffset = targetOffset;
	  state_.isEyeInitialized = true;
	  return input.pivotTarget + state_.currentEyeOffset;
   }

   // ピボット相対オフセットの距離を補間する。
   float t = ExpSmoothingFactor(settings.positionLerpSpeed, deltaTime);

   float currentLength = state_.currentEyeOffset.Length();
   float targetLength = targetOffset.Length();
   float smoothedLength = currentLength + (targetLength - currentLength) * t;
   smoothedLength = std::max(smoothedLength, std::max(1.0f, settings.distance));
   Vector3 currentDir =
	  currentLength > 1e-4f
	  ? state_.currentEyeOffset * (1.0f / currentLength)
	  : safeUp;
   Vector3 targetDir = targetLength > 1e-4f ? targetOffset * (1.0f / targetLength) : safeUp;

   // 低いジャンプでは空中後方と着地後方が短時間に往復する。
   // targetDirを即採用すると最終eye位置だけが大きく旋回するため、通常の小さな変化はそのまま通し、
   // 1フレームの方向変化が上限を超える場合だけ角速度を制限する。
   float targetAngle = std::acos(std::clamp(currentDir.Dot(targetDir), -1.0f, 1.0f));
   float maxAngularDelta =
	  std::max(0.0f, settings.eyeDirectionMaxAngularSpeed) * std::max(0.0f, deltaTime);
   Vector3 outputDir = targetAngle <= maxAngularDelta
	  ? targetDir
	  : RotateTowardsUnit(currentDir, targetDir, maxAngularDelta, aim.cachedRight);
   state_.currentEyeOffset = outputDir * smoothedLength;

   // 現在の settings.height をそのまま扱い、一定以下にならないように制限をかける
   float currentHeight = state_.currentEyeOffset.Dot(safeUp);

   // 地上・空中共通の基準面から確保する高さ。
   float minHeightLimit = 0.5f;
   bool heightCorrected = false;

   // currentHeight が制限値を下回っていたら押し上げる
   if (currentHeight < minHeightLimit) {
	  // 足りない高さの分だけ、Up方向へ加算する
	  state_.currentEyeOffset += safeUp * (minHeightLimit - currentHeight);
      heightCorrected = true;
   }

   if (input.isAirborne && transition.currentPreLandingBlend > 1e-4f) {
	  Vector3 landingUp = NormalizeOrFallback(input.predictedLandingUp, safeUp);
	  float minimumOutwardHeight =
		 std::max(0.0f, settings.preLandingMinOutwardHeight) * transition.currentPreLandingBlend;
	  float outwardHeight = state_.currentEyeOffset.Dot(landingUp);
	  if (outwardHeight < minimumOutwardHeight) {
		 state_.currentEyeOffset += landingUp * (minimumOutwardHeight - outwardHeight);
         heightCorrected = true;
	  }
   }

   if (heightCorrected) {
      // 高さ補正は負のUp成分を打ち消すため、先に確保した距離を短くすることがある。
      // 最終候補でも距離を守り、保存位置が基準面の下でもプレイヤーへ急接近させない。
      float correctedLength = state_.currentEyeOffset.Length();
      const float minimumDistance = std::max(1.0f, settings.distance);
      const Vector3 correctedDirection = NormalizeOrFallback(state_.currentEyeOffset, safeUp);
      if (correctedLength < minimumDistance) {
         correctedLength = minimumDistance;
         state_.currentEyeOffset = correctedDirection * correctedLength;
      }

      // 高さの押し上げで角速度上限を迂回しない。制約外の保存位置や急変したUpからは、
      // 距離と連続回転を優先して安全側の候補へ戻す（途中の高さは即時に強制しない）。
      const float correctedAngle = std::acos(std::clamp(currentDir.Dot(correctedDirection), -1.0f, 1.0f));
      if (correctedAngle > maxAngularDelta + 1e-5f) {
         state_.currentEyeOffset = RotateTowardsUnit(currentDir, correctedDirection, maxAngularDelta, aim.cachedRight)
            * std::max(smoothedLength, correctedLength);
      }
   }

   // ワールド座標に戻して返す
   return input.pivotTarget + state_.currentEyeOffset;
}

GameEngine::Vector3 RearCameraPositionSolver::Update(
   const RearCameraInput& input,
   const RearCameraSettings& settings,
   const RearCameraTransitionState& transition,
   const RearCameraDirectionState& direction,
   const RearCameraSpeedState& speed,
   const RearCameraAimState& aim,
   GameEngine::CameraState& state,
   const GameEngine::Vector3& up,
   float boostAlpha,
   float deltaTime) {
   // ⑦ 加速ブーストと空中距離を加味した eye 位置を算出する
   //    → 加速中はカメラが後退して視野が広がり、速度感が増す
   GameEngine::Vector3 eye = ComputeEye(input, settings, transition, direction, speed, up, boostAlpha);

   // ⑧ 初回はシーンに保存された配置を追従補間の始点にする。
   //    ここで理想位置へスナップすると、カウントダウン開始時点で編集時の配置からずれる。
   if (!state_.isEyeInitialized) {
	  state_.currentEyeOffset = state.transform.translation - input.pivotTarget;
	  state_.isEyeInitialized = true;
	  eye = state.transform.translation;
   } else {
	  // ピボット相対距離を補間し、方向は急変時だけ最大角速度を制限する。
	  eye = SmoothEye(input, settings, transition, aim, eye, up, deltaTime);
   }

   return eye;
}

void RearCameraPositionSolver::Reset(const RearCameraSettings& settings) {
   state_.currentEyeOffset = { 0.0f, settings.height, -settings.distance };
   state_.isEyeInitialized = false;
}

} // namespace App
