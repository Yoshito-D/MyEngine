#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"

namespace App {

/// @brief 惑星表面への着地判定と接地固定を行うコンポーネント
///
/// - ジャンプ中: 落下して地表半径以下に到達したら着地処理
/// - 地上: 常に地表へ再配置し、速度を抑制
class CharacterLanding final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "CharacterLanding";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "キャラクター着地", "Character Landing" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 着地判定と接地状態を更新する
   void Update(float) override;

   /// @brief 惑星中心を設定する
   void SetPlanetCenter(const GameEngine::Vector3& center) { planetCenter_ = center; }

   /// @brief 地表半径を設定する
   void SetSurfaceRadius(float radius) { surfaceRadius_ = radius; }

   /// @brief 接地中かどうかを返す
   bool IsGrounded() const { return isGrounded_; }

   /// @brief 直近の着地時にプレイヤーと惑星が接触したワールド座標を返す
   const GameEngine::Vector3& GetLastLandingContactPoint() const { return lastLandingContactPoint_; }

   /// @brief 直近の着地面の外向き法線を返す
   const GameEngine::Vector3& GetLastLandingNormal() const { return lastLandingNormal_; }

   /// @brief 有効な着地接触情報を保持しているかを返す
   bool HasLandingContact() const { return hasLandingContact_; }

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

public:

   /// @brief 原点（inertia中心）から着地点までの追加オフセット
   /// surfaceRadius_ に加算され、実際のスナップ位置が決まる
   float landingOffset = 0.0f;

   /// @brief OBBの半サイズ（各軸を個別指定）
   /// 地表スナップ時の支持半径計算に使用する
   GameEngine::Vector3 obbHalfExtents = { 0.35f, 0.175f, 0.75f };

private:
   /// @brief 惑星地表の半径
   float surfaceRadius_ = 15.0f;

   /// @brief 惑星中心座標
   GameEngine::Vector3 planetCenter_ = { 0.0f, 0.0f, 0.0f };

   /// @brief 接地フラグ
   bool isGrounded_ = true;

   /// @brief 直近の着地接触点（ワールド座標）
   GameEngine::Vector3 lastLandingContactPoint_ = { 0.0f, 0.0f, 0.0f };

   /// @brief 直近の着地面法線（惑星外向き）
   GameEngine::Vector3 lastLandingNormal_ = { 0.0f, 1.0f, 0.0f };

   /// @brief 直近の着地接触情報が有効か
   bool hasLandingContact_ = false;
};

} // namespace App
