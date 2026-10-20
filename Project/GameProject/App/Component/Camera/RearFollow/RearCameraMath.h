#pragma once

#include "GameProject/App/Component/Camera/RearFollow/RearCameraTypes.h"
#include "GameEngine/Math/Functions/MathConstants.h"
#include "GameEngine/Math/Functions/MatrixOperations.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>

namespace App::RearCameraMath {

constexpr float kPlanetVisibilityGainFadeRange = 0.08f;
constexpr float kDefaultSafeFov = 0.45f;
constexpr float kMinSafeFov = 0.017453292f; // 1度
constexpr float kMaxSafeFov = 3.12413936f;  // 179度
constexpr float kMaxSpringDeltaTime = 0.25f;
constexpr float kMaxSpringStep = 1.0f / 120.0f;

// 旧ターボ係数が最大になっていた速度差を統合後も基準として使う。
constexpr float kSpeedDeltaForMaxKick = 5.0f;
constexpr float kGroundDirectionProjectionBlendRange = 0.15f;
constexpr float kRadiansToDegrees = 57.29577951308232f;
constexpr size_t kMaxCameraMeasurementSamples = 36000;
constexpr const char* kCameraMeasurementDirectory = "../Generated/CameraMeasurements";

/// @brief 非有限値を既定値へ戻し、描画に使えるFOV範囲へ制限する。
float ClampCameraFov(float fov);

/// @brief 指数平滑の補間係数を返す（dt 変動に強く、常に 0..1 未満）
float ExpSmoothingFactor(float speed, float deltaTime);

/// @brief JSONの数値を読み、欠落や型違いでは既存設定を維持する。
float ReadFloat(const nlohmann::json& data, const char* key, float fallback);

/// @brief JSONの真偽値を読み、欠落や型違いでは既存設定を維持する。
bool ReadBool(const nlohmann::json& data, const char* key, bool fallback);

/// @brief 微小ベクトルでは代替方向を使い、正規化可能な方向を返す。
GameEngine::Vector3 NormalizeOrFallback(const GameEngine::Vector3& value, const GameEngine::Vector3& fallback);

/// @brief 1次元Spring（半陰的オイラー）で状態を1ステップ進める
void StepSpring1D(
   float target, float stiffness, float damping, float deltaTime, float& inOutValue, float& inOutVelocity);

/// @brief 現在ベクトルを目標ベクトルへ「最大角速度」で回転させる
/// @details 180°近傍でもゼロベクトル化を避けるため、線形補間ではなく角度ベースで追従する。
GameEngine::Vector3 RotateTowardsUnit(const GameEngine::Vector3& current, const GameEngine::Vector3& target,
   float maxRadiansDelta, const GameEngine::Vector3& preferredAntipodalAxis = {0.0f, 0.0f, 0.0f});

/// @brief 単位方向同士を角度ベースで補間する
/// @details 方向ベクトルを通常の Lerp で混ぜると、180°近い組み合わせで途中がゼロに近づき、
///          Normalize 後に反対方向へ跳ぶため、カメラの前後反転として見える。
///          角度量へ変換して RotateTowardsUnit することで、必ず現在方向から目標方向へ連続回転させる。
GameEngine::Vector3 BlendUnitDirectionSafely(const GameEngine::Vector3& current, const GameEngine::Vector3& target,
   float blend, const GameEngine::Vector3& fallback,
   const GameEngine::Vector3& preferredAntipodalAxis = {0.0f, 0.0f, 0.0f});

/// @brief 単位ベクトルを指定軸まわりに回転する
GameEngine::Vector3 RotateAroundAxisUnit(
   const GameEngine::Vector3& value, const GameEngine::Vector3& axis, float radians);

/// @brief 同一平面上の単位方向同士の符号付き角度を返す
/// @details 180度付近では外積がほぼゼロになって符号を失うため、直前の旋回符号を使う。
float SignedAngleAroundAxis(const GameEngine::Vector3& from, const GameEngine::Vector3& to,
   const GameEngine::Vector3& axis, float antipodalSign);

/// @brief 0..1 の値を端で滑らかになるS字カーブへ変換する
float SmoothStep01(float value);

/// @brief 2方向のなす角を度で返す
float AngleDegrees(const GameEngine::Vector3& from, const GameEngine::Vector3& to);

/// @brief ベクトルの全要素が有限値かを返す
bool IsFiniteVector(const GameEngine::Vector3& value);

/// @brief カメラの直交基底からワールド回転クォータニオンを構築する
/// @details ビュー行列の回転部はワールド回転の転置なので、軸を列へ格納して変換する。
GameEngine::Quaternion MakeCameraRotationFromBasis(
   const GameEngine::Vector3& right, const GameEngine::Vector3& up, const GameEngine::Vector3& forward);

/// @brief ベクトルを平面へ投影して正規化する（失敗時はフォールバックを返す）
/// @details 重力平面への水平化に使用する。投影結果が極小（up と平行）な場合は
///          fallback の投影を試み、それも失敗したら代替軸を返す。
GameEngine::Vector3 ProjectOnPlaneNorm(
   const GameEngine::Vector3& v, const GameEngine::Vector3& up, const GameEngine::Vector3& fallback);

} // namespace App::RearCameraMath
