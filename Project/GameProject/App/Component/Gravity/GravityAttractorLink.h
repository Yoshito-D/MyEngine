#pragma once

#include "Object/Component/IObjectComponent.h"
#include "GravityAttractor.h"
#include <string>

namespace App {

/// @brief 指定した GravityAttractor を同オーナーの GravityBody へ橋渡しするコンポーネント
class GravityAttractorLink final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "GravityAttractorLink";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "重力アトラクターリンク", "Gravity Attractor Link" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 現在の attractor を GravityBody へ適用する
   void Update(float) override;

   /// @brief 適用元となる重力発生源を設定する
   /// @details 非所有ポインターは保持せず、安定IDと型名から毎フレーム安全に再解決する。
   void SetAttractor(GravityAttractor* attractor);

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief シリアライズ（参照のみのため保存項目なし）
   nlohmann::json Serialize() const override  { return {}; }

   /// @brief デシリアライズ（保存項目なしのため処理なし）
   void Deserialize(const nlohmann::json&) override {}

private:
   /// @brief 現在接続中の重力発生源を再解決する
   GravityAttractor* ResolveAttractor() const;

   /// @brief 接続先オブジェクトの安定ID（旧データ等で空の場合は名前を使用）
   std::string attractorEntityId_;

   /// @brief 接続先オブジェクト名
   std::string attractorObjectName_;

   /// @brief 接続先コンポーネント型名
   std::string attractorTypeName_;
};

} // namespace App
