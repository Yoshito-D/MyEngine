#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include "GameEngine/Math/Types/Quaternion.h"
#include "GameProject/App/Component/Gravity/GravityAttractor.h"
#include <string>
#include <vector>

namespace GameEngine {
class Model;
}

namespace App {

/// @brief プレイヤー位置に応じて有効惑星と着地候補を選択する
class PlanetSwitcher final : public GameEngine::IObjectComponent {
public:
   /// @brief 依存する状態の更新後に処理するためのフェーズを返す。
   GameEngine::ObjectUpdatePhase GetUpdatePhase() const override {
      return GameEngine::ObjectUpdatePhase::TargetSelection;
   }

   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "PlanetSwitcher";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"惑星切り替え", "Planet Switcher"};

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 近傍惑星を選び、重力と着地の参照先を公開する
   void Update(float) override;

   /// @brief 惑星1件分の切替情報
   struct PlanetEntry {
      std::string objectId;            ///< 安定Entity ID
      GameEngine::Vector3 center = {}; ///< 惑星中心座標
      float surfaceRadius = 15.0f;     ///< 地表半径
   };

   /// @brief 切り替え候補の惑星を安定IDで追加する
   /// @param objectId 追加する惑星のEntity ID
   void AddPlanet(std::string objectId);

   /// @brief 候補を削除し、削除対象を指す選択・保留状態を解除する
   bool RemovePlanet(size_t index);

   /// @brief 候補を並べ替え、現在選択中の惑星との対応を維持する
   bool MovePlanet(size_t from, size_t to);

   /// @brief 参照設定の変更後に、指定ワールドから依存先を解決し直す。
   /// @param sceneWorld 参照先を検索するシーン。
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 現在選択中の惑星インデックスを返す
   std::int32_t GetCurrentPlanetIndex() const { return currentIndex_; }

   /// @brief 切り替え候補として登録された惑星数を返す
   /// @return 登録済みの惑星候補数
   std::int32_t GetPlanetCount() const { return static_cast<std::int32_t>(entries_.size()); }

   /// @brief 現在の重力適用先を返す。選択が無効な場合はnullptr。
   GravityAttractor* GetSelectedAttractor() const;

   /// @brief 着地済みの惑星の中心と半径を取得する。
   bool TryGetCurrentPlanet(GameEngine::Vector3& center, float& radius) const;

   /// @brief 着地済み惑星と異なる重力先へ空中で移動しているかを返す。
   bool HasPendingPlanetChange() const { return pendingIndex_ >= 0 && pendingIndex_ != currentIndex_; }

   /// @brief 着地判定に使う惑星情報を取得する
   /// @details 空中で保留中の候補があれば候補を、なければ現在の惑星を返す。
   /// @param outCenter 惑星中心の出力先
   /// @param outSurfaceRadius 地表半径の出力先
   /// @return 取得できた場合は true
   bool TryGetLandingPlanet(GameEngine::Vector3& outCenter, float& outSurfaceRadius) const;

   /// @brief 影の受け面モデルと、同じ着地候補の中心・半径を取得する。
   /// @param outReceiver 描画する惑星モデルの借用ポインター。失敗時はnullptr。
   /// @param outCenter 惑星中心のワールド座標。失敗時はゼロ。
   /// @param outSurfaceRadius 地表半径のワールド単位。失敗時はゼロ。
   /// @return 同じ候補からすべての情報を取得できた場合true。
   /// @note 実装時は既存の2引数版と同様にpendingIndex_を優先し、候補の安定IDからモデルを再解決する。
   /// モデルが消えていた場合はfalseとし、古いモデル参照を保存しない。
   bool TryGetLandingPlanet(
      GameEngine::Model*& outReceiver, GameEngine::Vector3& outCenter, float& outSurfaceRadius) const;

   /// @brief 空中で保留していた惑星切り替えを着地時に確定する
   void CommitPendingSwitch();

   /// @brief 惑星切替に必要な距離差のヒステリシス（小さいほど敏感）

   /// @brief 車体OBBの半サイズ（各軸を個別指定）
   /// Transformのscaleではなくこの値を距離判定に使用する

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

      /// @brief 惑星切替に必要な距離差のヒステリシス（小さいほど敏感）
      float switchHysteresis = 1.0f;

      /// @brief 車体OBBの半サイズ（各軸を個別指定）
      /// Transformのscaleではなくこの値を距離判定に使用する
      GameEngine::Vector3 obbHalfExtents = {0.35f, 0.175f, 0.75f};
   };

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const;

   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested);

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   float switchHysteresis = 1.0f; ///< 惑星切替に必要な距離差のヒステリシス（小さいほど敏感）。
   GameEngine::Vector3 obbHalfExtents = {0.35f, 0.175f, 0.75f}; ///< OBBの半サイズ（各軸を個別指定）。

private:
   /// @brief 登録済み惑星候補
   std::vector<PlanetEntry> entries_;
#ifdef USE_IMGUI
   std::string newPlanetId_; ///< 惑星リンクへ追加する候補の安定Entity ID。
#endif

   /// @brief 現在選択中インデックス
   std::int32_t currentIndex_ = -1;

   /// @brief 空中で見つけた着地候補。カメラなどの基準確定は着地まで保留する
   std::int32_t pendingIndex_ = -1;

   /// @brief 現在選択中の重力先の惑星インデックス
   std::int32_t selectedGravityIndex_ = -1;

private:
   std::int32_t SelectBestPlanetIndex(const GameEngine::Vector3& pos, const GameEngine::Quaternion& obbRot);

   bool IsOwnerAirborne() const;

   bool HasPlanet(const std::string& objectId) const;

   GravityAttractor* GetAttractorByObjectId(const std::string& objectId) const;

   GameEngine::Vector3 GetPlanetCenter(const std::string& objectId) const;

   float GetPlanetSurfaceRadius(const std::string& objectId) const;
};

} // namespace App
