#pragma once

#include "Object/Component/IObjectComponent.h"
#include <cstddef>
#include <string>
#include <vector>

namespace GameEngine {
class VirtualCamera;
}

namespace App {

/// @brief 入力に応じてJSON定義された仮想カメラの優先度を切り替える
class CameraModeSwitcher final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "CameraModeSwitcher";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "カメラモード切替", "Camera Mode Switcher" };

   /// @brief コンポーネント型名を取得する
   /// @return CameraModeSwitcher
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief JSONのカメラIDを実体へ解決して初期モードを適用する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief Tab入力で次のカメラへ切り替える
   /// @param deltaTime 未使用
   void Update(float deltaTime) override;

   /// @brief JSONで登録されたIDを指定してカメラを切り替える
   /// @param cameraId 切り替え先の仮想カメラIDまたは名前
   /// @return カメラが見つかり切り替えられた場合はtrue
   bool SwitchToCamera(const std::string& cameraId);

   /// @brief カメラ順序と初期モードをJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONからカメラ順序と初期モードを読み込む
   /// @param data カメラ切り替え設定
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief 現在のカメラモードを表示する
   void DrawInspector() override;
#endif

private:
   void ApplyMode();

   std::vector<std::string> cameraIds_;
   std::vector<GameEngine::VirtualCamera*> cameras_;
   size_t initialIndex_ = 0;
   size_t currentIndex_ = 0;
};

} // namespace App
