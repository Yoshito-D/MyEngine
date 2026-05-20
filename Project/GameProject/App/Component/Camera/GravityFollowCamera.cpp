#include "pch.h"
#include "GravityFollowCamera.h"
#include "Scene/Camera/Core/CameraState.h"
#include "Utility/MathUtils/MatrixOperations.h"
#include "Utility/MathUtils/VectorOperations.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

using namespace GameEngine;

namespace App {

// =============================================================================
// ファイルスコープのユーティリティ関数
// =============================================================================

/// @brief 任意軸周りの回転（ロドリゲスの回転公式）
/// @details クォータニオンを使わずに v を axis 周りに angle ラジアン回転させる。
///          ピッチ回転（right 軸周り）や yaw 回転（up 軸周り）に使用する。
static Vector3 RotateAroundAxis(const Vector3& v, const Vector3& axis, float angle) {
   float c = std::cos(angle);
   float s = std::sin(angle);
   // ロドリゲス公式: v*cos + (axis×v)*sin + axis*(axis・v)*(1-cos)
   return v * c + axis.Cross(v) * s + axis * (axis.Dot(v) * (1.0f - c));
}

/// @brief ベクトルを平面へ投影して正規化する（失敗時はフォールバックを返す）
/// @details 重力平面への投影に使用する。up 方向成分を除去して水平化する。
///          結果ベクトルの長さが極小（ほぼ up と平行）な場合は fallback を返す。
static Vector3 ProjectOnPlaneNorm(const Vector3& v, const Vector3& up, const Vector3& fallback) {
   Vector3 proj = v - up * up.Dot(v);
   float len = proj.Length();
   return len > 1e-4f ? proj * (1.0f / len) : fallback;
}

/// @brief 指数平滑の補間係数を返す（dt 変動に強く、常に 0..1 未満）
static float ExpSmoothingFactor(float speed, float deltaTime) {
   float k = (std::max)(0.0f, speed);
   float dt = (std::max)(0.0f, deltaTime);
   return 1.0f - std::exp(-k * dt);
}

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
static Vector3 RotateTowardsUnit(const Vector3& current, const Vector3& target, float maxRadiansDelta) {
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

   // ほぼ反平行: 補間中にゼロ化しないよう、直交軸周りに一定角回す
   if (dot < -0.999f) {
      Vector3 axis = (std::abs(c.x) < 0.9f) ? Vector3{ 1.0f, 0.0f, 0.0f } : Vector3{ 0.0f, 1.0f, 0.0f };
      axis = c.Cross(axis);
      float axisLen = axis.Length();
      if (axisLen < 1e-6f) {
         axis = Vector3{ 0.0f, 0.0f, 1.0f };
      } else {
         axis = axis * (1.0f / axisLen);
      }
      return RotateAroundAxis(c, axis, step);
   }

   float tRatio = step / angle;
   float sinTotal = std::sin(angle);
   float w0 = std::sin((1.0f - tRatio) * angle) / sinTotal;
   float w1 = std::sin(tRatio * angle) / sinTotal;
   Vector3 out = c * w0 + t * w1;
   float outLen = out.Length();
   return outLen > 1e-6f ? out * (1.0f / outLen) : t;
}

// =============================================================================
// プライベートメソッド実装
// =============================================================================

/// @brief 目標重力Up に向けて currentGravityUp_ を nlerp 補間する
/// @details 惑星を切り替えると gravityUp_（目標値）が急変するが、
///          currentGravityUp_ をフレームごとに少しずつ近づけることで
///          カメラのロールが急激に跳ぶ問題を防ぐ。
///          nlerp = 線形補間後に正規化。slerp より軽量で品質的にも十分。
Vector3 GravityFollowCamera::SmoothGravityUp(float deltaTime) {
   // 目標 up を正規化
   Vector3 targetUp = gravityUp_;
   float targetLen = targetUp.Length();
   if (targetLen < 1e-6f) targetUp = { 0.0f, 1.0f, 0.0f };
   else targetUp = targetUp * (1.0f / targetLen);

   // 現在 up も念のため正規化（数値誤差の蓄積を防ぐ）
   float curLen = currentGravityUp_.Length();
   if (curLen < 1e-6f) currentGravityUp_ = targetUp;
   else currentGravityUp_ = currentGravityUp_ * (1.0f / curLen);

   // 回転前の Up を保存しておく（後でデルタ回転を算出するため）
   Vector3 oldUp = currentGravityUp_;

   // 角速度制限付きで目標 Up へ追従（180°近傍でも破綻しない）
   float maxRadiansDelta = gravityUpLerpSpeed * (std::max)(0.0f, deltaTime);
   currentGravityUp_ = RotateTowardsUnit(oldUp, targetUp, maxRadiansDelta);

   // ─────────────────────────────────────────────────────────────
   // 【重要】今フレームの Up デルタ回転を flatForward_ にも適用する
   //
   // なぜこれが必要か：
   //   Up が変化しても flatForward_ を「重力平面へ投影」するだけでは
   //   "up.Cross(flatForward_)" の符号が Up の通過点で反転し、
   //   カメラの right 方向が瞬間に逆を向いてロールが跳ぶ。
   //
   //   Up の角変化（oldUp → currentGravityUp_）と同じ回転を
   //   flatForward_ に掛け合わせることで、カメラ全体が剛体のように
   //   回転し、right の符号が反転しない。
   // ─────────────────────────────────────────────────────────────
   float cosAngle = std::clamp(oldUp.Dot(currentGravityUp_), -1.0f, 1.0f);
   if (cosAngle < 0.9999f) {
      Vector3 rotAxis = oldUp.Cross(currentGravityUp_);
      float axisLen = rotAxis.Length();
      if (axisLen > 1e-6f) {
         rotAxis = rotAxis * (1.0f / axisLen);
         float deltaAngle = std::acos(cosAngle);
         flatForward_ = RotateAroundAxis(flatForward_, rotAxis, deltaAngle);
         float fLen = flatForward_.Length();
         if (fLen > 1e-6f) flatForward_ = flatForward_ * (1.0f / fLen);
      }
   }

   return currentGravityUp_;
}

/// @brief flatForward_ を現在の重力平面へ再投影し、right 軸を再構築する
/// @details gravityUp が変化すると前フレームの flatForward_ が平面外にズレるため、
///          毎フレーム投影し直すことで水平基底を常に正しい面に保つ。
///          退化（up と forward が平行）した場合は代替軸から復元する。
/// @return 正規化済みの right 軸（= up × flatForward_）
Vector3 GravityFollowCamera::RebuildBasis(const Vector3& up) {
   // flatForward_ を重力平面へ投影して正規化
   flatForward_ = ProjectOnPlaneNorm(flatForward_, up, flatForward_);

   // right = up × forward。退化判定を行い、退化時は代替軸から両軸を復元する
   Vector3 right = up.Cross(flatForward_);
   float rLen = right.Length();
   if (rLen > 1e-6f) {
      right = right * (1.0f / rLen);
   } else {
      // up と flatForward_ がほぼ平行（極付近など）→ 代替ベクトルから right を作り直す
      Vector3 tmp = (std::abs(up.x) < 0.9f) ? Vector3{ 1, 0, 0 } : Vector3{ 0, 1, 0 };
      right = up.Cross(tmp);
      right = right * (1.0f / right.Length());
      flatForward_ = right.Cross(up);
      flatForward_ = flatForward_ * (1.0f / flatForward_.Length());
   }
   return right;
}

/// @brief ピッチを加味した eye 位置と cameraUp を計算する
/// @details pitch_ は重力Up 方向を 0 として右方向（right）軸周りに回転させた角度。
///          ピッチ前方をそのまま反転した方向が eye←pivot ベクトルになる。
///          cameraUp もピッチ回転後の上方向を使うことでカメラのロールが自然になる。
void GravityFollowCamera::ComputeEyeAndUp(const Vector3& up,
                                          const Vector3& right,
                                          Vector3& outEye,
                                          Vector3& outCameraUp) const {
   // ピッチ後の前方方向（pivot から eye への逆方向）
   Vector3 pitchedForward = RotateAroundAxis(flatForward_, right, pitch_);
   float pfLen = pitchedForward.Length();
   if (pfLen > 1e-6f) pitchedForward = pitchedForward * (1.0f / pfLen);

   // eye = pivot の後方（−前方）に distance 分離れた位置
   outEye = pivotTarget_ + pitchedForward * (-distance_);

   // cameraUp = 重力Upをピッチ軸（right）周りに同量回転させたベクトル
   // ピッチに合わせて Up もチルトさせることで、カメラが自然な傾きを保つ
   outCameraUp = RotateAroundAxis(up, right, pitch_);
   float cuLen = outCameraUp.Length();
   if (cuLen > 1e-6f) outCameraUp = outCameraUp * (1.0f / cuLen);
}

/// @brief LookAt 行列を構築してカメラ状態へ書き込み、キャッシュ軸を更新する
/// @details MakeLookAtMatrix は eye・target・up から右手系ビュー行列を作成する。
///          cachedRight_ / cachedUp_ は外部から GetCameraRight/GetCameraUp で
///          参照されるため、ここで確定させる。
void GravityFollowCamera::ApplyLookAt(CameraState& state,
                                      const Vector3& eye,
                                      const Vector3& cameraUp) {
   // z 軸 = pivot → eye 方向（ビュー前方）
   Vector3 zaxis = pivotTarget_ - eye;
   float zLen = zaxis.Length();
   if (zLen > 1e-6f) zaxis = zaxis * (1.0f / zLen);

   // x 軸 = cameraUp × zaxis（right 方向）
   Vector3 xaxis = cameraUp.Cross(zaxis);
   float xLen = xaxis.Length();
   if (xLen > 1e-6f) xaxis = xaxis * (1.0f / xLen);
   else xaxis = cachedRight_; // 極端な退化時は前フレームの値を保持

   // キャッシュ更新（外部から参照される Right / Up を確定）
   cachedRight_ = xaxis;
   cachedUp_    = zaxis.Cross(xaxis);

   // カメラ位置とビュー行列をカメラ状態へ反映
   state.transform.translation = eye;
   state.SetViewMatrix(MakeLookAtMatrix(eye, pivotTarget_, cameraUp));
}

/// @brief プレイヤー速度に応じた FOV ブーストを補間し state.fov へ反映する
/// @details 速度が speedBoostThreshold を超えると FOV が fovDefault + fovBoostMax まで
///          線形に広がる。視野が広がると周辺視野に流れが生じ、加速感が増す。
///          速度が下がると fovLerpSpeed に従って徐々に通常 FOV に戻る。
void GravityFollowCamera::UpdateAccelerationEffect(CameraState& state, float deltaTime) {
   // 速度がどれだけ「加速帯域」に入っているかを [0,1] で算出
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
      // turboAlpha: ミニターボ相当の急激な変化をより大きく扱う係数
      float turboAlpha = std::clamp(speedDelta * 0.2f, 0.0f, 1.0f);
      float fovImpulse  = speedDelta * accelToFovKick      + turboAlpha * turboFovKickMax;
      float distImpulse = speedDelta * accelToDistanceKick + turboAlpha * turboDistanceKickMax;

      // impulse は velocity の次元（offset/sec）なので springStiffness を乗じて
      // Spring がすぐに「伸びた状態」に相当する速度を持つようにする
      springFovVelocity_      += std::clamp(fovImpulse,  0.0f, fovBoostMax      + turboFovKickMax)      * springStiffness;
      springDistanceVelocity_ += std::clamp(distImpulse, 0.0f, distanceBoostMax + turboDistanceKickMax) * springStiffness;
   } else if (speedDelta < 0.0f && playerSpeed_ < autoSpeed_) {
      // autoSpeed_ 以下に落ちたときのみ逆向きキックを与え、FOV を絞りつつカメラを近づける
      // ブースト後の autoSpeed への自然回復中は発火しない
      float decel = -speedDelta;
      float turboAlpha = std::clamp(decel * 0.2f, 0.0f, 1.0f);
      float fovImpulse  = decel * accelToFovKick      + turboAlpha * turboFovKickMax;
      float distImpulse = decel * accelToDistanceKick + turboAlpha * turboDistanceKickMax;

      springFovVelocity_      -= std::clamp(fovImpulse,  0.0f, fovBoostMax      + turboFovKickMax)      * springStiffness;
      springDistanceVelocity_ -= std::clamp(distImpulse, 0.0f, distanceBoostMax + turboDistanceKickMax) * springStiffness;
   }

   // 目標は常に 0（自然長）。Spring の復元力と減衰で収束させる。
   StepSpring1D(0.0f, springStiffness, springDamping, deltaTime, springFovOffset_,      springFovVelocity_);
   StepSpring1D(0.0f, springStiffness, springDamping, deltaTime, springDistanceOffset_, springDistanceVelocity_);

   // 目標 FOV = 通常FOV + 速度比例ブースト + Springキック
   float targetFov = fovDefault + fovBoostMax * boostAlpha + springFovOffset_;

   // 最終FOVは指数平滑で追従
   float t = ExpSmoothingFactor(fovLerpSpeed, deltaTime);
   currentFov_ = currentFov_ + (targetFov - currentFov_) * t;

   state.fov = currentFov_;
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
Vector3 GravityFollowCamera::SmoothEye(const Vector3& targetEye, float deltaTime) {
   // ピボットから見た理想オフセット
   Vector3 targetOffset = targetEye - pivotTarget_;

   // Springによる距離キックを後方方向へ加算
   float baseLen = targetOffset.Length();
   if (baseLen > 1e-6f) {
      Vector3 backDir = targetOffset * (1.0f / baseLen);
      targetOffset = targetOffset + backDir * springDistanceOffset_;
   }

   if (!isEyeInitialized_) {
      // 初回はスナップ（補間履歴なし）
      currentEyeOffset_ = targetOffset;
      isEyeInitialized_ = true;
      return pivotTarget_ + currentEyeOffset_;
   }

   // ピボット相対オフセットを補間する
   // → ピボットが動いてもオフセットの方向・長さが滑らかに変化するだけで
   //    補間パスがピボットを挟んで対側へ渡ることはない
   float t = ExpSmoothingFactor(positionLerpSpeed, deltaTime);
   currentEyeOffset_ = Vector3::Lerp(currentEyeOffset_, targetOffset, t);

   // ワールド座標に戻して返す
   return pivotTarget_ + currentEyeOffset_;
}

// =============================================================================
// パブリックメソッド実装
// =============================================================================

void GravityFollowCamera::MutateCameraState(CameraState& state, float deltaTime) {
   // ① 重力Up を目標値へ向けてスムーズに補間する
   //    → 惑星切り替え時の急激なロール変化を防ぐ
   Vector3 up = SmoothGravityUp(deltaTime);

   // ② 水平基底（flatForward_, right）を現在の重力平面に合わせて再構築する
   //    → gravityUp の変化に追従し、常に正しい水平軸を維持する
   Vector3 right = RebuildBasis(up);

   // ③ ピッチを加味した eye 位置と cameraUp を算出する
   Vector3 eye, cameraUp;
   ComputeEyeAndUp(up, right, eye, cameraUp);

   // ④ eye 位置を補間して急激なテレポートを防ぐ
   //    → ピボット（プレイヤー位置）が瞬間移動した場合や惑星切り替え時に
   //      カメラ位置が急変するのを positionLerpSpeed に従い滑らかに追従させる
   eye = SmoothEye(eye, deltaTime);

   // ⑤ LookAt 行列を構築してカメラ状態へ反映する（補間済み eye を使用）
   ApplyLookAt(state, eye, cameraUp);

   // ⑥ プレイヤー速度に応じた FOV ブーストを適用する（加速感の演出）
   UpdateAccelerationEffect(state, deltaTime);
}

void GravityFollowCamera::ProcessInput(const Vector2& mouseDelta, int32_t wheelDelta, bool isDragging) {
   // ドラッグ中はヨー（水平旋回）とピッチ（上下角）を更新する
   if (isDragging) {
      if (std::abs(mouseDelta.x) > 1e-6f) {
         // ヨー = 現在の補間済み重力Up 周りに flatForward_ を回転させる
         // currentGravityUp_ を使うことで補間中も自然な旋回になる
         Vector3 up = currentGravityUp_;
         float upLen = up.Length();
         if (upLen > 1e-6f) {
            up = up * (1.0f / upLen);
            flatForward_ = RotateAroundAxis(flatForward_, up, mouseDelta.x * rotateSpeed);
            float len = flatForward_.Length();
            if (len > 1e-6f) flatForward_ = flatForward_ * (1.0f / len);
         }
      }

      // ピッチ = 上下入力で更新。過度な反転（真上・真下）を防ぐためにクランプする
      pitch_ -= mouseDelta.y * rotateSpeed;
      pitch_ = std::clamp(pitch_, 0.1f, 1.4f);
   }

   // ホイール入力でカメラ距離を更新（近すぎないようにクランプ）
   if (wheelDelta != 0) {
      distance_ -= wheelDelta * scrollSpeed;
      distance_ = (std::max)(1.0f, distance_);
   }
}

Vector3 GravityFollowCamera::GetCameraUp()    const { return cachedUp_; }
Vector3 GravityFollowCamera::GetCameraRight() const { return cachedRight_; }

#ifdef USE_IMGUI
static constexpr float kRadToDeg = 57.2957795f;

void GravityFollowCamera::DrawInspector() {
   if (ImGui::Checkbox("Enabled", &isEnabled_)) {}

   ImGui::DragFloat("Distance",         &distance_,          0.1f,   0.5f,  200.0f);
   ImGui::DragFloat("Rotate Speed",     &rotateSpeed,        0.0001f, 0.0f,  0.1f, "%.4f");
   ImGui::DragFloat("Scroll Speed",     &scrollSpeed,        0.0001f, 0.0f,  0.1f, "%.4f");
   ImGui::DragFloat("GravityUp Lerp",   &gravityUpLerpSpeed, 0.1f,   0.1f,  30.0f);
   ImGui::DragFloat("FOV Default",      &fovDefault,         0.001f, 0.1f,  1.5f, "%.3f");
   ImGui::DragFloat("FOV Boost Max",    &fovBoostMax,        0.001f, 0.0f,  0.5f, "%.3f");
   ImGui::DragFloat("FOV Lerp Speed",   &fovLerpSpeed,       0.1f,   0.1f,  20.0f);
   ImGui::DragFloat("Distance Boost",   &distanceBoostMax,   0.05f,  0.0f,  10.0f);
   ImGui::DragFloat("Spring Stiffness", &springStiffness,    1.0f,   1.0f, 300.0f);
   ImGui::DragFloat("Spring Damping",   &springDamping,      0.5f,   0.0f, 100.0f);
   ImGui::DragFloat("Accel->FOV Kick",  &accelToFovKick,     0.0001f, 0.0f, 0.02f, "%.4f");
   ImGui::DragFloat("Accel->Dist Kick", &accelToDistanceKick,0.001f, 0.0f, 0.5f, "%.3f");
   ImGui::DragFloat("Turbo FOV Kick",   &turboFovKickMax,    0.001f, 0.0f, 0.3f, "%.3f");
   ImGui::DragFloat("Turbo Dist Kick",  &turboDistanceKickMax,0.05f, 0.0f, 8.0f);
   ImGui::DragFloat("Speed Threshold",  &speedBoostThreshold, 0.5f,  0.0f, 100.0f);
   ImGui::DragFloat("Speed Boost Max",  &speedBoostMax,       0.5f,  0.0f, 200.0f);
   ImGui::DragFloat("Position Lerp",    &positionLerpSpeed,   0.5f,  1.0f, 100.0f);

   float pitchDeg = pitch_ * kRadToDeg;
   if (ImGui::SliderFloat("Pitch (deg)", &pitchDeg, 5.0f, 80.0f)) {
      pitch_ = pitchDeg / kRadToDeg;
   }

   ImGui::Separator();
   ImGui::Text("Target GravityUp:  (%.2f, %.2f, %.2f)", gravityUp_.x, gravityUp_.y, gravityUp_.z);
   ImGui::Text("Current GravityUp: (%.2f, %.2f, %.2f)", currentGravityUp_.x, currentGravityUp_.y, currentGravityUp_.z);
   ImGui::Text("Pivot Target:      (%.2f, %.2f, %.2f)", pivotTarget_.x, pivotTarget_.y, pivotTarget_.z);
   ImGui::Text("Flat Forward:      (%.2f, %.2f, %.2f)", flatForward_.x, flatForward_.y, flatForward_.z);
   ImGui::Text("Player Speed: %.2f  Current FOV: %.3f", playerSpeed_, currentFov_);
}
#endif

} // namespace App
