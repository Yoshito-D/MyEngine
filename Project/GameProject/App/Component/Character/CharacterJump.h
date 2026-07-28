#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"

namespace App {

/// @brief 重力Up方向へジャンプ初速を付与するコンポーネント
class CharacterJump final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "CharacterJump";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "キャラクタージャンプ", "Character Jump" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 未ジャンプ時のみジャンプ速度を付与する
   /// @param gravityUp 現在の重力Up方向
   void Jump(const GameEngine::Vector3& gravityUp);

   /// @brief ジャンプ中かどうかを返す
   bool IsJumping()    const { return isJumping_; }

   /// @brief 着地通知を受けてジャンプ状態を解除する
   void NotifyLanded()       { isJumping_ = false; }

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

public:
   /// @brief ジャンプ初速の強さ
   float jumpStrength = 5.0f;

private:
#ifdef USE_IMGUI
   /// @brief 計測用に水平速度を持たない垂直落下状態を開始する
   void StartDebugVerticalDrop();

   /// @brief 垂直落下テスト開始時に現在位置から持ち上げる距離
   float debugVerticalDropHeight_ = 20.0f;

   /// @brief 垂直落下テスト開始時に与える下向き速度
   float debugVerticalDropInitialSpeed_ = 0.0f;
#endif

   /// @brief ジャンプ中フラグ
   bool isJumping_ = false;
};

} // namespace App
