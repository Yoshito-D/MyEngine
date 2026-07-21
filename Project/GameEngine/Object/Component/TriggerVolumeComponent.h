#pragma once

#include "IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include <string>
#include <utility>

namespace GameEngine {

class Object;
class SceneWorld;

/// @brief 指定オブジェクトの侵入・滞在・退出を検出する非物理トリガー
class TriggerVolumeComponent final : public IObjectComponent {
public:
   /// @brief 対応するトリガー形状
   enum class Shape {
      Sphere,
      Aabb,
   };

   static constexpr const char* kTypeName = "TriggerVolumeComponent";
   static constexpr ComponentDisplayName kDisplayName{ "トリガーボリューム", "Trigger Volume" };

   /// @brief コンポーネント型名を取得する
   /// @return TriggerVolumeComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief シーン参照を保存して対象IDを解決可能にする
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(SceneWorld& sceneWorld) override;

   /// @brief 対象オブジェクトとの重なり状態を更新する
   /// @param deltaTime 未使用
   void Update(float deltaTime) override;

   /// @brief 今フレームにトリガーへ侵入したか取得する
   /// @return 侵入したフレームだけtrue
   bool WasEnteredThisFrame() const { return enteredThisFrame_; }

   /// @brief 今フレームにトリガーから退出したか取得する
   /// @return 退出したフレームだけtrue
   bool WasExitedThisFrame() const { return exitedThisFrame_; }

   /// @brief 現在トリガー内にいるか取得する
   /// @return 重なっている場合はtrue
   bool IsInside() const { return isInside_; }

   /// @brief 重なり判定の対象オブジェクトIDを設定する
   /// @param targetObjectId シーンJSON上の安定したオブジェクトID
   void SetTargetObjectId(std::string targetObjectId) { targetObjectId_ = std::move(targetObjectId); }

   /// @brief 重なり判定の対象オブジェクトIDを取得する
   /// @return シーンJSON上の対象オブジェクトID
   const std::string& GetTargetObjectId() const { return targetObjectId_; }

   /// @brief トリガー設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONからトリガー設定を読み込む
   /// @param data トリガー設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief トリガー設定をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   Object* ResolveTarget() const;
   bool CalculateOverlap(const Object& target) const;

   SceneWorld* sceneWorld_ = nullptr;
   std::string targetObjectId_;
   Shape shape_ = Shape::Aabb;
   Vector3 centerOffset_{};
   Vector3 halfExtents_ = { 1.0f, 1.0f, 1.0f };
   float radius_ = 1.0f;
   bool debugDraw_ = false;
   bool isInside_ = false;
   bool enteredThisFrame_ = false;
   bool exitedThisFrame_ = false;
};

} // namespace GameEngine
