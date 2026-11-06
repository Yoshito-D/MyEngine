#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"

namespace App {

/// @brief 着地判定の結果
enum class LandingResult {
   Success, ///< 成功: ブースト加算
   Normal,  ///< 普通: 速度変化なし
   Failure, ///< 失敗: 減速
};

/// @brief 着地直後の速度ブーストを担うコンポーネント（オプション）
///
/// - VehicleMover が着地を検出したとき TryBoost() を呼び出す
/// - 着地角度（alignment）に応じて3段階の判定を行う
///   - alignment >= boostThreshold  → 成功: boostAmount を加算
///   - alignment >= normalThreshold → 普通: 速度変化なし
///   - alignment <  normalThreshold → 失敗: penaltySpeed に減速し、一定時間加速を禁止
/// - このコンポーネントを Object から外すだけでブーストを無効化できる
class VehicleLandingBoost final : public GameEngine::IObjectComponent {
public:
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "VehicleLandingBoost";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"車両着地ブースト", "Vehicle Landing Boost"};

   /// @brief 保存とコンポーネント検索に使う型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 着地時に判定を行い速度を調整する
   /// @param localUp   着地瞬間の車体上方ベクトル
   /// @param gravityUp 現在の重力Up方向
   /// @return 着地判定結果
   LandingResult TryBoost(const GameEngine::Vector3& localUp, const GameEngine::Vector3& gravityUp);

   /// @brief 直近の着地判定結果を取得する
   LandingResult GetLastLandingResult() const { return lastLandingResult_; }

#ifdef USE_IMGUI

   /// @brief 設定と実行状態を単位付きで表示・編集する。
   void DrawInspector() override;
#endif

   /// @brief 設定値をプロジェクト共通の単位でJSONへ保存する。
   nlohmann::json Serialize() const override;

   /// @brief プロジェクト共通の単位で保存された設定JSONを読み込む。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief シーン固有の参照を除き、共有可能な設定をJSONで返す。
   nlohmann::json GetSharedParameters() const override;

   /// @brief 実行状態を維持したまま、統一単位の共有設定JSONを適用する。
   void ApplySharedParameters(const nlohmann::json& data) override;

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   /// @brief 着地ブーストの加算量（m/s）
   float boostAmount = 0.0f;

   /// @brief ブーストが発動する平行度の閾値（dot 積。1 が完全平行）
   float boostThreshold = 0.95f;

   /// @brief 普通と失敗の境界となる平行度の閾値
   float normalThreshold = /* 0.65f*/ 0.0f;

   /// @brief 失敗着地の速度
   float penaltySpeed = 0.0f;

   /// @brief 着地失敗後にミニターボと自動加速を禁止する時間（秒）
   float penaltyDuration = 1.0f;

private:
   /// @brief 直近の着地判定結果（エフェクトなどの後続処理で参照する）
   LandingResult lastLandingResult_ = LandingResult::Normal;
};

} // namespace App
