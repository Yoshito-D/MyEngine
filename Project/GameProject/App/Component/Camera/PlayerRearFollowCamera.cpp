#include "PlayerRearFollowCamera.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "Utility/MathUtils/MatrixOperations.h"
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
   float dt = (std::max)(0.0f, deltaTime);
   if (dt <= 0.0f) return;

   float k = (std::max)(0.0f, stiffness);
   float c = (std::max)(0.0f, damping);

   float accel = -k * (inOutValue - target) - c * inOutVelocity;
   inOutVelocity += accel * dt;
   inOutValue += inOutVelocity * dt;
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

/// @brief 目標重力Up に向けて currentGravityUp_ を nlerp 補間する
/// @details 惑星切り替え時に gravityUp_（目標）が急変しても、
///          currentGravityUp_ をフレームごとに少しずつ近づけることで
///          カメラの Roll が急激にジャンプする問題を抑制する。
///          nlerp = Lerp 後に正規化。slerp より軽量で視覚的品質も十分。
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
/// @details 地上ではプレイヤー正面、空中ではプレイヤー速度の反対方向へ追従する。
///          リセット補間中はプレイヤー正面を強く採用し、空中姿勢にカメラを寄せる。
/// @param up 正規化済み補間済み重力Up
/// @param deltaTime フレーム時間
void PlayerRearFollowCamera::UpdateBackwardVector(const GameEngine::Vector3& up, float deltaTime) {
   using namespace GameEngine;

   Vector3 airborneFallbackBackward = NormalizeOrFallback(-airborneMoveForward_, currentBackward_);

   // 地上では惑星面に沿ったプレイヤー後方、空中では速度ベクトルの反対側を目標にする。
   // 速度がほぼゼロの瞬間は方向が定まらないため、橋渡し側で作った水平進行方向をフォールバックにする。
   Vector3 normalBackward = isAirborne_
	  ? NormalizeOrFallback(-playerVelocity_, airborneFallbackBackward)
	  : ProjectOnPlaneNorm(-followForward_, up, currentBackward_);

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
	  // 地上では gravityUp が変化しても水平性が保たれるよう、毎フレーム平面へ戻す。
	  currentBackward_ = ProjectOnPlaneNorm(currentBackward_, up, desiredBackward);
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
   currentBackward_ = isAirborne_ ? trackedBackward : ProjectOnPlaneNorm(trackedBackward, up, desiredBackward);
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
   float factorT = ExpSmoothingFactor(airbornePlanetDirectionLerpSpeed, deltaTime);
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

   float maxRadiansDelta = (std::max)(0.0f, airbornePlanetDirectionLerpSpeed) * (std::max)(0.0f, deltaTime);
   currentPlanetBackward_ = RotateTowardsUnit(currentPlanetBackward_, desiredBackward, maxRadiansDelta);
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
   float jumpRampSeconds = (std::max)(0.0f, jumpPlanetDirectionRampSeconds);
   float jumpRampAlpha = 1.0f;
   if (jumpRampSeconds > 1e-4f) {
	  jumpRampAlpha = std::clamp(jumpPlanetDirectionRampElapsed_ / jumpRampSeconds, 0.0f, 1.0f);
   }

   float cappedBlend = std::clamp(
	  currentAirborneBlend_ * (1.0f - airborneResetBlend_) * currentPlanetDirectionGravityFactor_ * jumpRampAlpha * configuredMaxBlend,
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
///          空中では currentAirborneBlend_ に応じてさらに後退し、着地先を見やすくする。
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
///          空中では車体姿勢や着地先を読みやすくするため、補間しながら中心へ戻す。
///          リセット中も空中扱いとして、プレイヤー中心を保つ。
GameEngine::Vector3 PlayerRearFollowCamera::ComputeLookTarget(const GameEngine::Vector3& up) const {
   float effectiveAirborneBlend = (std::max)(currentAirborneBlend_, airborneResetBlend_);
   float targetHeight = groundedTargetHeight * (1.0f - effectiveAirborneBlend);
   return pivotTarget_ + up * targetHeight;
}

/// @brief リセット状態を加味したカメラUpを返す
GameEngine::Vector3 PlayerRearFollowCamera::ComputeViewUp(const GameEngine::Vector3& gravityUp) const {
   GameEngine::Vector3 baseUp = NormalizeOrFallback(gravityUp, { 0.0f, 1.0f, 0.0f });
   if (airborneResetBlend_ <= 1e-4f) {
	  return baseUp;
   }

   GameEngine::Vector3 targetUp = NormalizeOrFallback(playerUp_, baseUp);
   return NormalizeOrFallback(
	  GameEngine::Vector3::Lerp(baseUp, targetUp, airborneResetBlend_),
	  targetUp);
}

/// @brief LookAt 行列を構築してカメラ状態へ書き込み、キャッシュ軸を更新する
/// @details LookAt の up には通常は補間済み重力Up、空中リセット中はプレイヤーUpとの補間値を使用する。
///          これにより通常時の惑星基準と、リセット時のプレイヤー姿勢基準を滑らかに行き来できる。
///          cachedRight_ / cachedUp_ は外部（UI など）で参照されるため確定させる。
void PlayerRearFollowCamera::ApplyLookAt(GameEngine::CameraState& state,
   const GameEngine::Vector3& eye,
   const GameEngine::Vector3& up) {
   using namespace GameEngine;

   Vector3 lookTarget = ComputeLookTarget(up);

   // zaxis = 注視点を向く方向（カメラ前方）
   Vector3 zaxis = lookTarget - eye;
   float zLen = zaxis.Length();
   if (zLen > 1e-6f) {
	  zaxis = zaxis * (1.0f / zLen);
   } else {
	  zaxis = -currentBackward_; // eye が pivot と一致する極端ケース
   }

   // 原因: 空中で落下/上昇方向を追うと視線 zaxis と up が平行に近づき、
   //       up.Cross(zaxis) が極小になって LookAt のロールが数学的に不定になる。
   //       さらに MakeLookAtMatrix 内でも同じ cross を再計算するため、ここで cachedRight_ を
   //       用意しても実際のビュー行列には退化対策が反映されず、急なロール回転として見えていた。
   // 修正: 前フレームの right を現在の視線平面へ投影して非常用の right とし、
   //       退化付近ではその right を優先して、安定した直交基底からビュー行列を直接組み立てる。
   Vector3 candidateRight = up.Cross(zaxis);
   float candidateRightLen = candidateRight.Length();
   if (candidateRightLen > 1e-6f) {
	  candidateRight = candidateRight * (1.0f / candidateRightLen);
   }

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

   Vector3 xaxis = previousRight;
   if (candidateRightLen > 1e-6f) {
	  float normalRightBlend = std::clamp(candidateRightLen / 0.15f, 0.0f, 1.0f);
	  xaxis = BlendUnitDirectionSafely(previousRight, candidateRight, normalRightBlend, previousRight);
   }
   xaxis = NormalizeOrFallback(xaxis - zaxis * zaxis.Dot(xaxis), previousRight);
   Vector3 yaxis = NormalizeOrFallback(zaxis.Cross(xaxis), up);

   // キャッシュ更新
   cachedRight_ = xaxis;
   cachedUp_ = yaxis;

   state.transform.translation = eye;
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
	  // 加速度に比例したキック量を速度（velocity）へ直接加算する
	  float turboAlpha = std::clamp(speedDelta * 0.2f, 0.0f, 1.0f);
	  float fovImpulse = speedDelta * accelToFovKick + turboAlpha * turboFovKickMax;
	  float distImpulse = speedDelta * accelToDistanceKick + turboAlpha * turboDistanceKickMax;

	  springFovVelocity_ += std::clamp(fovImpulse, 0.0f, fovBoostMax + turboFovKickMax) * springStiffness;
	  springDistanceVelocity_ += std::clamp(distImpulse, 0.0f, distanceBoostMax + turboDistanceKickMax) * springStiffness;
   } else if (speedDelta < 0.0f && playerSpeed_ < autoSpeed_) {
	  // autoSpeed_ 以下に落ちたときのみ逆向きキックを与え、FOV を絞りつつカメラを近づける
	  // ブースト後の autoSpeed への自然回復中は発火しない
	  float decel = -speedDelta;
	  float turboAlpha = std::clamp(decel * 0.2f, 0.0f, 1.0f);
	  float fovImpulse = decel * accelToFovKick + turboAlpha * turboFovKickMax;
	  float distImpulse = decel * accelToDistanceKick + turboAlpha * turboDistanceKickMax;

	  springFovVelocity_ -= std::clamp(fovImpulse, 0.0f, fovBoostMax + turboFovKickMax) * springStiffness;
	  springDistanceVelocity_ -= std::clamp(distImpulse, 0.0f, distanceBoostMax + turboDistanceKickMax) * springStiffness;
   }

   // 目標は常に 0（自然長）。Spring の復元力と減衰で収束させる。
   StepSpring1D(0.0f, springStiffness, springDamping, deltaTime, springFovOffset_, springFovVelocity_);
   StepSpring1D(0.0f, springStiffness, springDamping, deltaTime, springDistanceOffset_, springDistanceVelocity_);

   float effectiveAirborneBlend = (std::max)(currentAirborneBlend_, airborneResetBlend_);

   // 目標 FOV = 通常 FOV + 速度比例ブースト + 空中ブースト + Springキック
   float targetFov =
	  fovDefault
	  + fovBoostMax * boostAlpha
	  + airborneFovOffset * effectiveAirborneBlend
	  + springFovOffset_;

   // FOV を滑らかに補間（急変させず視覚的に自然に追従）
   float t = ExpSmoothingFactor(fovLerpSpeed, deltaTime);
   currentFov_ = currentFov_ + (targetFov - currentFov_) * t;
   state.fov = currentFov_;

   return boostAlpha;
}

/// @brief 目標 eye オフセット（ピボット相対）をUp基準の高さ・水平角・半径に分けて補間する
/// @details 【なぜ相対オフセットで補間するか】
///          絶対座標で補間すると、ピボット（プレイヤー）が移動するたびに
///          理想 eye も一緒にワールド空間を動く。この場合の補間パスは
///          「前フレームの eye（旧ピボット後方）→ 今フレームの eye（新ピボット後方）」
///          という直線を通るため、一瞬プレイヤーを突き抜けるルートになり得る。
///
///          ただし3D方向としてそのままslerpすると、着地時に空中速度後方から地上後方へ戻る際、
///          補間軸が重力Upと無関係になってカメラが縦に大きく回り込む。
///          Up方向の高さと水平面上の角度・半径を分けて補間し、着地時の一回転を防ぐ。
GameEngine::Vector3 PlayerRearFollowCamera::SmoothEye(const GameEngine::Vector3& targetEye,
   const GameEngine::Vector3& up,
   float deltaTime) {
   using namespace GameEngine;

   // ピボットから見た理想オフセット
   Vector3 targetOffset = targetEye - pivotTarget_;

   if (!isEyeInitialized_) {
	  // 初回はスナップ（補間履歴なし）
	  currentEyeOffset_ = targetOffset;
	  isEyeInitialized_ = true;
	  return pivotTarget_ + currentEyeOffset_;
   }

   // ピボット相対オフセットを補間する
   float t = ExpSmoothingFactor(positionLerpSpeed, deltaTime);

   // 原因: eye オフセット全体を3D方向としてslerpすると、着地時に補間軸が任意になり、
   //       カメラがプレイヤーの上や下を通って「ぐりん」と一回転するように見える。
   // 修正: 重力Upに沿う高さと、Upに垂直な水平半径・水平角を分けて補間し、
   //       位置補間を必ず重力水平面まわりの自然な回り込みに制限する。
   Vector3 safeUp = NormalizeOrFallback(up, { 0.0f, 1.0f, 0.0f });
   float currentHeight = currentEyeOffset_.Dot(safeUp);
   float targetHeight = targetOffset.Dot(safeUp);
   Vector3 currentPlanar = currentEyeOffset_ - safeUp * currentHeight;
   Vector3 targetPlanar = targetOffset - safeUp * targetHeight;
   float currentRadius = currentPlanar.Length();
   float targetRadius = targetPlanar.Length();

   float smoothedHeight = currentHeight + (targetHeight - currentHeight) * t;
   float smoothedRadius = currentRadius + (targetRadius - currentRadius) * t;
   Vector3 smoothedPlanar = targetPlanar;
   if (currentRadius > 1e-4f && targetRadius > 1e-4f) {
	  Vector3 currentDirection = currentPlanar * (1.0f / currentRadius);
	  Vector3 targetDirection = targetPlanar * (1.0f / targetRadius);
	  float dot = std::clamp(currentDirection.Dot(targetDirection), -1.0f, 1.0f);
	  float signedAngle = std::acos(dot);
	  if (safeUp.Dot(currentDirection.Cross(targetDirection)) < 0.0f) {
		 signedAngle = -signedAngle;
	  }
	  smoothedPlanar = RotateAroundAxisUnit(currentDirection, safeUp, signedAngle * t) * smoothedRadius;
   } else if (targetRadius > 1e-4f) {
	  smoothedPlanar = targetPlanar * (smoothedRadius / targetRadius);
   } else if (currentRadius > 1e-4f) {
	  smoothedPlanar = currentPlanar * (smoothedRadius / currentRadius);
   } else {
	  smoothedPlanar = { 0.0f, 0.0f, 0.0f };
   }
   currentEyeOffset_ = safeUp * smoothedHeight + smoothedPlanar;

   // ワールド座標に戻して返す
   return pivotTarget_ + currentEyeOffset_;
}

/// @brief 保存データから復元すべきでないランタイム補間状態を初期化する
/// @details シーン保存時に古い currentEyeOffset_ や currentFov_ が残っていても、
///          起動直後は現在のプレイヤー位置・設定値から必ず同じ初期状態を作る。
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
   isAirborneResetHeld_ = false;
   airborneResetBlend_ = 0.0f;
   currentPlanetDirectionGravityFactor_ = 0.0f;
   jumpPlanetDirectionRampElapsed_ = (std::max)(0.0f, jumpPlanetDirectionRampSeconds);
   currentBackward_ = { 0.0f, 0.0f, -1.0f };
   currentPlanetBackward_ = currentBackward_;
   isPlanetBackwardInitialized_ = false;
   isInitialized_ = false;
   landingRearLerpElapsed_ = (std::max)(0.0f, landingRearLerpRampSeconds);
   landingRearLerpStartSpeed_ = airborneForwardLerpSpeed;
   lastAirborneRearFollowSpeed_ = airborneForwardLerpSpeed;

   playerSpeed_ = 0.0f;
   playerVelocity_ = { 0.0f, 0.0f, 0.0f };
   currentFov_ = fovDefault;
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

   if (auto* owner = GetOwnerCamera()) {
	  GameEngine::CameraState state = owner->GetState();
	  state.fov = fovDefault;
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
	  jumpPlanetDirectionRampElapsed_ = 0.0f;
   }
   float jumpPlanetRampSeconds = (std::max)(0.0f, jumpPlanetDirectionRampSeconds);
   if (isAirborne_) {
	  // ジャンプ直後に惑星補間最大量まで一気に使うと、速度後方補間と合わさって急に画角が動く。
	  // 着地時の後方補間と同じ考え方で、惑星補間の強さだけを指定秒数で通常値へ戻す。
	  jumpPlanetDirectionRampElapsed_ = (std::min)(
		 jumpPlanetDirectionRampElapsed_ + (std::max)(0.0f, deltaTime),
		 jumpPlanetRampSeconds);
   } else {
	  jumpPlanetDirectionRampElapsed_ = jumpPlanetRampSeconds;
   }

   // ② 地上/空中の見え方を滑らかに切り替えるブレンド値を更新する
   UpdateAirborneBlend(deltaTime);

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
   ApplyLookAt(state, eye, viewUp);

   wasAirborneLastFrame_ = isAirborne_;
}

nlohmann::json PlayerRearFollowCamera::Serialize() const {
   return nlohmann::json{
	   { "distance", distance },
	   { "height", height },
	   { "groundedTargetHeight", groundedTargetHeight },
	   { "airborneDistanceOffset", airborneDistanceOffset },
	   { "airborneFovOffset", airborneFovOffset },
	   { "airbornePlanetDirectionBlend", airbornePlanetDirectionBlend },
	   { "airbornePlanetDirectionLerpSpeed", airbornePlanetDirectionLerpSpeed },
	   { "jumpPlanetDirectionRampSeconds", jumpPlanetDirectionRampSeconds },
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
	   { "accelToFovKick", accelToFovKick },
	   { "accelToDistanceKick", accelToDistanceKick },
	   { "turboFovKickMax", turboFovKickMax },
	   { "turboDistanceKickMax", turboDistanceKickMax },
	   { "speedBoostThreshold", speedBoostThreshold },
	   { "speedBoostMax", speedBoostMax },
	   { "positionLerpSpeed", positionLerpSpeed },
   };
}

void PlayerRearFollowCamera::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
	  return;
   }

   distance = ReadFloat(data, "distance", distance);
   height = ReadFloat(data, "height", height);
   groundedTargetHeight = ReadFloat(data, "groundedTargetHeight", groundedTargetHeight);
   airborneDistanceOffset = ReadFloat(data, "airborneDistanceOffset", airborneDistanceOffset);
   airborneFovOffset = ReadFloat(data, "airborneFovOffset", airborneFovOffset);
   airbornePlanetDirectionBlend = ReadFloat(data, "airbornePlanetDirectionBlend", airbornePlanetDirectionBlend);
   airbornePlanetDirectionLerpSpeed = ReadFloat(data, "airbornePlanetDirectionLerpSpeed", airbornePlanetDirectionLerpSpeed);
   jumpPlanetDirectionRampSeconds = ReadFloat(data, "jumpPlanetDirectionRampSeconds", jumpPlanetDirectionRampSeconds);
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
   fovDefault = ReadFloat(data, "fovDefault", fovDefault);
   fovBoostMax = ReadFloat(data, "fovBoostMax", fovBoostMax);
   fovLerpSpeed = ReadFloat(data, "fovLerpSpeed", fovLerpSpeed);
   distanceBoostMax = ReadFloat(data, "distanceBoostMax", distanceBoostMax);
   springStiffness = ReadFloat(data, "springStiffness", springStiffness);
   springDamping = ReadFloat(data, "springDamping", springDamping);
   accelToFovKick = ReadFloat(data, "accelToFovKick", accelToFovKick);
   accelToDistanceKick = ReadFloat(data, "accelToDistanceKick", accelToDistanceKick);
   turboFovKickMax = ReadFloat(data, "turboFovKickMax", turboFovKickMax);
   turboDistanceKickMax = ReadFloat(data, "turboDistanceKickMax", turboDistanceKickMax);
   speedBoostThreshold = ReadFloat(data, "speedBoostThreshold", speedBoostThreshold);
   speedBoostMax = ReadFloat(data, "speedBoostMax", speedBoostMax);
   positionLerpSpeed = ReadFloat(data, "positionLerpSpeed", positionLerpSpeed);
   autoSpeed_ = ReadFloat(data, "autoSpeed", autoSpeed_);

   ResetRuntimeState();
}

#ifdef USE_IMGUI
void PlayerRearFollowCamera::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   if (ImGui::Checkbox(Tr("有効", "Enabled"), &isEnabled_)) {}

   ImGui::DragFloat(Tr("距離", "Distance"), &distance, 0.1f, 1.0f, 100.0f);
   ImGui::DragFloat(Tr("高さ", "Height"), &height, 0.1f, -20.0f, 50.0f);
   ImGui::DragFloat(Tr("地上注視高さ", "Grounded Target Height"), &groundedTargetHeight, 0.05f, -5.0f, 10.0f);
   ImGui::DragFloat(Tr("空中距離加算", "Airborne Distance Offset"), &airborneDistanceOffset, 0.1f, 0.0f, 30.0f);
   ImGui::DragFloat(Tr("空中FOV加算", "Airborne FOV Offset"), &airborneFovOffset, 0.001f, 0.0f, 0.5f, "%.3f");
   ImGui::Checkbox(Tr("空中惑星方向ガイド", "Airborne Planet Direction Guide"), &enableAirbornePlanetDirectionGuide);
   ImGui::DragFloat(Tr("惑星補間最大量", "Planet Blend Max"), &airbornePlanetDirectionBlend, 0.01f, 0.0f, 1.0f, "%.2f");
   ImGui::DragFloat(Tr("惑星補間追従速度", "Planet Blend Follow Speed"), &airbornePlanetDirectionLerpSpeed, 0.1f, 0.0f, 30.0f);
   ImGui::DragFloat(Tr("ジャンプ惑星補間復帰秒", "Jump Planet Blend Ramp Seconds"), &jumpPlanetDirectionRampSeconds, 0.01f, 0.0f, 5.0f, "%.2f");
   ImGui::Checkbox(Tr("重力方向で開始判定", "Gate Planet Blend By Gravity"), &enableAirborneGravityDirectionBoost);
   ImGui::DragFloat(Tr("補間開始一致度", "Planet Blend Start Alignment"), &airborneGravityDirectionBoostThreshold, 0.01f, 0.0f, 1.0f, "%.2f");
   ImGui::DragFloat(Tr("補間最大一致度", "Planet Blend Full Alignment"), &airborneGravityDirectionBoostFullThreshold, 0.01f, 0.0f, 1.0f, "%.2f");
   ImGui::DragFloat(Tr("補間カーブ", "Planet Blend Bias"), &airborneGravityDirectionBoostBias, 0.05f, 0.01f, 5.0f, "%.2f");
   ImGui::DragFloat(Tr("空中補間速度", "Airborne Blend Speed"), &airborneBlendLerpSpeed, 0.1f, 0.1f, 30.0f);
   ImGui::DragFloat(Tr("空中速度後方補間", "Airborne Velocity Rear Lerp"), &airborneForwardLerpSpeed, 0.1f, 0.0f, 30.0f);
   ImGui::DragFloat(Tr("空中リセット補間", "Airborne Reset Lerp"), &airborneResetLerpSpeed, 0.1f, 0.1f, 30.0f);
   ImGui::DragFloat(Tr("後方補間速度", "Rear Lerp Speed"), &rearLerpSpeed, 0.1f, 0.0f, 30.0f);
   ImGui::DragFloat(Tr("着地後方補間到達秒", "Landing Rear Lerp Seconds"), &landingRearLerpRampSeconds, 0.01f, 0.0f, 5.0f, "%.2f");
   ImGui::DragFloat(Tr("GravityUp補間", "GravityUp Lerp"), &gravityUpLerpSpeed, 0.1f, 0.1f, 30.0f);
   ImGui::DragFloat(Tr("FOV デフォルト", "FOV Default"), &fovDefault, 0.001f, 0.1f, 1.5f, "%.3f");
   ImGui::DragFloat(Tr("FOV ブースト最大", "FOV Boost Max"), &fovBoostMax, 0.001f, 0.0f, 0.5f, "%.3f");
   ImGui::DragFloat(Tr("FOV 補間速度", "FOV Lerp Speed"), &fovLerpSpeed, 0.1f, 0.1f, 20.0f);
   ImGui::DragFloat(Tr("距離ブースト最大", "Dist Boost Max"), &distanceBoostMax, 0.1f, 0.0f, 30.0f);
   ImGui::DragFloat(Tr("ばね剛性", "Spring Stiffness"), &springStiffness, 1.0f, 1.0f, 300.0f);
   ImGui::DragFloat(Tr("ばね減衰", "Spring Damping"), &springDamping, 0.5f, 0.0f, 100.0f);
   ImGui::DragFloat(Tr("加速→FOVキック", "Accel->FOV Kick"), &accelToFovKick, 0.0001f, 0.0f, 0.02f, "%.4f");
   ImGui::DragFloat(Tr("加速→距離キック", "Accel->Dist Kick"), &accelToDistanceKick, 0.001f, 0.0f, 0.5f, "%.3f");
   ImGui::DragFloat(Tr("ターボFOVキック", "Turbo FOV Kick"), &turboFovKickMax, 0.001f, 0.0f, 0.3f, "%.3f");
   ImGui::DragFloat(Tr("ターボ距離キック", "Turbo Dist Kick"), &turboDistanceKickMax, 0.05f, 0.0f, 8.0f);
   ImGui::DragFloat(Tr("速度しきい値", "Speed Threshold"), &speedBoostThreshold, 0.5f, 0.0f, 100.0f);
   ImGui::DragFloat(Tr("速度ブースト最大", "Speed Boost Max"), &speedBoostMax, 0.5f, 0.0f, 200.0f);
   ImGui::DragFloat(Tr("位置補間", "Position Lerp"), &positionLerpSpeed, 0.5f, 1.0f, 100.0f);

   ImGui::Separator();
   ImGui::Text("%s: %s", Tr("空中", "Airborne"), isAirborne_ ? Tr("はい", "true") : Tr("いいえ", "false"));
   ImGui::Text("%s: %.2f", Tr("空中ブレンド", "Airborne Blend"), currentAirborneBlend_);
   ImGui::Text("%s: %s / %.2f", Tr("空中リセット", "Airborne Reset"), isAirborneResetHeld_ ? Tr("押下中", "held") : Tr("なし", "none"), airborneResetBlend_);
   ImGui::Text("%s: %.2f", Tr("惑星補間重力係数", "Planet Blend Gravity Factor"), currentPlanetDirectionGravityFactor_);
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

