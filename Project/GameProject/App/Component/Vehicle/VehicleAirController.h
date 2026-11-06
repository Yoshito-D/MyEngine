#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include "GameEngine/Math/Types/Quaternion.h"
#include "GameEngine/Math/Functions/MathConstants.h"

namespace App {

class GravityBody;

/// @brief 空中の roll / pitch 回転と慣性減衰を担うコンポーネント
///
/// - 入力による加速・反転は即時反映
/// - 入力を弱めるか離すと angularDamping に従って指数的に減衰
/// - 着地した瞬間に角速度をリセットする（VehicleMover から呼ばれる）
class VehicleAirController final : public GameEngine::IObjectComponent {
public:
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "VehicleAirController";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"車両空中制御", "Vehicle Air Controller"};

   /// @brief シーン保存とコンポーネント検索に使う型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 空中の慣性と抵抗を更新し、回転の候補を返す
   /// @param rollInput   左右ロール入力（-1〜+1）
   /// @param pitchInput  前後入力（-1〜+1）
   /// @param deltaTime   フレーム時間
   /// @param currentRot 計算の基準となる現在姿勢。
   /// @return 入力反映後の姿勢。無効時は入力姿勢を返す。
   GameEngine::Quaternion Apply(
      float rollInput, float pitchInput, float deltaTime, const GameEngine::Quaternion& currentRot);

   /// @brief 着地時に角速度をリセットする
   void ResetAngularVelocity();

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

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   /// @brief ロール角速度（rad/s）
   float rollSpeed = 285.0f * GameEngine::MathConstants::kDegreesToRadians;

   /// @brief ピッチ角速度（rad/s）
   float pitchSpeed = 285.0f * GameEngine::MathConstants::kDegreesToRadians;

   /// @brief 空中回転の減衰係数（大きいほど慣性が少ない）
   float angularDamping = 2.5f;

   /// @brief 空中の水平減速係数（1/s）
   /// 小さい値にするほど空中での減速が緩やかになる
   float airDrag = 0.001f;

private:
   /// @brief アナログ入力を考慮して角速度の加速・反転・慣性減衰を更新する
   void UpdateAngularVelocity(float input, float& angVel, float targetVel, float deltaTime) const;

   /// @brief pitch 角速度を回転クォータニオンへ反映する
   void ApplyPitchRotation(GameEngine::Quaternion& rot, const GameEngine::Vector3& localRight, float deltaTime) const;

   /// @brief roll 角速度を回転クォータニオンへ反映する
   void ApplyRollRotation(GameEngine::Quaternion& rot, const GameEngine::Vector3& localForward, float deltaTime) const;

   float angularVelRoll_ = 0.0f;  ///< 空中ロール操作の現在の角速度。
   float angularVelPitch_ = 0.0f; ///< 空中ピッチ操作の現在の角速度。
};

} // namespace App
