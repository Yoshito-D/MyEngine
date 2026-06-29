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

/// @brief 指数平滑の補間係数を返す（dt 変動に強く、常に 0..1 未満）
static float ExpSmoothingFactor(float speed, float deltaTime) {
   float k = (std::max)(0.0f, speed);
   float dt = (std::max)(0.0f, deltaTime);
   return 1.0f - std::exp(-k * dt);
}

static nlohmann::json SerializeVector3(const GameEngine::Vector3& value) {
   return nlohmann::json::array({ value.x, value.y, value.z });
}

static GameEngine::Vector3 DeserializeVector3(const nlohmann::json& data, const GameEngine::Vector3& fallback) {
   if (!data.is_array() || data.size() != 3) {
	  return fallback;
   }
   return GameEngine::Vector3(data[0].get<float>(), data[1].get<float>(), data[2].get<float>());
}

static float ReadFloat(const nlohmann::json& data, const char* key, float fallback) {
   return data.contains(key) && data.at(key).is_number() ? data.at(key).get<float>() : fallback;
}

static bool ReadBool(const nlohmann::json& data, const char* key, bool fallback) {
   return data.contains(key) && data.at(key).is_boolean() ? data.at(key).get<bool>() : fallback;
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
/// @details 3つのフェーズに分けて管理する：
///   1. 初回：desiredBackward をそのまま設定（補間なし）
///   2. 毎フレーム：currentBackward_ を現在の重力平面に再投影
///      → gravityUp が変化した際に、後方ベクトルが平面から外れるのを防ぐ
///   3. 地上時のみ：desiredBackward に向けて Lerp で追従
///      → 空中時は慣性を表現するため後方ベクトルを固定する
/// @param up 正規化済み補間済み重力Up
/// @param deltaTime フレーム時間
void PlayerRearFollowCamera::UpdateBackwardVector(const GameEngine::Vector3& up, float deltaTime) {
   using namespace GameEngine;

   // プレイヤーの前方の逆方向＝カメラが向かうべき後方ベクトルを重力平面に投影する
   // （followForward_ は 3D 空間の前方なので、水平成分だけ取り出す）
   Vector3 desiredBackward = ProjectOnPlaneNorm(-followForward_, up, currentBackward_);

   if (!isInitialized_) {
	  // 初回はスムーズ開始のためそのまま採用
	  currentBackward_ = desiredBackward;
	  isInitialized_ = true;
	  return;
   }

   // ---- 毎フレーム: 現在後方ベクトルを重力平面へ再投影 ----
   // gravityUp が変化すると前フレームの currentBackward_ が平面外にズレるため
   // 毎フレーム平面に投影し直すことで水平性を維持する
   currentBackward_ = ProjectOnPlaneNorm(currentBackward_, up, desiredBackward);

   if (!isAirborne_) {
	  // ---- 地上時のみ: desired へ Lerp で追従 ----
	  // プレイヤーが向きを変えたとき、カメラがその後方へじわりと追いかける挙動を実現する
	  // 空中時は追従させないことで、ジャンプ中に視点が回ってしまうのを防ぐ
	  float t = ExpSmoothingFactor(rearLerpSpeed, deltaTime);
	  Vector3 lerped = Vector3::Lerp(currentBackward_, desiredBackward, t);
	  float lerpLen = lerped.LengthSquared();
	  if (lerpLen > 1e-6f) {
		 currentBackward_ = lerped.Normalize();
	  } else {
		 currentBackward_ = desiredBackward;
	  }
   }
}

/// @brief eye 位置を計算する（後退距離に加速ブーストを加味）
/// @details カメラは pivot の後方（currentBackward_ 方向）に distance 離れた場所に置く。
///          加速中は distanceBoostMax * boostAlpha 分さらに後退させることで
///          「カメラが引けて世界が広がる」視覚的な加速感を演出する。
/// @param up 正規化済み重力Up（高さオフセットに使用）
/// @param boostAlpha 加速度合い [0,1]
GameEngine::Vector3 PlayerRearFollowCamera::ComputeEye(const GameEngine::Vector3& up,
   float boostAlpha) const {
   // 加速時の後退量を計算（通常距離 + 定常ブースト + Springキックオフセット）
   float boostedDistance = distance + distanceBoostMax * boostAlpha + springDistanceOffset_;

   // eye = pivot から上方向に height、後方に boostedDistance 離れた位置
   return pivotTarget_ + up * height + currentBackward_ * boostedDistance;
}

/// @brief LookAt 行列を構築してカメラ状態へ書き込み、キャッシュ軸を更新する
/// @details LookAt の up には補間済み重力Up（currentGravityUp_）を使用する。
///          これにより惑星の切り替え中も常に「その惑星の重力方向が上」として描画される。
///          cachedRight_ / cachedUp_ は外部（UI など）で参照されるため確定させる。
void PlayerRearFollowCamera::ApplyLookAt(GameEngine::CameraState& state,
   const GameEngine::Vector3& eye,
   const GameEngine::Vector3& up) {
   using namespace GameEngine;

   // zaxis = pivot を向く方向（カメラ前方）
   Vector3 zaxis = pivotTarget_ - eye;
   float zLen = zaxis.Length();
   if (zLen > 1e-6f) {
	  zaxis = zaxis * (1.0f / zLen);
   } else {
	  zaxis = -currentBackward_; // eye が pivot と一致する極端ケース
   }

   // xaxis = up × zaxis（右方向）。退化時は前フレームの値を保持
   Vector3 xaxis = up.Cross(zaxis);
   float xLen = xaxis.Length();
   if (xLen > 1e-6f) {
	  xaxis = xaxis * (1.0f / xLen);
   } else {
	  xaxis = cachedRight_;
   }

   // キャッシュ更新
   cachedRight_ = xaxis;
   // cachedUp_ は惑星基準の up を保持（GravityFollowCamera と同じ規約）
   cachedUp_ = up;

   state.transform.translation = eye;
   state.SetViewMatrix(MakeLookAtMatrix(eye, pivotTarget_, up));
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

   // 目標 FOV = 通常 FOV + 速度比例ブースト + Springキック
   float targetFov = fovDefault + fovBoostMax * boostAlpha + springFovOffset_;

   // FOV を滑らかに補間（急変させず視覚的に自然に追従）
   float t = ExpSmoothingFactor(fovLerpSpeed, deltaTime);
   currentFov_ = currentFov_ + (targetFov - currentFov_) * t;
   state.fov = currentFov_;

   return boostAlpha;
}

/// @brief 目標 eye オフセット（ピボット相対）を lerp 補間し、ワールド eye 位置を返す
/// @details 【なぜ相対オフセットで補間するか】
///          絶対座標で補間すると、ピボット（プレイヤー）が移動するたびに
///          理想 eye も一緒にワールド空間を動く。この場合の補間パスは
///          「前フレームの eye（旧ピボット後方）→ 今フレームの eye（新ピボット後方）」
///          という直線を通るため、一瞬プレイヤーを突き抜けるルートになり得る。
///
///          ピボット相対のオフセット（= eye - pivot）を補間すると、
///          補間パスは常に「ピボットを原点とした後方の弧」を描くため
///          カメラがプレイヤーの前方に入り込むことがなくなる。
GameEngine::Vector3 PlayerRearFollowCamera::SmoothEye(const GameEngine::Vector3& targetEye,
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
   currentEyeOffset_ = Vector3::Lerp(currentEyeOffset_, targetOffset, t);

   // ワールド座標に戻して返す
   return pivotTarget_ + currentEyeOffset_;
}

// =============================================================================
// パブリックメソッド実装
// =============================================================================

void PlayerRearFollowCamera::MutateCameraState(GameEngine::CameraState& state, float deltaTime) {
   // ① 重力Up を目標値へ向けてスムーズに補間する
   //    → 惑星切り替え時の急激なロール変化を防ぐ
   GameEngine::Vector3 up = SmoothGravityUp(deltaTime);

   // ② 後方ベクトル（currentBackward_）を更新する
   //    → 重力平面への再投影 + 地上時の Lerp 追従を行う
   UpdateBackwardVector(up, deltaTime);

   // ③ プレイヤー速度に応じた FOV ブーストを計算し boostAlpha を取得する
   //    → FOV は state.fov へ反映済み、boostAlpha は距離ブーストに転用する
   float boostAlpha = UpdateAccelerationEffect(state, deltaTime);

   // ④ 加速ブーストを加味した eye 位置を算出する
   //    → 加速中はカメラが後退して視野が広がり、速度感が増す
   GameEngine::Vector3 eye = ComputeEye(up, boostAlpha);

   // ⑤ eye 位置をピボット相対オフセットで補間し、急激なテレポートを防ぐ
   //    → 相対補間により、ピボットが移動してもカメラが前方へ突き抜けない
   eye = SmoothEye(eye, deltaTime);

   // ⑥ LookAt 行列を構築してカメラ状態へ反映する（補間済み eye を使用）
   ApplyLookAt(state, eye, up);
}

nlohmann::json PlayerRearFollowCamera::Serialize() const {
   return nlohmann::json{
	   { "distance", distance },
	   { "height", height },
	   { "rearLerpSpeed", rearLerpSpeed },
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
	   { "isAirborne", isAirborne_ },
	   { "isInitialized", isInitialized_ },
	   { "currentFov", currentFov_ },
	   { "springFovOffset", springFovOffset_ },
	   { "springFovVelocity", springFovVelocity_ },
	   { "springDistanceOffset", springDistanceOffset_ },
	   { "springDistanceVelocity", springDistanceVelocity_ },
	   { "previousPlayerSpeed", previousPlayerSpeed_ },
	   { "isSpeedInitialized", isSpeedInitialized_ },
	   { "currentEyeOffset", SerializeVector3(currentEyeOffset_) },
   };
}

void PlayerRearFollowCamera::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
	  return;
   }

   distance = ReadFloat(data, "distance", distance);
   height = ReadFloat(data, "height", height);
   rearLerpSpeed = ReadFloat(data, "rearLerpSpeed", rearLerpSpeed);
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

   isAirborne_ = ReadBool(data, "isAirborne", isAirborne_);
   isInitialized_ = ReadBool(data, "isInitialized", isInitialized_);
   autoSpeed_ = ReadFloat(data, "autoSpeed", autoSpeed_);
   currentFov_ = ReadFloat(data, "currentFov", currentFov_);
   springFovOffset_ = ReadFloat(data, "springFovOffset", springFovOffset_);
   springFovVelocity_ = ReadFloat(data, "springFovVelocity", springFovVelocity_);
   springDistanceOffset_ = ReadFloat(data, "springDistanceOffset", springDistanceOffset_);
   springDistanceVelocity_ = ReadFloat(data, "springDistanceVelocity", springDistanceVelocity_);
   isSpeedInitialized_ = ReadBool(data, "isSpeedInitialized", isSpeedInitialized_);
   if (data.contains("currentEyeOffset")) {
	  currentEyeOffset_ = DeserializeVector3(data.at("currentEyeOffset"), currentEyeOffset_);
   }
}

#ifdef USE_IMGUI
void PlayerRearFollowCamera::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   if (ImGui::Checkbox(Tr("有効", "Enabled"), &isEnabled_)) {}

   ImGui::DragFloat(Tr("距離", "Distance"), &distance, 0.1f, 1.0f, 100.0f);
   ImGui::DragFloat(Tr("高さ", "Height"), &height, 0.1f, -20.0f, 50.0f);
   ImGui::DragFloat(Tr("後方補間速度", "Rear Lerp Speed"), &rearLerpSpeed, 0.1f, 0.0f, 30.0f);
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
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("目標GravityUp", "Target GravityUp"), gravityUp_.x, gravityUp_.y, gravityUp_.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("現在GravityUp", "Current GravityUp"), currentGravityUp_.x, currentGravityUp_.y, currentGravityUp_.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("追従前方向", "Follow Forward"), followForward_.x, followForward_.y, followForward_.z);
   ImGui::Text("%s: %.2f  %s: %.3f", Tr("プレイヤー速度", "Player Speed"), playerSpeed_, Tr("現在FOV", "Current FOV"), currentFov_);
}
#endif

} // namespace App

