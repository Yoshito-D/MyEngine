#include "PlayerRearFollowCamera.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "Utility/MathUtils/MatrixOperations.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "imgui.h"
#include "Object/Component/IObjectComponent.h"
#endif

namespace App {

// =============================================================================
// ユーティリティ
// =============================================================================

constexpr float kPlanetVisibilityGainFadeRange = 0.08f;
constexpr float kDefaultSafeFov = 0.45f;
constexpr float kMinSafeFov = 0.017453292f;  // 1 degree
constexpr float kMaxSafeFov = 3.12413936f;   // 179 degrees
constexpr float kMaxSpringDeltaTime = 0.25f;
constexpr float kMaxSpringStep = 1.0f / 120.0f;
// 旧ターボ係数が最大になっていた速度差を統合後も基準として使う。
constexpr float kSpeedDeltaForMaxKick = 5.0f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kGroundDirectionProjectionBlendRange = 0.15f;
constexpr float kLookAtDegenerateBlendRange = 0.15f;

static float ClampCameraFov(float fov) {
   if (!std::isfinite(fov)) {
	  return kDefaultSafeFov;
   }
   return std::clamp(fov, kMinSafeFov, kMaxSafeFov);
}

/// @brief 指数平滑の補間係数を返す（dt 変動に強く、常に 0..1 未満）
static float ExpSmoothingFactor(float speed, float deltaTime) {
   float k = (std::max)(0.0f, speed);
   float dt = (std::max)(0.0f, deltaTime);
   return 1.0f - std::exp(-k * dt);
}

static float ReadFloat(const nlohmann::json& data, const char* key, float fallback) {
   return data.contains(key) && data.at(key).is_number() ? data.at(key).get<float>() : fallback;
}

static bool ReadBool(const nlohmann::json& data, const char* key, bool fallback) {
   return data.contains(key) && data.at(key).is_boolean() ? data.at(key).get<bool>() : fallback;
}

static GameEngine::Vector3 NormalizeOrFallback(
   const GameEngine::Vector3& value,
   const GameEngine::Vector3& fallback) {
   float len = value.Length();
   if (len > 1e-5f) {
	  return value * (1.0f / len);
   }

   float fallbackLen = fallback.Length();
   if (fallbackLen > 1e-5f) {
	  return fallback * (1.0f / fallbackLen);
   }

   return { 0.0f, 0.0f, 1.0f };
}

const bool kRegistered = GameEngine::VirtualCamera::RegisterComponentFactory(
   "PlayerRearFollowCamera",
   [](GameEngine::VirtualCamera& camera) -> GameEngine::ICinemachineComponent* {
	  if (auto* existing = camera.GetComponent<PlayerRearFollowCamera>()) {
		 return existing;
	  }
	  return camera.AddComponent<PlayerRearFollowCamera>();
   });

/// @brief 1次元Spring（半陰的オイラー）で状態を1ステップ進める
static void StepSpring1D(float target,
   float stiffness,
   float damping,
   float deltaTime,
   float& inOutValue,
   float& inOutVelocity) {
   float dt = std::clamp(deltaTime, 0.0f, kMaxSpringDeltaTime);
   if (dt <= 0.0f) return;

   float k = (std::max)(0.0f, stiffness);
   float c = (std::max)(0.0f, damping);

   if (!std::isfinite(inOutValue)) {
	  inOutValue = target;
   }
   if (!std::isfinite(inOutVelocity)) {
	  inOutVelocity = 0.0f;
   }

   // 起動直後やブレーク復帰時の大きな dt をそのまま入れると、半陰的オイラーでも
   // ばね速度が反転し過ぎて FOV オフセットが負方向へ大きく飛ぶため、小刻みに積分する。
   while (dt > 0.0f) {
	  float step = (std::min)(dt, kMaxSpringStep);
	  float accel = -k * (inOutValue - target) - c * inOutVelocity;
	  inOutVelocity += accel * step;
	  inOutValue += inOutVelocity * step;
	  dt -= step;

	  if (!std::isfinite(inOutValue) || !std::isfinite(inOutVelocity)) {
		 inOutValue = target;
		 inOutVelocity = 0.0f;
		 return;
	  }
   }
}

/// @brief 現在ベクトルを目標ベクトルへ「最大角速度」で回転させる
/// @details 180°近傍でもゼロベクトル化を避けるため、線形補間ではなく角度ベースで追従する。
static GameEngine::Vector3 RotateTowardsUnit(const GameEngine::Vector3& current,
   const GameEngine::Vector3& target,
   float maxRadiansDelta) {
   using namespace GameEngine;

   Vector3 c = current;
   float cLen = c.Length();
   if (cLen < 1e-6f) c = { 0.0f, 1.0f, 0.0f };
   else c = c * (1.0f / cLen);

   Vector3 t = target;
   float tLen = t.Length();
   if (tLen < 1e-6f) t = { 0.0f, 1.0f, 0.0f };
   else t = t * (1.0f / tLen);

   float dot = std::clamp(c.Dot(t), -1.0f, 1.0f);
   float angle = std::acos(dot);
   if (angle < 1e-6f) {
	  return t;
   }

   float step = (std::max)(0.0f, maxRadiansDelta);
   if (step >= angle) {
	  return t;
   }

   if (dot < -0.999f) {
	  Vector3 axis = (std::abs(c.x) < 0.9f) ? Vector3{ 1.0f, 0.0f, 0.0f } : Vector3{ 0.0f, 1.0f, 0.0f };
	  axis = c.Cross(axis);
	  float axisLen = axis.Length();
	  if (axisLen < 1e-6f) {
		 axis = Vector3{ 0.0f, 0.0f, 1.0f };
	  } else {
		 axis = axis * (1.0f / axisLen);
	  }

	  // ロドリゲス回転
	  float cs = std::cos(step);
	  float sn = std::sin(step);
	  return c * cs + axis.Cross(c) * sn + axis * (axis.Dot(c) * (1.0f - cs));
   }

   float tRatio = step / angle;
   float sinTotal = std::sin(angle);
   float w0 = std::sin((1.0f - tRatio) * angle) / sinTotal;
   float w1 = std::sin(tRatio * angle) / sinTotal;
   Vector3 out = c * w0 + t * w1;
   float outLen = out.Length();
   return outLen > 1e-6f ? out * (1.0f / outLen) : t;
}

/// @brief 単位方向同士を角度ベースで補間する
/// @details 方向ベクトルを通常の Lerp で混ぜると、180°近い組み合わせで途中がゼロに近づき、
///          Normalize 後に反対方向へ跳ぶため、カメラの前後反転として見える。
///          角度量へ変換して RotateTowardsUnit することで、必ず現在方向から目標方向へ連続回転させる。
static GameEngine::Vector3 BlendUnitDirectionSafely(const GameEngine::Vector3& current,
   const GameEngine::Vector3& target,
   float blend,
   const GameEngine::Vector3& fallback) {
   GameEngine::Vector3 from = NormalizeOrFallback(current, fallback);
   GameEngine::Vector3 to = NormalizeOrFallback(target, from);
   float t = std::clamp(blend, 0.0f, 1.0f);
   if (t <= 0.0f) {
	  return from;
   }
   if (t >= 1.0f) {
	  return to;
   }

   float angle = std::acos(std::clamp(from.Dot(to), -1.0f, 1.0f));
   return RotateTowardsUnit(from, to, angle * t);
}

/// @brief 単位ベクトルを指定軸まわりに回転する
static GameEngine::Vector3 RotateAroundAxisUnit(const GameEngine::Vector3& value,
   const GameEngine::Vector3& axis,
   float radians) {
   GameEngine::Vector3 v = NormalizeOrFallback(value, { 0.0f, 0.0f, 1.0f });
   GameEngine::Vector3 n = NormalizeOrFallback(axis, { 0.0f, 1.0f, 0.0f });
   float cs = std::cos(radians);
   float sn = std::sin(radians);
   GameEngine::Vector3 out = v * cs + n.Cross(v) * sn + n * (n.Dot(v) * (1.0f - cs));
   return NormalizeOrFallback(out, v);
}

/// @brief 同一平面上の単位方向同士の符号付き角度を返す
/// @details 180度付近では外積がほぼゼロになって符号を失うため、直前の旋回符号を使う。
static float SignedAngleAroundAxis(const GameEngine::Vector3& from,
   const GameEngine::Vector3& to,
   const GameEngine::Vector3& axis,
   float antipodalSign) {
   GameEngine::Vector3 safeFrom = NormalizeOrFallback(from, { 0.0f, 0.0f, 1.0f });
   GameEngine::Vector3 safeTo = NormalizeOrFallback(to, safeFrom);
   GameEngine::Vector3 safeAxis = NormalizeOrFallback(axis, { 0.0f, 1.0f, 0.0f });
   float cosine = std::clamp(safeFrom.Dot(safeTo), -1.0f, 1.0f);
   float sine = safeAxis.Dot(safeFrom.Cross(safeTo));
   if (std::abs(sine) <= 1e-5f && cosine < -0.9999f) {
      return antipodalSign < 0.0f ? -kPi : kPi;
   }
   return std::atan2(sine, cosine);
}

/// @brief 0..1 の値を端で滑らかになるS字カーブへ変換する
static float SmoothStep01(float value) {
   float t = std::clamp(value, 0.0f, 1.0f);
   return t * t * (3.0f - 2.0f * t);
}

/// @brief カメラの直交基底からワールド回転クォータニオンを構築する
/// @details ビュー行列の回転部はワールド回転の転置なので、軸を列へ格納して変換する。
static GameEngine::Quaternion MakeCameraRotationFromBasis(
   const GameEngine::Vector3& right,
   const GameEngine::Vector3& up,
   const GameEngine::Vector3& forward) {
   GameEngine::Matrix4x4 viewRotation{};
   viewRotation.m[0][0] = right.x;
   viewRotation.m[1][0] = right.y;
   viewRotation.m[2][0] = right.z;
   viewRotation.m[0][1] = up.x;
   viewRotation.m[1][1] = up.y;
   viewRotation.m[2][1] = up.z;
   viewRotation.m[0][2] = forward.x;
   viewRotation.m[1][2] = forward.y;
   viewRotation.m[2][2] = forward.z;
   viewRotation.m[3][3] = 1.0f;
   return GameEngine::MatrixToQuaternion(viewRotation);
}

/// @brief ベクトルを平面へ投影して正規化する（失敗時はフォールバックを返す）
/// @details 重力平面への水平化に使用する。投影結果が極小（up と平行）な場合は
///          fallback の投影を試み、それも失敗したら代替軸を返す。
GameEngine::Vector3 PlayerRearFollowCamera::ProjectOnPlaneNorm(const GameEngine::Vector3& v,
   const GameEngine::Vector3& up,
   const GameEngine::Vector3& fallback) {
   using namespace GameEngine;

   Vector3 proj = v - up * up.Dot(v);
   float len = proj.Length();
   if (len > 1e-4f) {
	  return proj * (1.0f / len);
   }

   // v がほぼ up と平行 → fallback を投影して使う
   Vector3 fb = fallback - up * up.Dot(fallback);
   float fbLen = fb.Length();
   if (fbLen > 1e-4f) {
	  return fb * (1.0f / fbLen);
   }

   // それも失敗（up と fallback が平行）→ 代替軸を構築して返す
   Vector3 axis = (std::abs(up.x) < 0.9f) ? Vector3{ 1.0f, 0.0f, 0.0f } : Vector3{ 0.0f, 0.0f, 1.0f };
   Vector3 out = axis - up * up.Dot(axis);
   float outLen = out.Length();
   return outLen > 1e-4f ? out * (1.0f / outLen) : Vector3{ 0.0f, 0.0f, 1.0f };
}

// =============================================================================
// プライベートメソッド実装
// =============================================================================

/// @brief 目標重力Up に向けて currentGravityUp_ を角速度制限付きで回転する
/// @details 惑星切り替え時に gravityUp_（目標）が急変しても、
///          gravityUpLerpSpeed * deltaTime を1フレームの最大回転量として
///          currentGravityUp_ を少しずつ近づけ、カメラの急なRoll変化を抑制する。
///          同じフレームのUpデルタ回転は currentBackward_ にも適用する。
GameEngine::Vector3 PlayerRearFollowCamera::SmoothGravityUp(float deltaTime) {
   using namespace GameEngine;

   // 目標 up を正規化
   Vector3 targetUp = gravityUp_;
   float targetLen = targetUp.Length();
   if (targetLen < 1e-6f) targetUp = { 0.0f, 1.0f, 0.0f };
   else targetUp = targetUp * (1.0f / targetLen);

   // 現在 up も正規化（数値誤差が蓄積しないよう毎フレーム実施）
   float curLen = currentGravityUp_.Length();
   if (curLen < 1e-6f) currentGravityUp_ = targetUp;
   else currentGravityUp_ = currentGravityUp_ * (1.0f / curLen);

   // 回転前の Up を保存しておく（後でデルタ回転を算出するため）
   Vector3 oldUp = currentGravityUp_;

   // 角速度制限付きで目標 Up へ追従（180°近傍でも破綻しない）
   float maxRadiansDelta = gravityUpLerpSpeed * (std::max)(0.0f, deltaTime);
   currentGravityUp_ = RotateTowardsUnit(oldUp, targetUp, maxRadiansDelta);

   // ─────────────────────────────────────────────────────────────
   // 【重要】今フレームの Up デルタ回転を currentBackward_ にも適用する
   //
   // なぜこれが必要か：
   //   Up が変化しても currentBackward_ を「重力平面へ投影」するだけでは
   //   "up.Cross(zaxis)" の符号が Up の通過点で反転し、
   //   カメラのロールが瞬間に跳ぶ。
   //
   //   Up の角変化（oldUp → currentGravityUp_）と同じ回転を
   //   currentBackward_ に掛け合わせることで、カメラ全体が剛体のように
   //   回転し、right の符号が反転しない。
   // ─────────────────────────────────────────────────────────────
   float cosAngle = std::clamp(oldUp.Dot(currentGravityUp_), -1.0f, 1.0f);
   if (cosAngle < 0.9999f) {
	  Vector3 rotAxis = oldUp.Cross(currentGravityUp_);
	  float axisLen = rotAxis.Length();
	  if (axisLen > 1e-6f) {
		 rotAxis = rotAxis * (1.0f / axisLen);
		 float deltaAngle = std::acos(cosAngle);

		 // ロドリゲス回転
		 float cs = std::cos(deltaAngle);
		 float sn = std::sin(deltaAngle);
		 currentBackward_ = currentBackward_ * cs
			+ rotAxis.Cross(currentBackward_) * sn
			+ rotAxis * (rotAxis.Dot(currentBackward_) * (1.0f - cs));
		 float bLen = currentBackward_.Length();
		 if (bLen > 1e-6f) currentBackward_ = currentBackward_ * (1.0f / bLen);
	  }
   }

   return currentGravityUp_;
}

/// @brief currentBackward_ を更新する
/// @details 地上では重力平面上のプレイヤー後方、空中ではプレイヤー速度の反対方向へ追従する。
///          リセット補間中はプレイヤー後方を強く採用し、空中姿勢にカメラを寄せる。
/// @param up 正規化済み補間済み重力Up
/// @param deltaTime フレーム時間
void PlayerRearFollowCamera::UpdateBackwardVector(const GameEngine::Vector3& up, float deltaTime) {
   using namespace GameEngine;

   Vector3 airborneFallbackBackward = NormalizeOrFallback(-airborneMoveForward_, currentBackward_);

   // 地上では惑星面に沿ったプレイヤー後方、空中では速度ベクトルの反対側を目標にする。
   // 機首がUpと平行に近い着地では水平投影の微小なノイズを正規化せず、
   // 前フレームの表示Rightから復元した後方を使って画面上の方位を維持する。
   Vector3 normalBackward = NormalizeOrFallback(-playerVelocity_, airborneFallbackBackward);
   if (!isAirborne_) {
      Vector3 motionBackward = ProjectOnPlaneNorm(-airborneMoveForward_, up, currentBackward_);
      Vector3 displayedBackward = ProjectOnPlaneNorm(up.Cross(cachedRight_), up, motionBackward);
      Vector3 followBackward = NormalizeOrFallback(-followForward_, displayedBackward);
      Vector3 projectedFollow = followBackward - up * up.Dot(followBackward);
      float projectedLength = projectedFollow.Length();

      if (projectedLength > 1e-5f) {
         projectedFollow = projectedFollow * (1.0f / projectedLength);
         float projectionBlend = SmoothStep01(projectedLength / kGroundDirectionProjectionBlendRange);
         float signedAngle = SignedAngleAroundAxis(displayedBackward, projectedFollow, up, 1.0f);
         normalBackward = RotateAroundAxisUnit(displayedBackward, up, signedAngle * projectionBlend);
      } else {
         normalBackward = displayedBackward;
      }
   }

   Vector3 resetBackward = NormalizeOrFallback(-playerForward_, normalBackward);
   Vector3 desiredBackward = normalBackward;
   if (isAirborne_ && airborneResetBlend_ > 1e-4f) {
	  // 原因: 空中の速度後方とリセット時のプレイヤー後方がほぼ逆向きだと、
	  //       Lerp 後の方向がゼロ付近になり、Normalize の結果でカメラ前後が反転して見える。
	  // 修正: 線形補間ではなく角度補間にして、現在の空中後方からリセット後方へ連続回転させる。
	  desiredBackward = BlendUnitDirectionSafely(normalBackward, resetBackward, airborneResetBlend_, resetBackward);
   }

   if (!isInitialized_) {
	  // 初回はスムーズ開始のためそのまま採用
	  currentBackward_ = desiredBackward;
	  isInitialized_ = true;
	  return;
   }

   if (isAirborne_) {
      // 空中では速度後方やリセット姿勢をそのまま使えるよう、重力平面へ押し戻さない。
      currentBackward_ = NormalizeOrFallback(currentBackward_, desiredBackward);
   } else {
      // 着地直後の空中後方を先に水平投影すると、垂直落下時に方位が一度で地上側へ飛ぶ。
      // 現在の3D方向を保持し、下の角度補間そのものに地上復帰を任せる。
      currentBackward_ = NormalizeOrFallback(currentBackward_, desiredBackward);
   }

   float airborneFollowSpeed = airborneForwardLerpSpeed + (airborneResetLerpSpeed - airborneForwardLerpSpeed) * airborneResetBlend_;
   float followSpeed = rearLerpSpeed;
   if (isAirborne_) {
	  followSpeed = airborneFollowSpeed;
	  lastAirborneRearFollowSpeed_ = followSpeed;
	  landingRearLerpStartSpeed_ = followSpeed;
	  landingRearLerpElapsed_ = (std::max)(0.0f, landingRearLerpRampSeconds);
   } else {
	  float rampSeconds = (std::max)(0.0f, landingRearLerpRampSeconds);
	  if (rampSeconds > 1e-4f && landingRearLerpElapsed_ < rampSeconds) {
		 // 着地した瞬間に rearLerpSpeed をそのまま使うと、地上後方へ戻る力が急に強くなり画が跳ねる。
		 // 着地直前の空中追従速度から設定値へ指定秒数で近づけ、接地直後の戻り方をなめらかにする。
		 landingRearLerpElapsed_ = (std::min)(landingRearLerpElapsed_ + (std::max)(0.0f, deltaTime), rampSeconds);
		 float rampAlpha = std::clamp(landingRearLerpElapsed_ / rampSeconds, 0.0f, 1.0f);
		 followSpeed = landingRearLerpStartSpeed_ + (rearLerpSpeed - landingRearLerpStartSpeed_) * rampAlpha;
	  }
   }
   float t = ExpSmoothingFactor(followSpeed, deltaTime);

   // 原因: 空中で速度方向が急に反対側へ変わると、currentBackward_ と desiredBackward が
   //       180°近く離れた状態で Lerp され、補間途中がゼロに近づいて前後反転が発生する。
   // 修正: 補間率 t を角度量へ変換し、RotateTowardsUnit で一方向に回すことで反転を防ぐ。
   Vector3 trackedBackward = BlendUnitDirectionSafely(currentBackward_, desiredBackward, t, desiredBackward);
   currentBackward_ = trackedBackward;
   if (!isAirborne_ && currentBackward_.Dot(desiredBackward) > 0.9999f) {
      // 収束後だけ厳密な地上後方へ確定し、長時間の数値誤差を残さない。
      currentBackward_ = desiredBackward;
   }
}

/// @brief 地上/空中ブレンド値を更新する
/// @details ジャンプ開始・着地で注視点、距離、FOV が一気に変わると画が跳ねるため、
///          共有ブレンド値を先に滑らかに動かして各パラメータへ適用する。
void PlayerRearFollowCamera::UpdateAirborneBlend(float deltaTime) {
   float targetBlend = isAirborne_ ? 1.0f : 0.0f;
   float t = ExpSmoothingFactor(airborneBlendLerpSpeed, deltaTime);
   currentAirborneBlend_ = std::clamp(
      currentAirborneBlend_ + (targetBlend - currentAirborneBlend_) * t,
      0.0f,
      1.0f);
}

/// @brief 離陸/着地に応じてプレイヤーの画面位置ブレンドを更新する
/// @details 地上下側(0)と空中中央(1)の構図だけを独立して補間し、
///          距離・FOV・惑星ガイドに使う currentAirborneBlend_ へは影響させない。
void PlayerRearFollowCamera::UpdatePlayerFramingBlend(float deltaTime) {
   float targetBlend = isAirborne_ ? 1.0f : 0.0f;
   if (std::abs(targetBlend - playerFramingBlendTarget_) > 1e-4f) {
      playerFramingBlendStart_ = currentPlayerFramingBlend_;
      playerFramingBlendTarget_ = targetBlend;
      playerFramingBlendElapsed_ = 0.0f;

      float configuredSeconds = isAirborne_
         ? (std::max)(0.0f, takeoffFramingBlendSeconds)
         : (std::max)(0.0f, landingFramingBlendSeconds);
      // 途中で状態が反転した場合は残り距離に比例して時間を短縮し、
      // 0→1 / 1→0 の全区間が設定秒数になる速度感を維持する。
      playerFramingBlendDuration_ = configuredSeconds
         * std::abs(playerFramingBlendTarget_ - playerFramingBlendStart_);
   }

   if (playerFramingBlendDuration_ <= 1e-4f) {
      currentPlayerFramingBlend_ = playerFramingBlendTarget_;
      playerFramingBlendElapsed_ = playerFramingBlendDuration_;
      return;
   }

   playerFramingBlendElapsed_ = (std::min)(
      playerFramingBlendElapsed_ + (std::max)(0.0f, deltaTime),
      playerFramingBlendDuration_);
   float progress = std::clamp(
      playerFramingBlendElapsed_ / playerFramingBlendDuration_,
      0.0f,
      1.0f);
   float easedProgress = SmoothStep01(progress);
   currentPlayerFramingBlend_ = std::clamp(
      playerFramingBlendStart_
         + (playerFramingBlendTarget_ - playerFramingBlendStart_) * easedProgress,
      0.0f,
      1.0f);
}

/// @brief 空中リセットの補間値を更新する
/// @details リセット入力を押している間だけ1へ、離したら0へ戻す。
///          値そのものを補間することで、カメラの前方向とUpの切り替えを同じ時間軸で扱う。
void PlayerRearFollowCamera::UpdateAirborneResetBlend(float deltaTime) {
   if (!isAirborne_) {
	  isAirborneResetHeld_ = false;
   }

   float targetBlend = (isAirborne_ && isAirborneResetHeld_) ? 1.0f : 0.0f;
   float t = ExpSmoothingFactor(airborneResetLerpSpeed, deltaTime);
   airborneResetBlend_ = std::clamp(
	  airborneResetBlend_ + (targetBlend - airborneResetBlend_) * t,
	  0.0f,
	  1.0f);
}

/// @brief 空中時にカメラ方向を近傍惑星側へ寄せるための方向と係数を更新する
/// @details 注視点はプレイヤー中心のまま保ち、eye 側の回り込み方向だけを補間する。
///          補間開始前は現在の後方へ張り付け、重力条件を満たした瞬間の逆振れを防ぐ。
void PlayerRearFollowCamera::UpdatePlanetDirectionGuide(float deltaTime) {
   GameEngine::Vector3 desiredBackward = currentBackward_;
   if (isAirborne_) {
	  GameEngine::Vector3 toPlanet = planetCenter_ - pivotTarget_;
	  float toPlanetLength = toPlanet.Length();
	  if (toPlanetLength > 1e-4f) {
		 desiredBackward = toPlanet * (-1.0f / toPlanetLength);
	  }
	}

	float targetFactor = ComputePlanetDirectionGravityFactorTarget();
	bool wasInactive = currentPlanetDirectionGravityFactor_ <= 1e-4f;
	float currentFollowSpeed = ComputePlanetDirectionFollowSpeed();
	float factorT = ExpSmoothingFactor(currentFollowSpeed, deltaTime);
	currentPlanetDirectionGravityFactor_ = std::clamp(
	  currentPlanetDirectionGravityFactor_ + (targetFactor - currentPlanetDirectionGravityFactor_) * factorT,
	  0.0f,
	  1.0f);

   bool wantsGuide = targetFactor > 1e-4f || currentPlanetDirectionGravityFactor_ > 1e-4f;
   if (!isPlanetBackwardInitialized_ || !wantsGuide) {
	  currentPlanetBackward_ = currentBackward_;
	  isPlanetBackwardInitialized_ = true;
	  return;
   }

   if (wasInactive) {
	  currentPlanetBackward_ = currentBackward_;
   }

	float maxRadiansDelta = currentFollowSpeed * (std::max)(0.0f, deltaTime);
	currentPlanetBackward_ = RotateTowardsUnit(currentPlanetBackward_, desiredBackward, maxRadiansDelta);
}

/// @brief 離陸後の待機と復帰を反映した惑星ガイド追従速度を計算する
/// @details 待機中は0を返す。待機終了後はSmoothstepで0から設定値へ戻し、
///          地上時は次回離陸へ影響しないよう常に設定値を返す。
float PlayerRearFollowCamera::ComputePlanetDirectionFollowSpeed() const {
	float configuredSpeed = (std::max)(0.0f, airbornePlanetDirectionLerpSpeed);
	if (!isAirborne_) {
	  return configuredSpeed;
	}

	float delaySeconds = (std::max)(0.0f, jumpPlanetDirectionDelaySeconds);
	if (jumpPlanetDirectionSpeedElapsed_ < delaySeconds) {
	  return 0.0f;
	}

	float restoreSeconds = (std::max)(0.0f, jumpPlanetDirectionRestoreSeconds);
	if (restoreSeconds <= 1e-4f) {
	  return configuredSpeed;
	}

	float restoreProgress = std::clamp(
	  (jumpPlanetDirectionSpeedElapsed_ - delaySeconds) / restoreSeconds,
	  0.0f,
	  1.0f);
	return configuredSpeed * SmoothStep01(restoreProgress);
}

/// @brief 速度方向と重力Down方向の近さから惑星方向補間の目標係数を計算する
/// @details 開始閾値と最大閾値の間を smoothstep でならし、bias で効き方を調整する。
///          開始閾値未満では惑星方向補間を始めず、最大閾値で最大係数になる。
float PlayerRearFollowCamera::ComputePlanetDirectionGravityFactorTarget() const {
   if (!enableAirbornePlanetDirectionGuide || !isAirborne_) {
	  return 0.0f;
   }

   if (!enableAirborneGravityDirectionBoost) {
	  return 1.0f;
   }

   GameEngine::Vector3 toPlanet = planetCenter_ - pivotTarget_;
   float toPlanetLength = toPlanet.Length();
   float velocityLength = playerVelocity_.Length();
   if (toPlanetLength <= 1e-4f || velocityLength <= 1e-4f) {
	  return 0.0f;
   }

   GameEngine::Vector3 gravityDown = toPlanet * (1.0f / toPlanetLength);
   GameEngine::Vector3 velocityDir = playerVelocity_ * (1.0f / velocityLength);
   float gravityAlignment = std::clamp(velocityDir.Dot(gravityDown), 0.0f, 1.0f);

   float startThreshold = std::clamp(airborneGravityDirectionBoostThreshold, 0.0f, 0.999f);
   float fullThreshold = std::clamp(airborneGravityDirectionBoostFullThreshold, 0.0f, 1.0f);
   if (fullThreshold <= startThreshold + 0.001f) {
	  fullThreshold = (std::min)(1.0f, startThreshold + 0.001f);
   }
   float normalized = std::clamp(
	  (gravityAlignment - startThreshold) / (fullThreshold - startThreshold),
	  0.0f,
	  1.0f);

   // 閾値の境界で急に効き始めないよう、S字カーブへ変換してから bias をかける。
   float smoothed = normalized * normalized * (3.0f - 2.0f * normalized);
   float bias = (std::max)(0.01f, airborneGravityDirectionBoostBias);
   return std::clamp(static_cast<float>(std::pow(smoothed, bias)), 0.0f, 1.0f);
}

/// @brief 現在の空中惑星方向補間量を返す
/// @details 基本補間量に対し、速度が惑星中心方向（重力Down方向）へ近いほど係数を上げる。
///          開始判定OFF時は係数目標を1にし、方向の立ち上がりだけは滑らかに保つ。
float PlayerRearFollowCamera::ComputePlanetDirectionBlend() const {
   if (!enableAirbornePlanetDirectionGuide) {
	  return 0.0f;
   }

	float configuredMaxBlend = std::clamp(airbornePlanetDirectionBlend, 0.0f, 1.0f);
	float cappedBlend = std::clamp(
	  currentAirborneBlend_ * (1.0f - airborneResetBlend_) * currentPlanetDirectionGravityFactor_ * configuredMaxBlend,
	  0.0f,
	  configuredMaxBlend);
   if (cappedBlend <= 0.0f) {
	  return 0.0f;
   }

   GameEngine::Vector3 toPlanet = planetCenter_ - pivotTarget_;
   float toPlanetLength = toPlanet.Length();
   if (toPlanetLength <= 1e-4f) {
	  return 0.0f;
   }

   GameEngine::Vector3 desiredPlanetBackward = toPlanet * (-1.0f / toPlanetLength);
   GameEngine::Vector3 currentBackward = NormalizeOrFallback(currentBackward_, desiredPlanetBackward);
   GameEngine::Vector3 guideBackward = NormalizeOrFallback(currentPlanetBackward_, desiredPlanetBackward);
   GameEngine::Vector3 cappedBackward = BlendUnitDirectionSafely(currentBackward, guideBackward, cappedBlend, currentBackward);
   float naturalPlanetVisibility = std::clamp(currentBackward.Dot(desiredPlanetBackward), -1.0f, 1.0f);
   float cappedPlanetVisibility = std::clamp(cappedBackward.Dot(desiredPlanetBackward), -1.0f, 1.0f);

   // 原因: 以前は惑星補間最大量と惑星方向への Dot 値を直接比較していたため、
   //       速度後方だけで既に惑星が映る場面でも最大量側へ戻す補正が発生して画が跳ねた。
   // 修正: 最大量を適用した候補方向を先に作り、自然な後方より惑星が見える場合だけ補正する。
   float visibilityGain = cappedPlanetVisibility - naturalPlanetVisibility;
   if (visibilityGain <= 1e-4f) {
	  return 0.0f;
   }

   // 改善量が 0 を跨いだ瞬間に補正をON/OFFすると小さく跳ねるため、
   // Dot の改善量も短くフェードさせ、最大量を超えない範囲で効き始めをならす。
   float usefulBlend = std::clamp(visibilityGain / kPlanetVisibilityGainFadeRange, 0.0f, 1.0f);
   usefulBlend = usefulBlend * usefulBlend * (3.0f - 2.0f * usefulBlend);
   return cappedBlend * usefulBlend;
}

/// @brief eye 位置を計算する（後退距離に加速ブーストを加味）
/// @details カメラは pivot の後方（currentBackward_ 方向）に distance 離れた場所に置く。
///          加速中は distanceBoostMax * boostAlpha 分さらに後退させることで
///          「カメラが引けて世界が広がる」視覚的な加速感を演出する。
///          空中では currentAirborneBlend_ に応じてさらに後退し、周囲を広く映す。
/// @param up カメラ位置の高さ方向
/// @param boostAlpha 加速度合い [0,1]
GameEngine::Vector3 PlayerRearFollowCamera::ComputeEye(const GameEngine::Vector3& up,
   float boostAlpha) const {
   float effectiveAirborneBlend = (std::max)(currentAirborneBlend_, airborneResetBlend_);
   GameEngine::Vector3 effectiveBackward = currentBackward_;
   float planetDirectionBlend = ComputePlanetDirectionBlend();
   if (planetDirectionBlend > 1e-4f) {
	  // 原因: 惑星方向ガイドが現在後方の反対側に近いと、Lerp した方向がゼロ付近を通り、
	  //       eye の後方ベクトルが一瞬反対側へ正規化されてカメラの前後反転に見える。
	  // 修正: 惑星補間量を角度補間へ使い、現在後方から惑星ガイド方向へ連続回転させる。
	  effectiveBackward = BlendUnitDirectionSafely(currentBackward_, currentPlanetBackward_, planetDirectionBlend, currentBackward_);
   }

   // 加速と空中状態の後退量を合成する。
   float boostedDistance =
	  distance
	  + distanceBoostMax * boostAlpha
	  + springDistanceOffset_
	  + airborneDistanceOffset * effectiveAirborneBlend;

   // eye = pivot から上方向に height、後方に boostedDistance 離れた位置
   return pivotTarget_ + up * height + effectiveBackward * boostedDistance;
}

/// @brief LookAt に使う注視点を計算する
/// @details 地上ではプレイヤー中心より少し上を狙い、画面内の地面比率を下げる。
///          空中では構図補間によりプレイヤー中心へ戻すが、着地先などへの注視切り替えは行わない。
///          リセット中も同じ注視点を使用する。
GameEngine::Vector3 PlayerRearFollowCamera::ComputeLookTarget(const GameEngine::Vector3& up) const {
   // 構図専用ブレンドを使い、距離やFOVの空中補間速度から独立して
   // 地上下側と空中中央の切り替え時間を調整できるようにする。
   float targetHeight = groundedTargetHeight * (1.0f - currentPlayerFramingBlend_);
   return pivotTarget_ + up * targetHeight;
}

/// @brief リセット状態を加味したカメラUpを返す
GameEngine::Vector3 PlayerRearFollowCamera::ComputeViewUp(const GameEngine::Vector3& gravityUp) const {
   GameEngine::Vector3 baseUp = NormalizeOrFallback(gravityUp, { 0.0f, 1.0f, 0.0f });
   if (airborneResetBlend_ <= 1e-4f) {
      return baseUp;
   }

   GameEngine::Vector3 targetUp = NormalizeOrFallback(playerUp_, baseUp);
   // 反対向きのUpをLerpすると中点でゼロになり、Normalize後の向きが1フレームで反転する。
   // 角度補間を使い、空中リセット解除から着地まで同じ回転弧を連続して辿らせる。
   return BlendUnitDirectionSafely(baseUp, targetUp, airborneResetBlend_, baseUp);
}

/// @brief LookAt 行列を構築してカメラ状態へ書き込み、キャッシュ軸を更新する
/// @details LookAt の up には通常は補間済み重力Up、空中リセット中はプレイヤーUpとの補間値を使用する。
///          これにより通常時の惑星基準と、リセット時のプレイヤー姿勢基準を滑らかに行き来できる。
///          cachedRight_ / cachedUp_ は外部（UI など）で参照されるため確定させる。
void PlayerRearFollowCamera::ApplyLookAt(GameEngine::CameraState& state,
   const GameEngine::Vector3& eye,
   const GameEngine::Vector3& up,
   float deltaTime) {
   using namespace GameEngine;

   Vector3 lookTarget = ComputeLookTarget(up);
   Vector3 requestedUp = NormalizeOrFallback(up, cachedUp_);

   // zaxis = 注視点を向く方向（カメラ前方）
   Vector3 zaxis = lookTarget - eye;
   float zLen = zaxis.Length();
   if (zLen > 1e-6f) {
	  zaxis = zaxis * (1.0f / zLen);
   } else {
      zaxis = -currentBackward_; // eye が pivot と一致する極端ケース
   }

   // 前フレームのRightを現在の視線平面へ平行移動し、特異点を跨いでも方位を維持する。
   Vector3 previousRight = cachedRight_ - zaxis * zaxis.Dot(cachedRight_);
   float previousRightLen = previousRight.Length();
   if (previousRightLen > 1e-6f) {
      previousRight = previousRight * (1.0f / previousRightLen);
   } else {
      Vector3 fallbackAxis = (std::abs(zaxis.x) < 0.9f)
         ? Vector3{ 1.0f, 0.0f, 0.0f }
         : Vector3{ 0.0f, 1.0f, 0.0f };
      previousRight = fallbackAxis - zaxis * zaxis.Dot(fallbackAxis);
      previousRight = NormalizeOrFallback(previousRight, { 1.0f, 0.0f, 0.0f });
   }

   // gravity Upから求めたRightへ戻す量は退化度に応じて連続化する。
   // これにより、垂直落下から着地してcrossが復活した瞬間の180度ロールを防ぐ。
   Vector3 candidateRight = requestedUp.Cross(zaxis);
   float candidateRightLen = candidateRight.Length();
   if (candidateRightLen > 1e-6f) {
      candidateRight = candidateRight * (1.0f / candidateRightLen);
   }

   Vector3 targetRight = previousRight;
   if (candidateRightLen > 1e-6f) {
      float normalRightBlend = SmoothStep01(candidateRightLen / kLookAtDegenerateBlendRange);
      float signedRoll = SignedAngleAroundAxis(previousRight, candidateRight, zaxis, 1.0f);
      targetRight = RotateAroundAxisUnit(previousRight, zaxis, signedRoll * normalRightBlend);
   }

   targetRight = NormalizeOrFallback(targetRight - zaxis * zaxis.Dot(targetRight), previousRight);
   Vector3 targetUp = NormalizeOrFallback(zaxis.Cross(targetRight), requestedUp);
   Quaternion targetRotation = MakeCameraRotationFromBasis(targetRight, targetUp, zaxis);

   if (!isViewRotationInitialized_) {
      currentViewRotation_ = targetRotation;
      isViewRotationInitialized_ = true;
   } else {
      float rotationT = ExpSmoothingFactor(rotationLerpSpeed, deltaTime);
      currentViewRotation_ = Quaternion::Slerp(currentViewRotation_, targetRotation, rotationT);
   }

   // 視線前方は常にプレイヤーへ向けたまま、補間済み回転のRightを再直交化する。
   // 全回転をそのまま使って注視点を遅らせず、ロールだけを時間補間できる。
   Vector3 smoothedRight = RotateVector({ 1.0f, 0.0f, 0.0f }, currentViewRotation_);
   Vector3 xaxis = smoothedRight - zaxis * zaxis.Dot(smoothedRight);
   xaxis = NormalizeOrFallback(xaxis, previousRight);
   Vector3 yaxis = NormalizeOrFallback(zaxis.Cross(xaxis), requestedUp);
   currentViewRotation_ = MakeCameraRotationFromBasis(xaxis, yaxis, zaxis);

   // キャッシュ更新
   cachedRight_ = xaxis;
   cachedUp_ = yaxis;

   state.transform.translation = eye;
   state.transform.SetRotationQuaternion(currentViewRotation_);
   Matrix4x4 view{};
   view.m[0][0] = xaxis.x;
   view.m[1][0] = xaxis.y;
   view.m[2][0] = xaxis.z;
   view.m[3][0] = -xaxis.Dot(eye);
   view.m[0][1] = yaxis.x;
   view.m[1][1] = yaxis.y;
   view.m[2][1] = yaxis.z;
   view.m[3][1] = -yaxis.Dot(eye);
   view.m[0][2] = zaxis.x;
   view.m[1][2] = zaxis.y;
   view.m[2][2] = zaxis.z;
   view.m[3][2] = -zaxis.Dot(eye);
   view.m[0][3] = 0.0f;
   view.m[1][3] = 0.0f;
   view.m[2][3] = 0.0f;
   view.m[3][3] = 1.0f;
   state.SetViewMatrix(view);
}

/// @brief プレイヤー速度に応じた FOV ブーストを補間し state.fov へ反映する
/// @details GravityFollowCamera と同じ考え方で FOV を広げて速度感を演出する。
///          加えて、boostAlpha を返すことで距離ブースト（ComputeEye）と
///          同じ速度スケールを共有できるようにする。
/// @return boostAlpha [0, 1]（加速の強さ。距離ブーストにも流用）
float PlayerRearFollowCamera::UpdateAccelerationEffect(GameEngine::CameraState& state,
   float deltaTime) {
   // 速度が [speedBoostThreshold, speedBoostMax] の範囲にどれだけ入っているかを正規化する
   float speedRange = speedBoostMax - speedBoostThreshold;
   float boostAlpha = 0.0f;
   if (speedRange > 1e-4f) {
	  boostAlpha = std::clamp((playerSpeed_ - speedBoostThreshold) / speedRange, 0.0f, 1.0f);
   }

   // ─────────────────────────────────────────────────────────────
   // インパルス方式Spring による加速キック
   //
   // 【旧設計の問題点】
   //   目標値 = speedDelta として Spring に追従させると、翌フレームでは
   //   speedDelta が 0 に戻るため Spring が即座に逆向きに引き戻され、
   //   ミニターボ解放直後の演出がほぼ 1 フレームで消えてしまっていた。
   //
   // 【インパルス方式の考え方】
   //   Spring の目標は「常に 0（自然長）」に固定する。
   //   加速が検出されたフレームだけ Spring の速度（velocity）へ直接
   //   インパルス（瞬間的な蹴り）を与える。
   //   以降は Spring の復元力と減衰だけで自然に 0 へ収束するため、
   //   ミニターボ特有の「瞬間的に広がって徐々に戻る」演出が得られる。
   // ─────────────────────────────────────────────────────────────
   float speedDelta = 0.0f;
   if (!isSpeedInitialized_) {
	  previousPlayerSpeed_ = playerSpeed_;
	  isSpeedInitialized_ = true;
   } else {
	  speedDelta = playerSpeed_ - previousPlayerSpeed_;
	  previousPlayerSpeed_ = playerSpeed_;
   }

	if (speedDelta > 0.0f) {
	  // 同じ速度差へ係数とターボ量を二重加算せず、最大キック量だけで強さを決める。
	  float kickAlpha = std::clamp(speedDelta / kSpeedDeltaForMaxKick, 0.0f, 1.0f);
	  float fovImpulse = kickAlpha * (std::max)(0.0f, speedChangeFovKickMax);
	  float distImpulse = kickAlpha * (std::max)(0.0f, speedChangeDistanceKickMax);

	  springFovVelocity_ += fovImpulse * springStiffness;
	  springDistanceVelocity_ += distImpulse * springStiffness;
	} else if (speedDelta < 0.0f && playerSpeed_ < autoSpeed_) {
	  // autoSpeed_ 以下に落ちたときのみ逆向きキックを与え、FOV を絞りつつカメラを近づける
	  // ブースト後の autoSpeed への自然回復中は発火しない
	  float decel = -speedDelta;
	  float kickAlpha = std::clamp(decel / kSpeedDeltaForMaxKick, 0.0f, 1.0f);
	  float fovImpulse = kickAlpha * (std::max)(0.0f, speedChangeFovKickMax);
	  float distImpulse = kickAlpha * (std::max)(0.0f, speedChangeDistanceKickMax);

	  springFovVelocity_ -= fovImpulse * springStiffness;
	  springDistanceVelocity_ -= distImpulse * springStiffness;
   }

   // 目標は常に 0（自然長）。Spring の復元力と減衰で収束させる。
   StepSpring1D(0.0f, springStiffness, springDamping, deltaTime, springFovOffset_, springFovVelocity_);
   StepSpring1D(0.0f, springStiffness, springDamping, deltaTime, springDistanceOffset_, springDistanceVelocity_);

   float effectiveAirborneBlend = (std::max)(currentAirborneBlend_, airborneResetBlend_);

   // 目標 FOV = 通常 FOV + 速度比例ブースト + 空中ブースト + Springキック
   float baseFov = ClampCameraFov(fovDefault);
   float targetFov =
	  baseFov
	  + std::max(0.0f, fovBoostMax) * boostAlpha
	  + std::max(0.0f, airborneFovOffset) * effectiveAirborneBlend
	  + springFovOffset_;
   targetFov = ClampCameraFov(targetFov);

   // FOV を滑らかに補間（急変させず視覚的に自然に追従）
   if (!std::isfinite(currentFov_)) {
	  currentFov_ = baseFov;
   }
   float t = ExpSmoothingFactor(fovLerpSpeed, deltaTime);
   currentFov_ = currentFov_ + (targetFov - currentFov_) * t;
   currentFov_ = ClampCameraFov(currentFov_);
   state.fov = currentFov_;

   return boostAlpha;
}

/// @brief 目標 eye オフセット（ピボット相対）の3D方向と距離を補間する
/// @details 【なぜ相対オフセットで補間するか】
///          絶対座標で補間すると、ピボット（プレイヤー）が移動するたびに
///          理想 eye も一緒にワールド空間を動く。この場合の補間パスは
///          「前フレームの eye（旧ピボット後方）→ 今フレームの eye（新ピボット後方）」
///          という直線を通るため、一瞬プレイヤーを突き抜けるルートになり得る。
///
///          現在の実装では、ピボット相対オフセットを距離と単位方向に分け、
///          距離は指数平滑、方向は180度付近でもゼロを通らない角度補間で目標へ近づける。
///          補間後はUp方向の高さ下限を適用し、カメラが基準面より下へ沈みすぎることを防ぐ。
GameEngine::Vector3 PlayerRearFollowCamera::SmoothEye(const GameEngine::Vector3& targetEye,
   const GameEngine::Vector3& up,
   float deltaTime) {
   using namespace GameEngine;

   // ピボットから見た理想オフセット
   Vector3 targetOffset = targetEye - pivotTarget_;
   Vector3 safeUp = NormalizeOrFallback(up, { 0.0f, 1.0f, 0.0f });

   if (!isEyeInitialized_) {
	  // 初回はスナップ（補間履歴なし）
	  currentEyeOffset_ = targetOffset;
	  isEyeInitialized_ = true;
	  return pivotTarget_ + currentEyeOffset_;
   }

   // ピボット相対オフセットを補間する
   float t = ExpSmoothingFactor(positionLerpSpeed, deltaTime);

   // 1. 方向と距離を3D空間で最短補間（プレイヤーの頭上を通るルートを許可）
   float currentLength = currentEyeOffset_.Length();
   float targetLength = targetOffset.Length();

   float smoothedLength = currentLength + (targetLength - currentLength) * t;

   Vector3 currentDir = currentLength > 1e-4f ? currentEyeOffset_ * (1.0f / currentLength) : safeUp;
   Vector3 targetDir = targetLength > 1e-4f ? targetOffset * (1.0f / targetLength) : safeUp;

   Vector3 smoothedDir = BlendUnitDirectionSafely(currentDir, targetDir, t, targetDir);

   // 3D補間後のオフセットを一度決定する
   currentEyeOffset_ = smoothedDir * smoothedLength;


   // 2. 現在の height をそのまま扱い、一定以下にならないように制限をかける
   float currentHeight = currentEyeOffset_.Dot(safeUp);

   // 下限値（※もしクラス内に既存の height 変数などがあれば、この数値を置き換えてください）
   float minHeightLimit = 0.5f;

   // currentHeight が制限値を下回っていたら押し上げる
   if (currentHeight < minHeightLimit) {
	  // 足りない高さの分だけ、Up方向へ加算する
	  currentEyeOffset_ += safeUp * (minHeightLimit - currentHeight);
   }

   // ワールド座標に戻して返す
   return pivotTarget_ + currentEyeOffset_;
}

/// @brief 保存データから復元すべきでないランタイム補間状態を初期化する
/// @details 設定値とは別に、実行中だけ使う補間値と初期化フラグを既定状態へ戻す。
void PlayerRearFollowCamera::ResetRuntimeState() {
   gravityUp_ = { 0.0f, 1.0f, 0.0f };
   currentGravityUp_ = gravityUp_;
   pivotTarget_ = { 0.0f, 0.0f, 0.0f };
   planetCenter_ = { 0.0f, 0.0f, 0.0f };
   followForward_ = { 0.0f, 0.0f, 1.0f };
   airborneMoveForward_ = { 0.0f, 0.0f, 1.0f };
   playerForward_ = { 0.0f, 0.0f, 1.0f };
   playerUp_ = { 0.0f, 1.0f, 0.0f };

   isAirborne_ = false;
   wasAirborneLastFrame_ = false;
   currentAirborneBlend_ = 0.0f;
   currentPlayerFramingBlend_ = 0.0f;
   playerFramingBlendStart_ = 0.0f;
   playerFramingBlendTarget_ = 0.0f;
   playerFramingBlendElapsed_ = 0.0f;
   playerFramingBlendDuration_ = 0.0f;
   isAirborneResetHeld_ = false;
   airborneResetBlend_ = 0.0f;
   currentPlanetDirectionGravityFactor_ = 0.0f;
	jumpPlanetDirectionSpeedElapsed_ = (std::max)(0.0f, jumpPlanetDirectionDelaySeconds)
	  + (std::max)(0.0f, jumpPlanetDirectionRestoreSeconds);
   currentBackward_ = { 0.0f, 0.0f, -1.0f };
   currentPlanetBackward_ = currentBackward_;
   isPlanetBackwardInitialized_ = false;
   isInitialized_ = false;
   landingRearLerpElapsed_ = (std::max)(0.0f, landingRearLerpRampSeconds);
   landingRearLerpStartSpeed_ = airborneForwardLerpSpeed;
   lastAirborneRearFollowSpeed_ = airborneForwardLerpSpeed;

   playerSpeed_ = 0.0f;
   playerVelocity_ = { 0.0f, 0.0f, 0.0f };
   currentFov_ = ClampCameraFov(fovDefault);
   springFovOffset_ = 0.0f;
   springFovVelocity_ = 0.0f;
   springDistanceOffset_ = 0.0f;
   springDistanceVelocity_ = 0.0f;
   previousPlayerSpeed_ = 0.0f;
   isSpeedInitialized_ = false;

   currentEyeOffset_ = { 0.0f, height, -distance };
   isEyeInitialized_ = false;
   cachedRight_ = { 1.0f, 0.0f, 0.0f };
   cachedUp_ = { 0.0f, 1.0f, 0.0f };
   currentViewRotation_ = GameEngine::Quaternion::Identity();
   isViewRotationInitialized_ = false;
   lastEyePlanarDirection_ = { 0.0f, 0.0f, -1.0f };
   isEyePlanarDirectionInitialized_ = false;
   eyeOrbitTurnSign_ = 1.0f;

   if (auto* owner = GetOwnerCamera()) {
	  GameEngine::CameraState state = owner->GetState();
	  state.fov = currentFov_;
	  owner->SetState(state);
   }
}

// =============================================================================
// パブリックメソッド実装
// =============================================================================

void PlayerRearFollowCamera::MutateCameraState(GameEngine::CameraState& state, float deltaTime) {
   // ① 重力Up を目標値へ向けてスムーズに補間する
   //    → 惑星切り替え時の急激なロール変化を防ぐ
   GameEngine::Vector3 up = SmoothGravityUp(deltaTime);

   // 着地した瞬間から地上用 rearLerpSpeed を使うと後方復帰が急に強くなるため、
   // 着地直前の空中追従速度を始点として、指定秒数で地上設定値へ近づける。
   if (wasAirborneLastFrame_ && !isAirborne_) {
	  landingRearLerpElapsed_ = 0.0f;
	  landingRearLerpStartSpeed_ = lastAirborneRearFollowSpeed_;
   }

	if (!wasAirborneLastFrame_ && isAirborne_) {
	  jumpPlanetDirectionSpeedElapsed_ = 0.0f;
	  currentPlanetDirectionGravityFactor_ = 0.0f;
	  currentPlanetBackward_ = currentBackward_;
	  isPlanetBackwardInitialized_ = true;
	}
	float jumpPlanetDirectionControlSeconds = (std::max)(0.0f, jumpPlanetDirectionDelaySeconds)
	  + (std::max)(0.0f, jumpPlanetDirectionRestoreSeconds);
	if (isAirborne_) {
	  // 離陸直後はガイド方向を動かさず、待機終了後に追従速度そのものを徐々に戻す。
	  jumpPlanetDirectionSpeedElapsed_ = (std::min)(
		 jumpPlanetDirectionSpeedElapsed_ + (std::max)(0.0f, deltaTime),
		 jumpPlanetDirectionControlSeconds);
	} else {
	  jumpPlanetDirectionSpeedElapsed_ = jumpPlanetDirectionControlSeconds;
	}

   // ② 地上/空中の見え方を滑らかに切り替えるブレンド値を更新する
   UpdateAirborneBlend(deltaTime);

   // プレイヤーの画面位置は離陸/着地それぞれの設定秒数で独立して切り替える
   UpdatePlayerFramingBlend(deltaTime);

   // ③ 空中リセットの補間値を更新する
   UpdateAirborneResetBlend(deltaTime);

   // ④ 後方ベクトル（currentBackward_）を更新する
   //    → 重力平面への再投影 + 角度ベースの追従を行う
   UpdateBackwardVector(up, deltaTime);

   // ⑤ 空中時に近傍惑星が画角へ入るよう、eye の回り込み方向を少しずつ補間する
   //    → 速度方向が重力Down方向へ近いほど惑星方向への補間係数が強まる
   UpdatePlanetDirectionGuide(deltaTime);

   // ⑥ プレイヤー速度に応じた FOV ブーストを計算し boostAlpha を取得する
   //    → FOV は state.fov へ反映済み、boostAlpha は距離ブーストに転用する
   float boostAlpha = UpdateAccelerationEffect(state, deltaTime);

   // ⑦ リセット時はカメラUpもプレイヤーUpへ補間する
   GameEngine::Vector3 viewUp = ComputeViewUp(up);

   // ⑧ 加速ブーストと空中距離を加味した eye 位置を算出する
   //    → 加速中はカメラが後退して視野が広がり、速度感が増す
   GameEngine::Vector3 eye = ComputeEye(viewUp, boostAlpha);

   // ⑨ eye 位置をピボット相対オフセットで補間し、急激なテレポートを防ぐ
   //    → 相対補間により、ピボットが移動してもカメラが前方へ突き抜けない
   eye = SmoothEye(eye, viewUp, deltaTime);

   // ⑩ LookAt 行列を構築してカメラ状態へ反映する（補間済み eye を使用）
   ApplyLookAt(state, eye, viewUp, deltaTime);

   wasAirborneLastFrame_ = isAirborne_;
}

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
	   { "airborneForwardLerpSpeed", airborneForwardLerpSpeed },
	   { "airborneResetLerpSpeed", airborneResetLerpSpeed },
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
      { "rotationLerpSpeed", rotationLerpSpeed },
   };
}

void PlayerRearFollowCamera::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
	  return;
   }

   distance = ReadFloat(data, "distance", distance);
   height = ReadFloat(data, "height", height);
   groundedTargetHeight = ReadFloat(data, "groundedTargetHeight", groundedTargetHeight);
   takeoffFramingBlendSeconds = (std::max)(
      0.0f,
      ReadFloat(data, "takeoffFramingBlendSeconds", takeoffFramingBlendSeconds));
   landingFramingBlendSeconds = (std::max)(
      0.0f,
      ReadFloat(data, "landingFramingBlendSeconds", landingFramingBlendSeconds));
   airborneDistanceOffset = ReadFloat(data, "airborneDistanceOffset", airborneDistanceOffset);
   airborneFovOffset = ReadFloat(data, "airborneFovOffset", airborneFovOffset);
   airbornePlanetDirectionBlend = ReadFloat(data, "airbornePlanetDirectionBlend", airbornePlanetDirectionBlend);
   airbornePlanetDirectionLerpSpeed = ReadFloat(data, "airbornePlanetDirectionLerpSpeed", airbornePlanetDirectionLerpSpeed);
	jumpPlanetDirectionDelaySeconds = (std::max)(
	  0.0f,
	  ReadFloat(
		 data,
		 "jumpPlanetDirectionDelaySeconds",
		 ReadFloat(data, "jumpPlanetDirectionRampSeconds", jumpPlanetDirectionDelaySeconds)));
	jumpPlanetDirectionRestoreSeconds = (std::max)(
	  0.0f,
	  ReadFloat(data, "jumpPlanetDirectionRestoreSeconds", jumpPlanetDirectionRestoreSeconds));
   enableAirbornePlanetDirectionGuide = ReadBool(data, "enableAirbornePlanetDirectionGuide", enableAirbornePlanetDirectionGuide);
   enableAirborneGravityDirectionBoost = ReadBool(data, "enableAirborneGravityDirectionBoost", enableAirborneGravityDirectionBoost);
   airborneGravityDirectionBoostThreshold = ReadFloat(data, "airborneGravityDirectionBoostThreshold", airborneGravityDirectionBoostThreshold);
   airborneGravityDirectionBoostFullThreshold = ReadFloat(data, "airborneGravityDirectionBoostFullThreshold", airborneGravityDirectionBoostFullThreshold);
   airborneGravityDirectionBoostBias = ReadFloat(data, "airborneGravityDirectionBoostBias", airborneGravityDirectionBoostBias);
   airborneBlendLerpSpeed = ReadFloat(data, "airborneBlendLerpSpeed", airborneBlendLerpSpeed);
   airborneForwardLerpSpeed = ReadFloat(data, "airborneForwardLerpSpeed", airborneForwardLerpSpeed);
   airborneResetLerpSpeed = ReadFloat(data, "airborneResetLerpSpeed", airborneResetLerpSpeed);
   rearLerpSpeed = ReadFloat(data, "rearLerpSpeed", rearLerpSpeed);
   landingRearLerpRampSeconds = ReadFloat(data, "landingRearLerpRampSeconds", landingRearLerpRampSeconds);
   gravityUpLerpSpeed = ReadFloat(data, "gravityUpLerpSpeed", gravityUpLerpSpeed);
   fovDefault = ClampCameraFov(ReadFloat(data, "fovDefault", fovDefault));
   fovBoostMax = (std::max)(0.0f, ReadFloat(data, "fovBoostMax", fovBoostMax));
   fovLerpSpeed = ReadFloat(data, "fovLerpSpeed", fovLerpSpeed);
   distanceBoostMax = ReadFloat(data, "distanceBoostMax", distanceBoostMax);
   springStiffness = ReadFloat(data, "springStiffness", springStiffness);
   springDamping = ReadFloat(data, "springDamping", springDamping);
	// 旧データでは同じ速度差に「加速度係数」と「ターボ最大量」を重ねていた。
	// 最大キックに到達する速度差で両者が作る量を合算し、新しい1項目へ移行する。
	float legacyTurboFovKickMax = (std::max)(0.0f, ReadFloat(data, "turboFovKickMax", speedChangeFovKickMax));
	float legacyTurboDistanceKickMax = (std::max)(0.0f, ReadFloat(data, "turboDistanceKickMax", speedChangeDistanceKickMax));
	float legacyFovKickMax = (std::min)(
	   legacyTurboFovKickMax + (std::max)(0.0f, ReadFloat(data, "accelToFovKick", 0.0f)) * kSpeedDeltaForMaxKick,
	   (std::max)(0.0f, fovBoostMax) + legacyTurboFovKickMax);
	float legacyDistanceKickMax = (std::min)(
	   legacyTurboDistanceKickMax + (std::max)(0.0f, ReadFloat(data, "accelToDistanceKick", 0.0f)) * kSpeedDeltaForMaxKick,
	   (std::max)(0.0f, distanceBoostMax) + legacyTurboDistanceKickMax);
	speedChangeFovKickMax = (std::max)(0.0f, ReadFloat(data, "speedChangeFovKickMax", legacyFovKickMax));
	speedChangeDistanceKickMax = (std::max)(0.0f, ReadFloat(data, "speedChangeDistanceKickMax", legacyDistanceKickMax));
   speedBoostThreshold = ReadFloat(data, "speedBoostThreshold", speedBoostThreshold);
   speedBoostMax = ReadFloat(data, "speedBoostMax", speedBoostMax);
   positionLerpSpeed = ReadFloat(data, "positionLerpSpeed", positionLerpSpeed);
   rotationLerpSpeed = ReadFloat(data, "rotationLerpSpeed", rotationLerpSpeed);
   autoSpeed_ = ReadFloat(data, "autoSpeed", autoSpeed_);

   ResetRuntimeState();
}

#ifdef USE_IMGUI
void PlayerRearFollowCamera::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   auto DrawHelp = [Tr](const char* japanese, const char* english) {
      ImGui::SameLine();
      ImGui::TextDisabled("(?)");
      if (ImGui::IsItemHovered()) {
         ImGui::SetTooltip("%s", Tr(japanese, english));
      }
   };

   if (ImGui::Checkbox(Tr("有効", "Enabled"), &isEnabled_)) {}

   ImGui::TextDisabled("%s", Tr("(?) にカーソルを合わせると役割を表示します。補間速度は大きいほど速く、秒数は大きいほど遅く変化します。",
      "Hover (?) for details. Larger blend speeds react faster; larger durations change more slowly."));

   if (ImGui::CollapsingHeader(Tr("基本構図", "Base Framing"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat(Tr("基準後方距離", "Base Rear Distance"), &distance, 0.1f, 1.0f, 100.0f);
      DrawHelp("通常時にプレイヤーからカメラまで離す後方距離です。空中距離や速度演出の加算前の基準値です。",
         "Base distance behind the player before airborne and speed-effect offsets are added.");
      ImGui::DragFloat(Tr("カメラ高さ", "Camera Height"), &height, 0.1f, -20.0f, 50.0f);
      DrawHelp("カメラ位置を現在のUp方向へずらす量です。注視点の高さではありません。",
         "Moves the camera position along the current Up direction; it does not move the look target.");
      ImGui::DragFloat(Tr("地上注視点高さ", "Grounded Look Target Height"), &groundedTargetHeight, 0.05f, -5.0f, 10.0f);
      DrawHelp("地上でカメラが見る点をプレイヤー中心から上へずらします。値を上げるとプレイヤーが画面下側に寄ります。",
         "Raises the grounded look target above the player center, placing the player lower in the frame.");
      ImGui::DragFloat(Tr("離陸構図切替秒", "Takeoff Framing Duration"), &takeoffFramingBlendSeconds, 0.01f, 0.0f, 5.0f, "%.2f");
      DrawHelp("離陸時に、地上の画面位置から空中の中央構図へ切り替える時間です。距離やFOVには影響しません。",
         "Time to move from grounded framing to centered airborne framing; it does not affect distance or FOV.");
      ImGui::DragFloat(Tr("着地構図切替秒", "Landing Framing Duration"), &landingFramingBlendSeconds, 0.01f, 0.0f, 5.0f, "%.2f");
      DrawHelp("着地時に、空中の中央構図から地上の画面位置へ戻す時間です。",
         "Time to return from centered airborne framing to grounded framing.");
   }

   if (ImGui::CollapsingHeader(Tr("空中の見え方", "Airborne Presentation"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat(Tr("空中時の追加距離", "Airborne Extra Distance"), &airborneDistanceOffset, 0.1f, 0.0f, 30.0f);
      DrawHelp("空中で基準後方距離へ加える量です。周囲を広く見せます。",
         "Extra rear distance added while airborne to show more surroundings.");
      ImGui::DragFloat(Tr("空中時の追加FOV", "Airborne Extra FOV"), &airborneFovOffset, 0.001f, 0.0f, 0.5f, "%.3f");
      DrawHelp("空中で通常FOVへ加える量です。速度によるFOV演出とは別です。",
         "FOV added while airborne, independently of speed-based FOV effects.");
      ImGui::DragFloat(Tr("空中表示切替速度", "Airborne State Blend Speed"), &airborneBlendLerpSpeed, 0.1f, 0.1f, 30.0f);
      DrawHelp("空中距離・空中FOV・惑星ガイドの有効量を地上と空中の間で切り替える速度です。画面内のプレイヤー位置は構図切替秒で調整します。",
         "Blends airborne distance, FOV, and planet-guide strength. Player screen placement uses the framing durations instead.");
   }

   if (ImGui::CollapsingHeader(Tr("空中の惑星方向ガイド", "Airborne Planet Guide"))) {
      ImGui::Checkbox(Tr("惑星方向ガイドを使う", "Enable Planet Direction Guide"), &enableAirbornePlanetDirectionGuide);
      DrawHelp("空中でカメラ位置を近傍惑星と反対側へ少し回り込ませ、プレイヤーの奥に惑星を映しやすくします。注視点はプレイヤーのままです。",
         "Orbits the camera slightly away from the nearby planet so the planet is easier to see behind the player; the player remains the look target.");
      ImGui::DragFloat(Tr("惑星方向への最大補正率", "Maximum Planet Correction"), &airbornePlanetDirectionBlend, 0.01f, 0.0f, 1.0f, "%.2f");
      DrawHelp("速度後方から惑星を映す方向へ寄せる上限です。0で補正なし、1でガイド方向まで寄せます。",
         "Maximum blend from velocity-rear direction toward the planet-guide direction: 0 disables correction, 1 allows full correction.");
	  ImGui::DragFloat(Tr("惑星ガイド追従速度", "Planet Guide Follow Speed"), &airbornePlanetDirectionLerpSpeed, 0.1f, 0.0f, 30.0f);
	  DrawHelp("惑星ガイドの方向と、重力方向による有効係数が変化へ追従する通常時の速度です。離陸後は停止・復帰設定に従って0からこの値へ戻ります。",
		 "Normal follow speed for the guide direction and its gravity-gated strength. After takeoff, delay and restore settings bring speed back from zero to this value.");
	  ImGui::DragFloat(Tr("離陸後ガイド停止秒", "Post-takeoff Guide Delay"), &jumpPlanetDirectionDelaySeconds, 0.01f, 0.0f, 5.0f, "%.2f");
	  DrawHelp("離陸直後に惑星ガイド追従速度を0へ保つ時間です。この間、惑星ガイドはカメラ方向を動かしません。",
		 "Time after takeoff during which planet-guide follow speed remains zero and cannot move the camera direction.");
	  ImGui::DragFloat(Tr("惑星ガイド速度復帰秒", "Planet Guide Speed Restore"), &jumpPlanetDirectionRestoreSeconds, 0.01f, 0.0f, 5.0f, "%.2f");
	  DrawHelp("停止時間が終わってから、惑星ガイド追従速度を0から設定値まで滑らかに戻す時間です。0なら即座に戻ります。",
		 "Time used after the delay to restore planet-guide follow speed smoothly from zero to its configured value; zero restores immediately.");
      ImGui::Checkbox(Tr("落下方向のときだけ使う", "Gate Guide by Falling Direction"), &enableAirborneGravityDirectionBoost);
      DrawHelp("有効時は、速度が惑星中心方向に近いときだけ惑星ガイドを効かせます。無効時は空中で常に候補になります。",
         "When enabled, the guide acts only while velocity points toward the planet center; otherwise it is always eligible in air.");
      ImGui::DragFloat(Tr("ガイド開始方向一致度", "Guide Start Alignment"), &airborneGravityDirectionBoostThreshold, 0.01f, 0.0f, 1.0f, "%.2f");
      DrawHelp("速度方向と惑星中心方向の内積がこの値を超えると、ガイドが効き始めます。0は直交、1は完全に同方向です。",
         "The guide starts when velocity alignment with the planet-center direction exceeds this dot-product value; 0 is perpendicular and 1 is identical.");
      ImGui::DragFloat(Tr("ガイド最大方向一致度", "Guide Full Alignment"), &airborneGravityDirectionBoostFullThreshold, 0.01f, 0.0f, 1.0f, "%.2f");
      DrawHelp("方向一致度がこの値に達すると、重力方向による係数が最大になります。開始値より大きくしてください。",
         "Alignment at which the gravity-gated factor reaches full strength; set it above the start value.");
      ImGui::DragFloat(Tr("ガイド強度カーブ", "Guide Strength Curve"), &airborneGravityDirectionBoostBias, 0.05f, 0.01f, 5.0f, "%.2f");
      DrawHelp("開始から最大までの効き方です。1が標準、1より大きいと後半で強まり、1より小さいと早めに強まります。",
         "Shapes the transition from start to full: 1 is neutral, above 1 delays strength, below 1 brings it in earlier.");
   }

   if (ImGui::CollapsingHeader(Tr("向きの追従", "Direction Tracking"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat(Tr("空中速度後方の追従速度", "Airborne Velocity-rear Follow"), &airborneForwardLerpSpeed, 0.1f, 0.0f, 30.0f);
      DrawHelp("空中でカメラの理想後方を、プレイヤー速度の反対方向へ回す速さです。カメラ位置全体の追従とは別です。",
         "How quickly the ideal rear direction turns opposite the player's velocity; separate from final camera-position smoothing.");
      ImGui::DragFloat(Tr("空中リセット姿勢の追従速度", "Airborne Reset Follow"), &airborneResetLerpSpeed, 0.1f, 0.1f, 30.0f);
      DrawHelp("リセット入力中にカメラ後方とUpをプレイヤー姿勢へ寄せ、入力解除時に戻す速さです。",
         "How quickly camera rear and Up move toward the player basis while reset is held, and return after release.");
      ImGui::DragFloat(Tr("地上後方の追従速度", "Grounded Rear Follow"), &rearLerpSpeed, 0.1f, 0.0f, 30.0f);
      DrawHelp("地上でカメラの理想後方をプレイヤー正面の反対へ回す速さです。",
         "How quickly the ideal rear direction turns behind the player's facing direction on the ground.");
      ImGui::DragFloat(Tr("着地後方速度の切替秒", "Landing Rear-speed Transition"), &landingRearLerpRampSeconds, 0.01f, 0.0f, 5.0f, "%.2f");
      DrawHelp("着地直前の空中後方追従速度から、地上後方追従速度へ切り替える時間です。",
         "Time to transition from the airborne rear-follow speed to the grounded rear-follow speed after landing.");
      ImGui::DragFloat(Tr("重力Upの最大追従角速度", "Gravity Up Angular Speed"), &gravityUpLerpSpeed, 0.1f, 0.1f, 30.0f);
      DrawHelp("惑星切替などで重力Upが変わったとき、カメラ基準Upを追従させる最大角速度（rad/s）です。最終ロール補間とは別です。",
         "Maximum angular speed (rad/s) used to follow a changing gravity Up; separate from final roll smoothing.");
   }

   if (ImGui::CollapsingHeader(Tr("速度演出", "Speed Effects"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat(Tr("通常FOV", "Base FOV"), &fovDefault, 0.001f, 0.1f, 1.5f, "%.3f");
      DrawHelp("空中・速度演出を加える前の基準FOVです。",
         "Base FOV before airborne and speed effects are added.");
      ImGui::DragFloat(Tr("高速時の追加FOV最大", "High-speed Extra FOV"), &fovBoostMax, 0.001f, 0.0f, 0.5f, "%.3f");
      DrawHelp("速度しきい値を超えて走り続けている間に加えるFOVの最大量です。瞬間キックとは異なる持続演出です。",
         "Maximum sustained FOV added while speed remains high; unlike the momentary kick, this persists.");
      ImGui::DragFloat(Tr("FOV出力追従速度", "FOV Output Follow Speed"), &fovLerpSpeed, 0.1f, 0.1f, 20.0f);
      DrawHelp("通常・空中・高速・瞬間キックを合成した目標FOVへ、最終FOVが追従する速度です。",
         "How quickly final FOV follows the target composed from base, airborne, high-speed, and kick effects.");
      ImGui::DragFloat(Tr("高速時の追加距離最大", "High-speed Extra Distance"), &distanceBoostMax, 0.1f, 0.0f, 30.0f);
      DrawHelp("速度しきい値を超えて走り続けている間に追加する後方距離の最大量です。瞬間キックとは異なる持続演出です。",
         "Maximum sustained rear distance added while speed remains high; separate from the momentary kick.");
      ImGui::DragFloat(Tr("速度変化FOVキック最大", "Speed-change FOV Kick Max"), &speedChangeFovKickMax, 0.001f, 0.0f, 0.3f, "%.3f");
      DrawHelp("急加速・急減速した瞬間にばねへ与えるFOV変化の最大量です。旧『加速→FOVキック』と『ターボFOVキック』を統合した項目です。",
         "Maximum momentary FOV impulse on sudden acceleration or deceleration; replaces the former coefficient and turbo-kick pair.");
      ImGui::DragFloat(Tr("速度変化距離キック最大", "Speed-change Distance Kick Max"), &speedChangeDistanceKickMax, 0.05f, 0.0f, 8.0f);
      DrawHelp("急加速・急減速した瞬間にばねへ与える後方距離変化の最大量です。旧2項目を統合しています。",
         "Maximum momentary rear-distance impulse on sudden acceleration or deceleration; replaces the former two controls.");
      ImGui::DragFloat(Tr("キックばね剛性", "Kick Spring Stiffness"), &springStiffness, 1.0f, 1.0f, 300.0f);
      DrawHelp("瞬間キックを元のFOV・距離へ引き戻すばねの強さです。大きいほど戻す力が強くなります。",
         "Restoring force that pulls momentary FOV and distance kicks back to zero; larger values pull harder.");
      ImGui::DragFloat(Tr("キックばね減衰", "Kick Spring Damping"), &springDamping, 0.5f, 0.0f, 100.0f);
      DrawHelp("瞬間キックの揺れを減らす強さです。大きいほど振動が早く収まります。",
         "Damping applied to momentary kicks; larger values settle oscillation sooner.");
      ImGui::DragFloat(Tr("高速演出開始速度", "High-speed Effect Start"), &speedBoostThreshold, 0.5f, 0.0f, 100.0f);
      DrawHelp("持続する高速FOV・距離演出が効き始めるプレイヤー速度です。",
         "Player speed at which sustained high-speed FOV and distance effects begin.");
      ImGui::DragFloat(Tr("高速演出最大速度", "High-speed Effect Full Speed"), &speedBoostMax, 0.5f, 0.0f, 200.0f);
      DrawHelp("持続する高速FOV・距離演出が最大になるプレイヤー速度です。開始速度より大きくしてください。",
         "Player speed at which sustained high-speed effects reach full strength; set it above the start speed.");
   }

   if (ImGui::CollapsingHeader(Tr("最終出力の安定化", "Final Output Stabilization"), ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat(Tr("最終カメラ位置の追従速度", "Final Camera Position Follow"), &positionLerpSpeed, 0.5f, 1.0f, 100.0f);
      DrawHelp("各方向・距離を合成した最終カメラ位置を滑らかにする安全段です。後方追従は理想方向を決め、この項目は実際のeye位置を追従させます。",
         "Final safety smoothing after all directions and distances are composed. Rear-follow controls the ideal direction; this tracks the actual eye position.");
      ImGui::DragFloat(Tr("最終ロールの追従速度", "Final Roll Follow"), &rotationLerpSpeed, 0.5f, 0.1f, 100.0f);
      DrawHelp("カメラの視線をプレイヤーへ固定したまま、最終的なRight/Up（ロール）だけを滑らかにします。重力Up補間とは処理段階が異なります。",
         "Smooths only final Right/Up roll while keeping the view aimed at the player; it is downstream from gravity-Up tracking.");
   }

   ImGui::Separator();
   ImGui::Text("%s: %s", Tr("空中", "Airborne"), isAirborne_ ? Tr("はい", "true") : Tr("いいえ", "false"));
   ImGui::Text("%s: %.2f", Tr("空中ブレンド", "Airborne Blend"), currentAirborneBlend_);
   ImGui::Text("%s: %.2f", Tr("画面位置ブレンド", "Player Framing Blend"), currentPlayerFramingBlend_);
   ImGui::Text("%s: %s / %.2f", Tr("空中リセット", "Airborne Reset"), isAirborneResetHeld_ ? Tr("押下中", "held") : Tr("なし", "none"), airborneResetBlend_);
	ImGui::Text("%s: %.2f", Tr("惑星補間重力係数", "Planet Blend Gravity Factor"), currentPlanetDirectionGravityFactor_);
	ImGui::Text("%s: %.2f", Tr("現在の惑星ガイド追従速度", "Current Planet Guide Follow Speed"), ComputePlanetDirectionFollowSpeed());
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("目標GravityUp", "Target GravityUp"), gravityUp_.x, gravityUp_.y, gravityUp_.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("現在GravityUp", "Current GravityUp"), currentGravityUp_.x, currentGravityUp_.y, currentGravityUp_.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("追従前方向", "Follow Forward"), followForward_.x, followForward_.y, followForward_.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("プレイヤー速度ベクトル", "Player Velocity"), playerVelocity_.x, playerVelocity_.y, playerVelocity_.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("空中補助進行方向", "Air Move Fallback"), airborneMoveForward_.x, airborneMoveForward_.y, airborneMoveForward_.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("注視惑星中心", "Look Planet Center"), planetCenter_.x, planetCenter_.y, planetCenter_.z);
   ImGui::Text("%s: %.2f  %s: %.3f", Tr("プレイヤー速度", "Player Speed"), playerSpeed_, Tr("現在FOV", "Current FOV"), currentFov_);
}
#endif

} // namespace App

