#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "Utility/Math/Quaternion.h"
#include "GravityAttractor.h"
#include <string>
#include <vector>

namespace App {

/// @brief プレイヤー位置に応じて有効惑星を選択し、関連コンポーネントを切り替える
class PlanetSwitcher final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "PlanetSwitcher";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "惑星切り替え", "Planet Switcher" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 近傍惑星を選び、リンク先を更新する
   void Update(float) override;

   /// @brief 惑星1件分の切替情報
   struct PlanetEntry {
	  std::string objectName; ///< 安定Entity ID（旧データの名前は参照解決時に移行）
	  GameEngine::Vector3 center = {};      ///< 惑星中心座標
	  float surfaceRadius = 15.0f;  ///< 地表半径
   };

   /// @brief 切り替え候補の惑星を安定IDまたは旧オブジェクト名で追加する
   /// @param objectName 追加する惑星のEntity IDまたは表示名
   void AddPlanet(std::string objectName);
   /// @brief 候補を削除し、削除対象を指す選択・保留状態を解除する
   bool RemovePlanet(size_t index);
   /// @brief 候補を並べ替え、現在選択中の惑星との対応を維持する
   bool MovePlanet(size_t from, size_t to);
   /// @copydoc GameEngine::IObjectComponent::OnReferencesChanged
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 現在選択中の惑星インデックスを返す
   int GetCurrentPlanetIndex() const { return currentIndex_; }

   /// @brief 切り替え候補として登録された惑星数を返す
   /// @return 登録済みの惑星候補数
   int GetPlanetCount() const { return static_cast<int>(entries_.size()); }

   /// @brief 直近フレームで惑星が切り替わったかどうかを返す
   bool HasSwitched() const { return switched_; }

   /// @brief 惑星切り替え済みフラグを解除する
   void ResetSwitchedFlag() { switched_ = false; }

   /// @brief 着地判定に使う惑星情報を取得する
   /// @details 空中で保留中の候補があれば候補を、なければ現在の惑星を返す。
   /// @param outCenter 惑星中心の出力先
   /// @param outSurfaceRadius 地表半径の出力先
   /// @return 取得できた場合は true
   bool TryGetLandingPlanet(GameEngine::Vector3& outCenter, float& outSurfaceRadius) const;

   /// @brief 空中で保留していた惑星切り替えを着地時に確定する
   void CommitPendingSwitch();

   /// @brief 惑星切替に必要な距離差のヒステリシス（小さいほど敏感）
   float switchHysteresis = 1.0f;

   /// @brief 車体OBBの半サイズ（各軸を個別指定）
   /// Transformのscaleではなくこの値を距離判定に使用する
   GameEngine::Vector3 obbHalfExtents = { 0.35f, 0.175f, 0.75f };

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

private:
   /// @brief 登録済み惑星候補
   std::vector<PlanetEntry> entries_;
#ifdef USE_IMGUI
   std::string newPlanetId_;
#endif

   /// @brief 現在選択中インデックス
   int currentIndex_ = -1;

   /// @brief 空中で見つけた着地候補。カメラなどの基準確定は着地まで保留する
   int pendingIndex_ = -1;

   /// @brief 現在 GravityAttractorLink に接続している惑星インデックス
   int activeGravityIndex_ = -1;

   // 惑星を切り換えたか
   bool switched_ = false;

private:
   int SelectBestPlanetIndex(const GameEngine::Vector3& pos,
	  const GameEngine::Quaternion& obbRot);

   void ApplyPlanetIndex(int newIndex);

   void ApplyAirborneAttractorIndex(int newIndex, const GameEngine::Vector3& pos);

   bool IsOwnerAirborne() const;

   void ApplyCurrentPlanetParameters();

   bool HasPlanet(const std::string& objectName) const;

   GravityAttractor* GetAttractorByObjectName(const std::string& objectName) const;

   GameEngine::Vector3 GetPlanetCenter(const std::string& objectName) const;

   float GetPlanetSurfaceRadius(const std::string& objectName) const;
};

} // namespace App
