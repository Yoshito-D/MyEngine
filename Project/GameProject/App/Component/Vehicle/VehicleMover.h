#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include "GameEngine/Math/Types/Quaternion.h"

namespace App {

/// @brief 車移動の coordinator コンポーネント
///
/// 各サブコンポーネントへ処理を委譲する薄いコーディネーター。
/// 同一 Object に以下のコンポーネントをアタッチして機能を組み合わせる:
///
///   必須:
///     - VehicleGroundMover  … 地上前進・ステアリング・姿勢再構築
///     - VehicleAirController… 空中 roll/pitch 回転（慣性付き）
///
///   オプション（外すと機能が無効化される）:
///     - VehicleLandingAligner … 着地直後の Slerp 姿勢補正
///     - VehicleLandingBoost   … 着地時の速度ブースト
///     - VehicleDrift          … ドリフト処理
class VehicleMover final : public GameEngine::IObjectComponent {
public:
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "VehicleMover";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"車両移動", "Vehicle Mover"};

   /// @brief シーン保存とコンポーネント検索に使う型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 操舵・空中姿勢入力と接地状態から移動・姿勢を適用する
   /// @param steerInput  地上の左右操舵入力（-1〜+1）
   /// @param rollInput   空中の左右ロール入力（-1〜+1）
   /// @param pitchInput  前後入力（-1〜+1）: 空中のみ有効
   /// @param driftInput  ドリフトボタン入力（VehicleDrift コンポーネントへ転送）
   /// @param isGrounded  接地中なら true
   /// @param gravityUp   現在の重力Up方向
   /// @param deltaTime   フレーム時間
   void ApplyMovement(float steerInput, float rollInput, float pitchInput, bool driftInput, bool isGrounded,
      const GameEngine::Vector3& gravityUp, float deltaTime);

   /// @brief 直近の移動方向を取得する
   GameEngine::Vector3 GetLastMoveDirection() const { return lastMoveDirection_; }

   /// @brief 重力への姿勢追従速度を共有パラメータとして返す。
   nlohmann::json GetSharedParameters() const override;

   /// @brief 補間履歴を維持し、重力への姿勢追従速度を適用する。
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief 現在の設定と実行状態をインスペクターへ表示する。
   void DrawInspector() override;
#endif

   /// @brief コンポーネントの保存対象設定をJSONで返す。
   nlohmann::json Serialize() const override;

   /// @brief JSONから設定を復元する。
   /// @param data 保存されたコンポーネント設定。
   void Deserialize(const nlohmann::json& data) override;

private:
   /// @brief 重力基準の変化を現在姿勢へ反映し、車体へ書き込まず候補を返す。
   GameEngine::Quaternion AlignToGravity(
      const GameEngine::Quaternion& rotation, const GameEngine::Vector3& gravityUp, float deltaTime);

   float gravityAlignmentSpeed_ = 5.0f;                  ///< 地表法線の変化へ追従する速度。
   GameEngine::Vector3 orientationUp_{0.0f, 1.0f, 0.0f}; ///< 姿勢追従に使用した前回の上方向。

   /// @brief 着地遷移時に各サブコンポーネントへ通知する
   void OnLanded(const GameEngine::Quaternion& currentRotation, const GameEngine::Vector3& gravityUp);

   /// @brief VehicleLandingBoost へブースト試行を通知する
   void NotifyLandingBoost(const GameEngine::Vector3& localUp, const GameEngine::Vector3& gravityUp);

   /// @brief VehicleAirController の角速度をリセットする
   void ResetAirAngularVelocity();

   /// @brief VehicleLandingAligner へ補正開始を通知する
   void NotifyLandingAligner(const GameEngine::Quaternion& currentRotation, const GameEngine::Vector3& gravityUp);

   /// @brief gravityUp に合わせた着地目標クォータニオンを生成する
   GameEngine::Quaternion BuildAlignTargetRotation(
      const GameEngine::Quaternion& currentRotation, const GameEngine::Vector3& gravityUp) const;

   /// @brief 直近の移動方向
   GameEngine::Vector3 lastMoveDirection_ = {0.0f, 0.0f, 1.0f};

   /// @brief 前フレームの接地状態（着地遷移検出用）
   bool wasGrounded_ = true;
};

} // namespace App
