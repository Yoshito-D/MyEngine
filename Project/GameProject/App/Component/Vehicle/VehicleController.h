#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <unordered_set>

namespace App {

/// @brief 入力を収集して車の移動・ジャンプへ振り分けるコンポーネント
class VehicleController final : public GameEngine::IObjectComponent {
public:
   /// @brief 依存する状態の更新後に処理するためのフェーズを返す。
   GameEngine::ObjectUpdatePhase GetUpdatePhase() const override { return GameEngine::ObjectUpdatePhase::Control; }

   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "VehicleController";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"車両制御", "Vehicle Controller"};

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 入力取得と各サブコンポーネントへの委譲を行う
   void Update(float deltaTime) override;

   /// @brief 呼び出し元の要求だけを追加・解除し、すべての要求が解除されるまで走行を停止する。
   /// @param requester 解除まで生存する要求元のアドレス。参照先の読み書きには使わない。
   /// @param stopped 要求を追加する場合はtrue。
   void SetMotionStopped(const void* requester, bool stopped);

   /// @brief 操作と物理の一時停止を要求する所有者がいるかを返す。
   bool IsMotionStopped() const { return !stopRequests_.empty(); }

#ifdef USE_IMGUI

   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

private:
   std::unordered_set<const void*> stopRequests_;
};

} // namespace App
