#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "GravityAttractor.h"
#include <vector>

namespace App {

/// @brief プレイヤー位置に応じて有効惑星を選択し、関連コンポーネントを切り替える
class PlanetSwitcher final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "PlanetSwitcher";

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 近傍惑星を選び、リンク先を更新する
   void Update(float deltaTime) override;

   /// @brief 惑星1件分の切替情報
   struct PlanetEntry {
	  std::string objectName; ///< オブジェクト名
	  GameEngine::Vector3 center = {};      ///< 惑星中心座標
	  float surfaceRadius = 15.0f;  ///< 地表半径
   };

   void AddPlanet(std::string objectName);;

   /// @brief 現在選択中の惑星インデックスを返す
   int GetCurrentPlanetIndex() const { return currentIndex_; }

   /// @brief 惑星切替に必要な距離差のヒステリシス（小さいほど敏感）
   float switchHysteresis = 1.5f;

   /// @brief 車体OBBの半サイズ（各軸を個別指定）
   /// Transformのscaleではなくこの値を距離判定に使用する
   GameEngine::Vector3 obbHalfExtents = { 0.35f, 0.3f, 0.75f };

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override {
	  nlohmann::json json;
	  // TODO: entries_ のシリアライズを追加する場合はここに記述
	  return json;
   }

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override {
	  (void)data;
	  // TODO: entries_ のデシリアライズを追加する場合はここに記述
   }

private:
   /// @brief 登録済み惑星候補
   std::vector<PlanetEntry> entries_;

   /// @brief 現在選択中インデックス
   int currentIndex_ = -1;

private:
   GravityAttractor* GetAttractorByObjectName(const std::string& objectName) const;

   GameEngine::Vector3 GetPlanetCenter(const std::string& objectName) const;

   float GetPlanetSurfaceRadius(const std::string& objectName) const;
};

} // namespace App
