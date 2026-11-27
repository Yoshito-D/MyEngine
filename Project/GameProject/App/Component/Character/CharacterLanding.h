#pragma once

#include <cmath>
#include <algorithm>

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"

#include <optional>

namespace App {

/// @brief 車体が支持される地表上の点と外向き法線。
struct SurfaceContact {
   GameEngine::Vector3 point;  ///< ワールド座標（m）。
   GameEngine::Vector3 normal; ///< ワールド空間の単位法線。
};

/// @brief 車体が惑星表面に支持される状態を管理する。
/// @details 接地中は地表に沿って位置を補正し、空中では降下して車体が地表に達した場合に接地する。
class CharacterLanding final : public GameEngine::IObjectComponent {
public:
   /// @brief 依存する状態の更新後に処理するためのフェーズを返す。
   GameEngine::ObjectUpdatePhase GetUpdatePhase() const override { return GameEngine::ObjectUpdatePhase::Contacts; }

   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "CharacterLanding";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"キャラクター着地", "Character Landing"};

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 着地判定と接地状態を更新する
   void Update(float) override;

   /// @brief 有効な地表に接地しているかを返す。
   bool IsGrounded() const { return contact_.has_value(); }

   /// @brief このフレームの接触判定で空中から着地したかを返す。
   bool DidLandThisFrame() const { return landedThisFrame_; }

   /// @brief 接地点を返し、支持状態を解除する。空中では値を返さない。
   std::optional<SurfaceContact> Takeoff();

   /// @brief 無効にした接触判定の支持状態を破棄する。
   void OnDisable() override;

#ifdef USE_IMGUI

   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

   /// @brief シーン固有の参照と実行履歴を除く、共有可能な設定JSONを返す。
   nlohmann::json GetSharedParameters() const override;

   /// @brief 実行履歴を維持したまま共有設定を適用する。
   /// @param data 適用する共有設定JSON。
   void ApplySharedParameters(const nlohmann::json& data) override;

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {

      /// @brief 原点（inertia中心）から着地点までの追加オフセット
      /// 惑星の球半径に加算され、実際のスナップ位置が決まる。
      float landingOffset = 0.0f;

      /// @brief OBBの半サイズ（各軸を個別指定）
      /// メートルで指定し、惑星選択・接触・カメラの着地予測で共用する。
      GameEngine::Vector3 obbHalfExtents = {0.35f, 0.175f, 0.75f};
   };

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const;

   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested);

private:
   void ReadParameters(const nlohmann::json& data);

   Settings settings_;
   std::optional<SurfaceContact> contact_;
   bool evaluatedContact_ = false; ///< 初回配置時だけ、空中からの着地と区別して地表に接地させる。
   bool landedThisFrame_ = false;
};

} // namespace App
