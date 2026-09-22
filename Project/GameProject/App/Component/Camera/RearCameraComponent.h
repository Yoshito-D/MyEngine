#pragma once
#include "Scene/Camera/Core/ICinemachineComponent.h"
#include "RearCameraTypes.h"

namespace App {
class PlayerRearFollowCamera;

/// @brief 後方カメラ各部品の共通ライフサイクル。計算状態は各派生クラスが所有する。
class RearCameraComponent : public GameEngine::ICinemachineComponent {
public:
   /// @brief 所有カメラへ接続し、入力・設定部品が既にある場合はその設定で初期化する。
   void Initialize(GameEngine::VirtualCamera* owner) override;
   /// @brief 設定読込時に担当処理の補間履歴を初期化する。
   void Deserialize(const nlohmann::json& data) override;
   /// @brief 担当処理の補間履歴をリセットする。履歴を持たない表示部品では何もしない。
   virtual void Reset(const RearCameraSettings& settings) { (void)settings; }
#ifdef USE_IMGUI
   /// @brief 個別の有効状態をInspectorに表示する。
   void DrawInspector() override;
#endif

protected:
   /// @brief 入力・設定部品を解決する。無効・削除済み・必須部品不足ならnullptr。
   PlayerRearFollowCamera* GetRearCamera() const;
};

} // namespace App
