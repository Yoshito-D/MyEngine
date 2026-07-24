#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector2.h"
#include <cstdint>

namespace GameEngine {
struct InputActionState;
}

namespace App {

/// @brief 車両と追従カメラが利用するゲーム固有入力を提供するコンポーネント
/// @details 物理キーやパッド番号を制御ロジックから隠し、アクションIDだけをこの境界で管理する。
class VehicleInputComponent final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "VehicleInputComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "車両入力", "Vehicle Input" };

   /// @brief コンポーネント型名を取得する
   /// @return VehicleInputComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 左右の操舵入力を取得する
   /// @return -1.0から1.0の操舵量
   float GetSteerInput() const;

   /// @brief 空中での前後姿勢入力を取得する
   /// @return -1.0から1.0の姿勢入力
   float GetPitchInput() const;

   /// @brief 空中での左右ロール入力を取得する
   /// @return -1.0から1.0のロール入力
   float GetRollInput() const;

   /// @brief カメラ視点入力を取得する
   /// @return Xが左右、Yが上下の入力
   GameEngine::Vector2 GetCameraLookInput() const;

   /// @brief ジャンプがこのフレームに押されたかを取得する
   /// @return 押された瞬間ならtrue
   bool IsJumpTriggered() const;

   /// @brief ドリフト入力が押されているかを取得する
   /// @return 押されている間はtrue
   bool IsDriftHeld() const;

   /// @brief 次のカメラへ切り替える入力が押されたかを取得する
   /// @return 押された瞬間ならtrue
   bool IsNextCameraTriggered() const;

   /// @brief プレイヤースロットを保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief プレイヤースロットを復元する
   /// @param data 入力コンポーネント設定
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief 入力対象のプレイヤースロットを編集する
   void DrawInspector() override;
#endif

public:
   /// @brief 入力を読み取るプレイヤースロット
   uint32_t playerSlot = 0;

private:
   const GameEngine::InputActionState& GetActionState(const char* actionId) const;
};

} // namespace App
