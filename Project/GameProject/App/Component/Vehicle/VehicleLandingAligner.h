#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include "GameEngine/Math/Types/Quaternion.h"

namespace App {

/// @brief 着地直後に Slerp で姿勢を gravityUp に合わせるコンポーネント
///
/// - VehicleMover が着地を検出したとき BeginAlign() を呼び出す
/// - タイマーが残っている間は VehicleMover が地上操舵より補間姿勢を優先する
/// - VehicleMover が Evaluate() を呼び、補間候補を受け取って適用する
class VehicleLandingAligner final : public GameEngine::IObjectComponent {
public:
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "VehicleLandingAligner";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"車両着地姿勢合わせ", "Vehicle Landing Aligner"};

   /// @brief シーン保存とコンポーネント検索に使う型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 毎フレーム Slerp 補正を進行させる
   /// @param rotation 補間中なら姿勢候補を受け取る。
   /// @return 有効な補間候補がある場合true。
   bool Evaluate(float deltaTime, GameEngine::Quaternion& rotation);

   /// @brief 着地補正を開始する
   /// @param startRot  着地した瞬間の回転
   /// @param targetRot gravityUp に合わせた目標回転
   void BeginAlign(const GameEngine::Quaternion& startRot, const GameEngine::Quaternion& targetRot);

   /// @brief 補正中かどうかを返す
   bool IsAligning() const { return alignTimer_ > 0.0f; }

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

   /// @brief 姿勢補正にかける時間（秒）
   float alignTime = 0.1f;

private:
   /// @brief 現在のタイマーから Slerp 済みクォータニオンを計算する
   GameEngine::Quaternion CalcBlendedRotation() const;

   float alignTimer_ = 0.0f;                                       ///< 着地姿勢補正を開始してからの経過時間（秒）。
   GameEngine::Quaternion alignStart_ = {0.0f, 0.0f, 0.0f, 1.0f};  ///< 着地姿勢補正を開始した時点の回転。
   GameEngine::Quaternion alignTarget_ = {0.0f, 0.0f, 0.0f, 1.0f}; ///< 着地姿勢補正で到達する目標回転。
};

} // namespace App
