#pragma once

#include <cmath>
#include <algorithm>
#include "GameEngine/Math/Functions/MathConstants.h"

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include "GameEngine/Math/Types/Quaternion.h"

namespace App {

/// @brief 接地中の前進・ステアリング・姿勢再構築を担うコンポーネント
///
/// - 毎フレーム autoSpeed へ向けて速度を回復しながら前進
/// - ステアリング入力で yaw 回転
/// - (gravityUp, flatForward) から姿勢を完全再構築
/// - 姿勢候補を返し、最終適用はVehicleMoverに委ねる
class VehicleGroundMover final : public GameEngine::IObjectComponent {
public:
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "VehicleGroundMover";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"車両地上移動", "Vehicle Ground Mover"};

   /// @brief シーン保存とコンポーネント検索に使う型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 地上速度を更新し、地表に沿った姿勢候補を返す
   /// @param steerInput  左右入力（-1〜+1）
   /// @param gravityUp   現在の重力Up方向
   /// @param deltaTime   フレーム時間
   /// @param currentRot 計算の基準となる現在姿勢。
   /// @return 操舵後の姿勢。無効時は入力姿勢を返す。
   GameEngine::Quaternion Apply(float steerInput, const GameEngine::Vector3& gravityUp, float deltaTime,
      const GameEngine::Quaternion& currentRot);

   /// @brief 着地失敗による減速と加速禁止を一括で適用する
   /// @param speed ペナルティ適用直後の前進速度（m/s）
   /// @param duration 加速を禁止する時間（秒）
   void ApplyLandingPenalty(float speed, float duration);

   /// @brief 加速度を積む（持続的な加速・減速用）
   /// 積まれた加速度は UpdateSpeed 内で v += a * dt として適用され、
   /// 適用後にリセットされる。autoSpeed への回復はその後に行われる。
   /// @param accel 加算する加速度（m/s²）。
   void AddAcceleration(float accel);

   /// @brief 速度を即座に加算する（着地ブースト・ペナルティなど瞬間的な速度変化用）
   /// autoSpeed への回復は UpdateSpeed の指数平滑によって行われる。
   /// @param impulse 瞬間的に加える速度変化（m/s）。
   void AddVelocityImpulse(float impulse);

   /// @brief 着地判定前に禁止時間を進める（空中も含めて移動更新ごとに1回）
   void AdvanceAccelerationBlock(float deltaTime);

   /// @brief ミニターボと自動加速が禁止されているか取得する
   bool IsAccelerationBlocked() const { return accelerationBlockRemaining_ > 0.0f; }

   /// @brief 現在の前進実速度（m/s）を取得する
   float GetCurrentSpeed() const { return currentSpeed_; }

   /// @brief 直近の水平前方ベクトルを取得する
   GameEngine::Vector3 GetFlatForward() const { return flatForward_; }

#ifdef USE_IMGUI

   /// @brief 現在の設定と実行状態をインスペクターへ表示する。
   void DrawInspector() override;
#endif

   /// @brief コンポーネントの保存対象設定をJSONで返す。
   nlohmann::json Serialize() const override;

   /// @brief JSONから設定を復元する。
   /// @param data 保存されたコンポーネント設定。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief シーン固有の参照と実行履歴を除く、共有可能な設定JSONを返す。
   nlohmann::json GetSharedParameters() const override;

   /// @brief 実行履歴を維持したまま共有設定を適用する。
   /// @param data 適用する共有設定JSON。
   void ApplySharedParameters(const nlohmann::json& data) override;

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {

      /// @brief 自動前進速度（m/s）
      float autoSpeed = 16.0f;

      /// @brief ステアリング角速度（rad/s）
      float steerSpeed = 120.0f * GameEngine::MathConstants::kDegreesToRadians;

      /// @brief スピード回復速度（1/s）
      float speedRecovery = 1.0f;
      float maxSpeed = 40.0f; ///< 外部加速を含めた前進速度の上限（m/s）
   };

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const;

   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested);

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   float autoSpeed = 16.0f; ///< 通常走行速度。これを下回ったときのみ減速演出を発火する。
   float steerSpeed = 120.0f * GameEngine::MathConstants::kDegreesToRadians; ///< ステアリング角速度（rad/s）。
   float speedRecovery = 1.0f;                                               ///< スピード回復速度（1/s）。
   float maxSpeed = 40.0f;                                                   ///< 外部加速を含めた前進速度の上限（m/s）

private:
   /// @brief 指定秒数だけ加速を禁止し、保留中の正の加速を破棄する
   void BlockAcceleration(float seconds);

   /// @brief 速度を autoSpeed へ向けて回復させる
   void UpdateSpeed(float deltaTime);

   /// @brief ステアリング入力を反映した前方ベクトルを返す
   GameEngine::Vector3 ApplySteering(float steerInput, const GameEngine::Vector3& localForward,
      const GameEngine::Vector3& gravityUp, float deltaTime) const;

   /// @brief 方向ベクトルを gravityUp 平面に投影して正規化する
   GameEngine::Vector3 ProjectToHorizontalPlane(
      const GameEngine::Vector3& dir, const GameEngine::Vector3& gravityUp) const;

   /// @brief GravityBody の水平速度を flatForward * currentSpeed_ に更新する
   void ApplyVelocityToGravityBody(const GameEngine::Vector3& flatForward, const GameEngine::Vector3& gravityUp);

   /// @brief 現在の前進実速度（負値は未初期化を示す）
   float currentSpeed_ = -1.0f;

   /// @brief 外部から積まれた加速度（m/s²）。UpdateSpeed で v += a * dt して毎フレームリセット
   float acceleration_ = 0.0f;

   /// @brief 着地失敗による加速禁止の残り時間（秒）
   float accelerationBlockRemaining_ = 0.0f;

   /// @brief 外部から積まれた瞬間速度変化（m/s）。UpdateSpeed で直接加算してリセット
   float velocityImpulse_ = 0.0f;

   /// @brief 直近の水平前方ベクトル
   GameEngine::Vector3 flatForward_ = {0.0f, 0.0f, 1.0f};

   const float kInitialSpeed = 15.0f; ///< 初期化時に適用する地上走行速度。
};

} // namespace App
